#include "entrance/runtime.h"
#include "entrance/core.h"
#include "entrance/mfrc522.h"
#include "trevor/runtime.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/apps/sntp.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define ENTRY_DOOR_PIN 14
#define EXIT_DOOR_PIN 15
#define ENTRY_DENY_PIN 16
#define EXIT_DENY_PIN 17
#define WIFI_CONNECTED_BIT BIT0
#define DOOR_OPEN_MS 900u

static const char *TAG="entrance";
static entrance_state state;
static SemaphoreHandle_t state_mutex;
static EventGroupHandle_t wifi_events;
static bool started;
static volatile bool clock_ready;
static uint64_t door_deadline[2];
static uint64_t deny_deadline[2];
static const int grant_pin[2]={ENTRY_DOOR_PIN,EXIT_DOOR_PIN};
static const int deny_pin[2]={ENTRY_DENY_PIN,EXIT_DENY_PIN};

static void wifi_handler(void *arg, esp_event_base_t base,int32_t id,void *data) {
    (void)arg;(void)data;
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_START) esp_wifi_connect();
    else if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_events,WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    } else if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) xEventGroupSetBits(wifi_events,WIFI_CONNECTED_BIT);
}

static void clock_task(void *arg) {
    (void)arg;
    esp_err_t e=esp_netif_init();
    if(e!=ESP_OK && e!=ESP_ERR_INVALID_STATE) { ESP_LOGE(TAG,"network init: %s",esp_err_to_name(e));vTaskDelete(NULL); }
    e=esp_event_loop_create_default();
    if(e!=ESP_OK && e!=ESP_ERR_INVALID_STATE) { ESP_LOGE(TAG,"event loop: %s",esp_err_to_name(e));vTaskDelete(NULL); }
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_handler,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_handler,NULL));
    wifi_events=xEventGroupCreate();
    if(!wifi_events) { ESP_LOGE(TAG,"could not allocate Wi-Fi event group");vTaskDelete(NULL); }
    wifi_config_t config={.sta={.ssid="Wokwi-GUEST",.password="",.threshold.authmode=WIFI_AUTH_OPEN}};
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&config));
    ESP_ERROR_CHECK(esp_wifi_start());
    EventBits_t bits;
    while(true) {
        bits=xEventGroupWaitBits(wifi_events,WIFI_CONNECTED_BIT,pdFALSE,pdFALSE,pdMS_TO_TICKS(20000));
        if(bits&WIFI_CONNECTED_BIT)break;
        ESP_LOGW(TAG,"waiting for Wokwi-GUEST; entry remains disabled until time is synchronized");
    }
    setenv("TZ","EAT-3",1);tzset();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0,"pool.ntp.org");
    sntp_init();
    while(true) {
        time_t now=time(NULL);
        if(now>1760000000 && !clock_ready) {
            clock_ready=true;
            ESP_LOGI(TAG,"SNTP synchronized; timezone Africa/Kampala (UTC+03:00)");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void save_event(system_access_event *event) {
    /* Stable compact encoding: no compiler padding, UID bytes are the only credential material logged. */
    uint8_t bytes[40]={0};
    bytes[0]=(uint8_t)(event->event_id);bytes[1]=(uint8_t)(event->event_id>>8);
    bytes[2]=(uint8_t)(event->event_id>>16);bytes[3]=(uint8_t)(event->event_id>>24);
    bytes[4]=(uint8_t)(event->user_id);bytes[5]=(uint8_t)(event->user_id>>8);
    bytes[6]=(uint8_t)(event->user_id>>16);bytes[7]=(uint8_t)(event->user_id>>24);
    for(unsigned i=0;i<8;i++) { bytes[8+i]=(uint8_t)(event->utc_epoch_seconds>>(8*i));bytes[16+i]=(uint8_t)(event->monotonic_ms>>(8*i)); }
    bytes[24]=(uint8_t)event->point;bytes[25]=(uint8_t)event->result;bytes[26]=event->uid_size;
    memcpy(bytes+27,event->uid,event->uid_size);
    if(!tr_runtime_record_access(bytes,27u+event->uid_size)) ESP_LOGW(TAG,"storage queue full; event %" PRIu32 " not queued",event->event_id);
}

static void schedule_watch_task(void *arg) {
    (void)arg;
    bool previous_open=false, initialized=false;
    while(true) {
        if(clock_ready) {
            time_t utc=time(NULL);struct tm local={0};
            if(utc>1760000000 && localtime_r(&utc,&local)) {
                system_access_schedule schedule;
                xSemaphoreTake(state_mutex,portMAX_DELAY);schedule=state.schedule;xSemaphoreGive(state_mutex);
                bool is_open=entrance_is_open(schedule,(unsigned)(local.tm_hour*60+local.tm_min));
                if(initialized && previous_open && !is_open) {
                    system_user users[SYSTEM_MAX_USERS];bool inside[SYSTEM_MAX_USERS];
                    size_t user_count=0,inside_count=0;
                    if(entrance_runtime_snapshot(users,inside,SYSTEM_MAX_USERS,&user_count,&inside_count,NULL)) {
                        if(inside_count) {
                            ESP_LOGW(TAG,"ACCESS WINDOW CLOSED with %u person(s) still inside; exit remains enabled",(unsigned)inside_count);
                            for(size_t i=0;i<SYSTEM_MAX_USERS;i++) if(inside[i])
                                ESP_LOGW(TAG,"still inside: %s (user %u)",users[i].display_name,(unsigned)i);
                        } else ESP_LOGI(TAG,"access window closed; building occupancy is zero");
                    }
                }
                previous_open=is_open;initialized=true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

static void process_scan(uint8_t reader,const uint8_t *uid,uint8_t uid_size) {
    uint64_t mono=(uint64_t)esp_timer_get_time()/1000u;
    time_t utc=time(NULL);struct tm local={0};bool valid=clock_ready && utc>1760000000;
    if(valid)localtime_r(&utc,&local);
    system_access_point point=reader==0?SYSTEM_ENTRY:SYSTEM_EXIT;
    uint32_t user_id,event_id;
    xSemaphoreTake(state_mutex,portMAX_DELAY);
    system_access_result result=entrance_apply_scan(&state,point,uid,uid_size,valid,
        valid?(unsigned)(local.tm_hour*60+local.tm_min):0,&user_id,&event_id);
    system_access_event event={.event_id=event_id,.user_id=user_id,.utc_epoch_seconds=valid?(uint64_t)utc:0,
        .monotonic_ms=mono,.point=point,.result=result,.uid_size=uid_size};
    memcpy(event.uid,uid,uid_size);
    size_t inside_count=entrance_inside_count(&state);
    const char *name=user_id<SYSTEM_MAX_USERS?state.users[user_id].display_name:"unknown";
    bool granted=result==SYSTEM_ACCESS_GRANTED;
    door_deadline[reader]=granted?mono+DOOR_OPEN_MS:0;
    deny_deadline[reader]=granted?0:mono+DOOR_OPEN_MS;
    xSemaphoreGive(state_mutex);
    gpio_set_level(grant_pin[reader],granted?1:0);
    gpio_set_level(deny_pin[reader],granted?0:1);
    ESP_LOGI(TAG,"event=%" PRIu32 " utc=%" PRIu64 " point=%s user=%s uid=%02X%02X%02X%02X result=%s inside=%u%s",
        event_id,event.utc_epoch_seconds,point==SYSTEM_ENTRY?"entry":"exit",name,uid[0],uid_size>1?uid[1]:0,
        uid_size>2?uid[2]:0,uid_size>3?uid[3]:0,granted?"GRANTED":"DENIED",(unsigned)inside_count,
        result==SYSTEM_ACCESS_SCHEDULE_CLOSED?" (schedule closed)":
        result==SYSTEM_ACCESS_CLOCK_UNAVAILABLE?" (clock unavailable)":
        result==SYSTEM_ACCESS_ALREADY_INSIDE?" (already inside)":
        result==SYSTEM_ACCESS_NOT_INSIDE?" (not inside)":
        result==SYSTEM_ACCESS_UNKNOWN_USER?" (unknown user)":"");
    save_event(&event);
}

static void entrance_task(void *arg) {
    (void)arg;
    uint8_t last_uid[2][SYSTEM_UID_MAX_BYTES]={{0}},last_size[2]={0};
    uint64_t last_seen[2]={0};
    while(true) {
        uint64_t now=(uint64_t)esp_timer_get_time()/1000u;
        for(uint8_t reader=0;reader<2;reader++) {
            uint8_t uid[SYSTEM_UID_MAX_BYTES],size=0;
            if(mfrc522_poll(reader,uid,&size)) {
                bool duplicate=last_size[reader]==size && memcmp(last_uid[reader],uid,size)==0 && now-last_seen[reader]<1500u;
                last_seen[reader]=now;
                if(!duplicate) { memcpy(last_uid[reader],uid,size);last_size[reader]=size;process_scan(reader,uid,size); }
            } else if(now-last_seen[reader]>500u) last_size[reader]=0;
            if(door_deadline[reader] && now>=door_deadline[reader]) {
                gpio_set_level(grant_pin[reader],0);door_deadline[reader]=0;
            }
            if(deny_deadline[reader] && now>=deny_deadline[reader]) {
                gpio_set_level(deny_pin[reader],0);deny_deadline[reader]=0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t entrance_runtime_start(void) {
    if(started)return ESP_ERR_INVALID_STATE;
    state_mutex=xSemaphoreCreateMutex();if(!state_mutex)return ESP_ERR_NO_MEM;
    entrance_state_init(&state);
    gpio_config_t outputs={.pin_bit_mask=(1ULL<<ENTRY_DOOR_PIN)|(1ULL<<EXIT_DOOR_PIN)|
            (1ULL<<ENTRY_DENY_PIN)|(1ULL<<EXIT_DENY_PIN),
        .mode=GPIO_MODE_OUTPUT,.intr_type=GPIO_INTR_DISABLE};
    esp_err_t e=gpio_config(&outputs);if(e!=ESP_OK)return e;
    for(unsigned i=0;i<2;i++) { gpio_set_level(grant_pin[i],0);gpio_set_level(deny_pin[i],0); }
    e=mfrc522_init();if(e!=ESP_OK)return e;
    if(xTaskCreate(clock_task,"entrance_clock",4096,NULL,3,NULL)!=pdPASS)return ESP_ERR_NO_MEM;
    if(xTaskCreate(entrance_task,"entrance_rfid",4096,NULL,4,NULL)!=pdPASS)return ESP_ERR_NO_MEM;
    if(xTaskCreate(schedule_watch_task,"entrance_schedule",4096,NULL,2,NULL)!=pdPASS)return ESP_ERR_NO_MEM;
    started=true;
    ESP_LOGI(TAG,"entry/exit RFID ready; demo UIDs DE:AD:BE:EF and 11:22:33:44; default window 08:00-18:00 Kampala time");
    return ESP_OK;
}

bool entrance_runtime_snapshot(system_user *users,bool *inside,size_t capacity,size_t *user_count,
    size_t *inside_count,system_access_schedule *schedule) {
    if(!started || !user_count || !inside_count || capacity<SYSTEM_MAX_USERS || !users || !inside)return false;
    xSemaphoreTake(state_mutex,portMAX_DELAY);
    memcpy(users,state.users,sizeof(state.users));memcpy(inside,state.inside,sizeof(state.inside));
    *user_count=0;*inside_count=0;
    for(size_t i=0;i<SYSTEM_MAX_USERS;i++) { if(state.users[i].enabled)(*user_count)++;if(state.inside[i])(*inside_count)++; }
    if(schedule)*schedule=state.schedule;
    xSemaphoreGive(state_mutex);return true;
}

esp_err_t entrance_runtime_set_schedule(system_access_schedule schedule) {
    if(!started)return ESP_ERR_INVALID_STATE;
    if(!entrance_schedule_valid(schedule))return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(state_mutex,portMAX_DELAY);state.schedule=schedule;xSemaphoreGive(state_mutex);
    ESP_LOGI(TAG,"access schedule changed to %02u:%02u-%02u:%02u (requires authenticated admin CLI)",
        schedule.start_minute/60,schedule.start_minute%60,schedule.end_minute/60,schedule.end_minute%60);
    return ESP_OK;
}
