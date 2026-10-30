#include "entrance/keypad.h"
#include "entrance/runtime.h"
#include "entrance/core.h"
#include "auth/runtime.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const int rows[4]={14,15,16,17};
static const int cols[4]={34,35,36,39};
static const char keys[4][4]={{'1','2','3','A'},{'4','5','6','B'},
    {'7','8','9','C'},{'*','0','#','D'}};
enum ui_mode { UI_PUBLIC,UI_PIN,UI_MENU,UI_OPEN,UI_CLOSE,UI_MESSAGE };
static enum ui_mode mode=UI_PUBLIC;
static SemaphoreHandle_t ui_mutex;
static char input[9];
static size_t input_size;
static char message[4][21];
static int64_t session_until;
static int64_t message_until;
static bool started;
static const char *TAG="entrance_keypad";

static void screen_set(const char *a,const char *b,const char *c,const char *d) {
    memset(message,' ',sizeof(message));
    snprintf(message[0],21,"%s",a?a:"");
    snprintf(message[1],21,"%s",b?b:"");
    snprintf(message[2],21,"%s",c?c:"");
    snprintf(message[3],21,"%s",d?d:"");
}

static char scan_key(void) {
    char found=0;
    for(unsigned r=0;r<4;r++) {
        for(unsigned i=0;i<4;i++)gpio_set_level(rows[i],1);
        gpio_set_level(rows[r],0);
        esp_rom_delay_us(100);
        for(unsigned c=0;c<4;c++)if(gpio_get_level(cols[c])==0)found=keys[r][c];
        if(found)break;
    }
    for(unsigned i=0;i<4;i++)gpio_set_level(rows[i],1);
    return found;
}

static void show_message(const char *a,const char *b,const char *c,const char *d) {
    xSemaphoreTake(ui_mutex,portMAX_DELAY);
    mode=UI_MESSAGE;message_until=esp_timer_get_time()/1000+1800;
    screen_set(a,b,c,d);
    xSemaphoreGive(ui_mutex);
}

static void begin_time_entry(enum ui_mode target) {
    xSemaphoreTake(ui_mutex,portMAX_DELAY);
    mode=target;input_size=0;input[0]=0;session_until=esp_timer_get_time()/1000+60000;
    screen_set(target==UI_OPEN?"Set OPEN time":"Set CLOSE time","Enter HHMM then #","* clears input","D cancels");
    xSemaphoreGive(ui_mutex);
}

static void handle_key(char key) {
    int64_t now=esp_timer_get_time()/1000;
    xSemaphoreTake(ui_mutex,portMAX_DELAY);
    if(mode==UI_PUBLIC) {
        if(key=='A') {
            mode=UI_PIN;input_size=0;input[0]=0;
            screen_set("ADMIN LOGIN","PIN: ____","# confirms  * clears","D cancels");
            xSemaphoreGive(ui_mutex);return;
        }
        if(key=='B') {
            mode=UI_MESSAGE;message_until=now+2500;
            screen_set("SIMULATED EVENT","TAILGATE ALERT","Occupancy uncertain","Check serial log");
            xSemaphoreGive(ui_mutex);
            entrance_runtime_note_simulated_tailgate();
            return;
        }
        if(key=='C') {
            mode=UI_MESSAGE;message_until=now+2500;
            screen_set("ROLL-CALL SENT","See USB serial log","Report is read-only","");
            xSemaphoreGive(ui_mutex);
            entrance_runtime_print_roll_call(false);
            return;
        }
        xSemaphoreGive(ui_mutex);return;
    }
    if(mode==UI_MESSAGE) { mode=UI_PUBLIC; }
    if(mode==UI_PIN) {
        if(key=='D') {mode=UI_PUBLIC;xSemaphoreGive(ui_mutex);return;}
        if(key=='*') {input_size=0;input[0]=0;}
        else if(key>='0'&&key<='9'&&input_size<4) {input[input_size++]=key;input[input_size]=0;}
        else if(key=='#') {
            char pin[sizeof(input)];snprintf(pin,sizeof(pin),"%s",input);
            input_size=0;input[0]=0;
            xSemaphoreGive(ui_mutex);
            bool ok=auth_runtime_verify_pin(pin);
            memset(pin,0,sizeof(pin));
            xSemaphoreTake(ui_mutex,portMAX_DELAY);
            if(ok) {
                mode=UI_MENU;session_until=now+60000;
                screen_set("ADMIN AUTHORIZED","1 Set OPEN time","2 Set CLOSE time","D Logout");
            } else {
                mode=UI_MESSAGE;message_until=now+2000;
                screen_set("ACCESS DENIED","Bad PIN / locked","Wait, then try again","");
            }
            xSemaphoreGive(ui_mutex);return;
        }
        char masked[8]="PIN: ";
        for(size_t i=0;i<input_size;i++)strcat(masked,"*");
        screen_set("ADMIN LOGIN",masked,"# confirms  * clears","D cancels");
        xSemaphoreGive(ui_mutex);return;
    }
    if(mode==UI_MENU) {
        session_until=now+60000;
        if(key=='1') {xSemaphoreGive(ui_mutex);begin_time_entry(UI_OPEN);return;}
        if(key=='2') {xSemaphoreGive(ui_mutex);begin_time_entry(UI_CLOSE);return;}
        if(key=='D'||key=='*') {mode=UI_PUBLIC;xSemaphoreGive(ui_mutex);return;}
        screen_set("ADMIN AUTHORIZED","1 Set OPEN time","2 Set CLOSE time","D Logout");
        xSemaphoreGive(ui_mutex);return;
    }
    if(mode==UI_OPEN||mode==UI_CLOSE) {
        session_until=now+60000;
        if(key=='D') {mode=UI_MENU;screen_set("ADMIN AUTHORIZED","1 Set OPEN time","2 Set CLOSE time","D Logout");xSemaphoreGive(ui_mutex);return;}
        if(key=='*') {input_size=0;input[0]=0;}
        else if(key>='0'&&key<='9'&&input_size<4) {input[input_size++]=key;input[input_size]=0;}
        else if(key=='#') {
            if(input_size!=4) {
                mode=UI_MESSAGE;message_until=now+1800;screen_set("ENTER 4 DIGITS","Use HHMM format","","");
                xSemaphoreGive(ui_mutex);return;
            }
            char digits[5];memcpy(digits,input,5);
            enum ui_mode target=mode;mode=UI_MENU;input_size=0;input[0]=0;
            session_until=now+60000;
            xSemaphoreGive(ui_mutex);
            entrance_status s;
            unsigned hour=(unsigned)(digits[0]-'0')*10u+(unsigned)(digits[1]-'0');
            unsigned minute=(unsigned)(digits[2]-'0')*10u+(unsigned)(digits[3]-'0');
            bool good=hour<24&&minute<60&&entrance_runtime_status(&s);
            if(good) {
                system_access_schedule next=s.schedule;
                uint16_t value=(uint16_t)(hour*60u+minute);
                if(target==UI_OPEN)next.start_minute=value;else next.end_minute=value;
                good=entrance_schedule_valid(next)&&entrance_runtime_set_schedule(next)==ESP_OK;
            }
            if(good)show_message("SCHEDULE SAVED",target==UI_OPEN?"Opening time updated":"Closing time updated","Press any key","");
            else show_message("INVALID / NOT SAVED","Check time and schedule","Press any key","");
            return;
        }
        char time_text[5]="";
        if(input_size>0) {
            size_t copied=input_size<sizeof(time_text)-1?input_size:sizeof(time_text)-1;
            memcpy(time_text,input,copied);time_text[copied]=0;
        }
        screen_set(mode==UI_OPEN?"Set OPEN time":"Set CLOSE time","Enter HHMM then #",time_text,"* clears  D cancels");
        xSemaphoreGive(ui_mutex);return;
    }
    xSemaphoreGive(ui_mutex);
}

static void keypad_task(void *arg) {
    (void)arg;
    char previous=0;
    while(true) {
        char key=scan_key();
        if(key && key!=previous)handle_key(key);
        previous=key;
        int64_t now=esp_timer_get_time()/1000;
        xSemaphoreTake(ui_mutex,portMAX_DELAY);
        if((mode==UI_MENU||mode==UI_OPEN||mode==UI_CLOSE) && now>=session_until) {
            mode=UI_PUBLIC;
            ESP_LOGI(TAG,"keypad admin session expired");
        } else if(mode==UI_MESSAGE && now>=message_until)mode=UI_PUBLIC;
        xSemaphoreGive(ui_mutex);
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

esp_err_t entrance_keypad_start(void) {
    if(started)return ESP_ERR_INVALID_STATE;
    ui_mutex=xSemaphoreCreateMutex();if(!ui_mutex)return ESP_ERR_NO_MEM;
    uint64_t row_mask=0,col_mask=0;
    for(unsigned i=0;i<4;i++){row_mask|=1ULL<<rows[i];col_mask|=1ULL<<cols[i];}
    gpio_config_t row_cfg={.pin_bit_mask=row_mask,.mode=GPIO_MODE_OUTPUT,.intr_type=GPIO_INTR_DISABLE};
    esp_err_t e=gpio_config(&row_cfg);if(e!=ESP_OK)return e;
    gpio_config_t col_cfg={.pin_bit_mask=col_mask,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_DISABLE,
        .pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    e=gpio_config(&col_cfg);if(e!=ESP_OK)return e;
    for(unsigned i=0;i<4;i++)gpio_set_level(rows[i],1);
    if(xTaskCreate(keypad_task,"entrance_keypad",4096,NULL,2,NULL)!=pdPASS)return ESP_ERR_NO_MEM;
    started=true;return ESP_OK;
}

bool entrance_keypad_admin_view(char rows_out[4][21]) {
    if(!started||!rows_out)return false;
    xSemaphoreTake(ui_mutex,portMAX_DELAY);
    bool show=mode!=UI_PUBLIC;
    if(show)memcpy(rows_out,message,sizeof(message));
    xSemaphoreGive(ui_mutex);
    return show;
}
