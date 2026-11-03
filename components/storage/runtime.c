#include "storage/runtime.h"
#include "driver/spi_common.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CARD_MOUNT_POINT "/sdcard"
#define EVENT_FILE CARD_MOUNT_POINT "/events.csv"
#define ROSTER_FILE CARD_MOUNT_POINT "/users.bin"
#define ROSTER_TEMP_FILE CARD_MOUNT_POINT "/users.tmp"
#define ROSTER_MAGIC 0x434F4349u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t byte_count;
} roster_header;

static const char *TAG = "edward_storage";
static sdmmc_card_t *card;
static volatile bool ready;
static bool spi_ready;
static SemaphoreHandle_t file_mutex;

static uint32_t read_u32_le(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static uint64_t read_u64_le(const uint8_t *bytes) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; i++)
        value |= (uint64_t)bytes[i] << (8 * i);
    return value;
}

static void format_local_time(uint64_t utc_seconds, char text[24]) {
    if (!utc_seconds) {
        snprintf(text, 24, "TIME_UNAVAILABLE");
        return;
    }
    time_t utc = (time_t)utc_seconds;
    struct tm local = {0};
    if (!localtime_r(&utc, &local) || !strftime(text, 24, "%Y-%m-%d %H:%M:%S EAT", &local))
        snprintf(text, 24, "TIME_UNAVAILABLE");
}

static const char *access_point_name(uint8_t point) {
    return point == SYSTEM_ENTRY ? "ENTRY" : point == SYSTEM_EXIT ? "EXIT" : "UNKNOWN_POINT";
}

static const char *access_result_name(uint8_t result) {
    switch (result) {
    case SYSTEM_ACCESS_GRANTED:
        return "GRANTED";
    case SYSTEM_ACCESS_UNKNOWN_USER:
        return "UNKNOWN_USER";
    case SYSTEM_ACCESS_ALREADY_INSIDE:
        return "ALREADY_INSIDE";
    case SYSTEM_ACCESS_NOT_INSIDE:
        return "NOT_INSIDE";
    case SYSTEM_ACCESS_SCHEDULE_CLOSED:
        return "SCHEDULE_CLOSED";
    case SYSTEM_ACCESS_CLOCK_UNAVAILABLE:
        return "CLOCK_UNAVAILABLE";
    case SYSTEM_ACCESS_STALE_EVENT:
        return "STALE_EVENT";
    default:
        return "UNKNOWN_RESULT";
    }
}

static bool write_access_event(FILE *file, const tr_storage_event *event) {
    const uint8_t *data = event->access_data;
    if (event->access_size < 28 || event->access_size > 27 + SYSTEM_UID_MAX_BYTES)
        return false;
    uint8_t uid_size = data[26];
    if (!uid_size || uid_size > SYSTEM_UID_MAX_BYTES || event->access_size != 27 + uid_size)
        return false;

    char timestamp[24], uid[2 * SYSTEM_UID_MAX_BYTES + 1];
    format_local_time(read_u64_le(data + 8), timestamp);
    for (unsigned i = 0; i < uid_size; i++)
        snprintf(uid + i * 2, sizeof(uid) - i * 2, "%02X", data[27 + i]);
    uid[uid_size * 2] = 0;

    return fprintf(file, "ACCESS,%" PRIu32 ",%s,\"%s\",%s,%s,%s,%" PRIu32 ",%" PRIu32
                         ",%" PRIu64 ",%" PRIu64 "\n",
                   read_u32_le(data), timestamp, event->access_name,
                   access_point_name(data[24]), access_result_name(data[25]), uid,
                   read_u32_le(data + 4), event->inside_count, read_u64_le(data + 8),
                   read_u64_le(data + 16)) > 0;
}

static bool write_schedule_event(FILE *file, const tr_storage_event *event) {
    char timestamp[24];
    format_local_time(event->schedule_utc_seconds, timestamp);
    return fprintf(file, "SCHEDULE,%s,OPEN=%02u:%02u->%02u:%02u,CLOSE=%02u:%02u->%02u:%02u\n",
                   timestamp, event->old_schedule.start_minute / 60,
                   event->old_schedule.start_minute % 60,
                   event->new_schedule.start_minute / 60, event->new_schedule.start_minute % 60,
                   event->old_schedule.end_minute / 60, event->old_schedule.end_minute % 60,
                   event->new_schedule.end_minute / 60, event->new_schedule.end_minute % 60) > 0;
}

esp_err_t storage_runtime_init_shared_spi(void) {
    if (spi_ready)
        return ESP_OK;
    spi_bus_config_t bus = {.mosi_io_num = 23,
                            .miso_io_num = 19,
                            .sclk_io_num = 18,
                            .quadwp_io_num = -1,
                            .quadhd_io_num = -1,
                            .max_transfer_sz = 4096};
    esp_err_t error = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (error == ESP_OK || error == ESP_ERR_INVALID_STATE) {
        spi_ready = true;
        return ESP_OK;
    }
    return error;
}

static esp_err_t try_mount_card(void) {
    if (ready)
        return ESP_OK;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = GPIO_NUM_2;
    esp_vfs_fat_mount_config_t mount = {
        .format_if_mount_failed = false, .max_files = 4, .allocation_unit_size = 16 * 1024};
    esp_err_t error = esp_vfs_fat_sdspi_mount(CARD_MOUNT_POINT, &host, &slot, &mount, &card);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "SD unavailable; event queue remains active, but events will not persist: %s",
                 esp_err_to_name(error));
        return error;
    }
    ready = true;
    ESP_LOGI(TAG, "SD card mounted; access and room events go to %s", EVENT_FILE);
    return ESP_OK;
}

esp_err_t storage_runtime_start(void) {
    file_mutex = xSemaphoreCreateMutex();
    if (!file_mutex)
        return ESP_ERR_NO_MEM;
    /* RFID and SD share SPI2; initialize it once with SD-sized transfers. */
    esp_err_t error = storage_runtime_init_shared_spi();
    if (error != ESP_OK)
        return error;
    (void)try_mount_card();
    return ESP_OK;
}

bool storage_runtime_available(void) {
    xSemaphoreTake(file_mutex, portMAX_DELAY);
    bool mounted = ready;
    xSemaphoreGive(file_mutex);
    return mounted;
}

static void unmount_card(void) {
    if (ready)
        (void)esp_vfs_fat_sdcard_unmount(CARD_MOUNT_POINT, card);
    ready = false;
    card = NULL;
}

esp_err_t storage_runtime_load_users(system_user users[SYSTEM_MAX_USERS]) {
    if (!users)
        return ESP_ERR_INVALID_ARG;
    /* NVS is authoritative: an old reinserted SD card must not revive deleted users. */
    xSemaphoreTake(file_mutex, portMAX_DELAY);
    nvs_handle_t handle;
    esp_err_t error = nvs_open("users", NVS_READONLY, &handle);
    if (error == ESP_OK) {
        size_t size = sizeof(system_user) * SYSTEM_MAX_USERS;
        error = nvs_get_blob(handle, "roster", users, &size);
        nvs_close(handle);
        if (error == ESP_OK && size != sizeof(system_user) * SYSTEM_MAX_USERS)
            error = ESP_ERR_INVALID_SIZE;
    }
    if (error != ESP_ERR_NVS_NOT_FOUND) {
        xSemaphoreGive(file_mutex);
        return error;
    }
    /* Support an existing version-1 SD roster on a device with no NVS roster. */
    if (ready) {
        FILE *file = fopen(ROSTER_FILE, "rb");
        if (file) {
            roster_header header = {0};
            /* Check the file label and size before loading the saved roster. */
            bool valid = fread(&header, sizeof(header), 1, file) == 1 &&
                         header.magic == ROSTER_MAGIC && header.version == 1 &&
                         header.byte_count == sizeof(system_user) * SYSTEM_MAX_USERS &&
                         fread(users, header.byte_count, 1, file) == 1;
            fclose(file);
            error = valid ? ESP_OK : ESP_ERR_INVALID_SIZE;
        }
    }
    xSemaphoreGive(file_mutex);
    return error;
}

esp_err_t storage_runtime_save_users(const system_user users[SYSTEM_MAX_USERS]) {
    if (!users)
        return ESP_ERR_INVALID_ARG;
    /* Commit the authoritative roster before updating its optional SD mirror. */
    xSemaphoreTake(file_mutex, portMAX_DELAY);
    nvs_handle_t handle;
    esp_err_t error = nvs_open("users", NVS_READWRITE, &handle);
    if (error != ESP_OK) {
        xSemaphoreGive(file_mutex);
        return error;
    }
    error = nvs_set_blob(handle, "roster", users, sizeof(system_user) * SYSTEM_MAX_USERS);
    if (error == ESP_OK)
        error = nvs_commit(handle);
    nvs_close(handle);
    if (error != ESP_OK) {
        xSemaphoreGive(file_mutex);
        return error;
    }
    if (!ready) {
        xSemaphoreGive(file_mutex);
        ESP_LOGW(TAG, "roster saved in NVS; SD mirror unavailable");
        return ESP_OK;
    }

    /* Write a temporary file before replacing the previous SD roster. */
    FILE *file = fopen(ROSTER_TEMP_FILE, "wb");
    if (!file) {
        unmount_card();
        xSemaphoreGive(file_mutex);
        ESP_LOGW(TAG, "roster saved in NVS; could not update SD mirror");
        return ESP_OK;
    }
    roster_header header = {
        .magic = ROSTER_MAGIC, .version = 1, .byte_count = sizeof(system_user) * SYSTEM_MAX_USERS};
    bool saved = fwrite(&header, sizeof(header), 1, file) == 1 &&
                 fwrite(users, sizeof(system_user) * SYSTEM_MAX_USERS, 1, file) == 1;
    if (fflush(file) != 0)
        saved = false;
    if (fsync(fileno(file)) != 0)
        saved = false;
    if (fclose(file) != 0)
        saved = false;
    if (saved) {
        (void)remove(ROSTER_FILE);
        if (rename(ROSTER_TEMP_FILE, ROSTER_FILE) != 0)
            saved = false;
    }
    if (!saved)
        (void)remove(ROSTER_TEMP_FILE);
    if (!saved)
        unmount_card();
    xSemaphoreGive(file_mutex);
    if (!saved)
        ESP_LOGW(TAG, "roster saved in NVS; could not update SD mirror");
    /* An SD error must not make the CLI roll back an already committed roster. */
    return ESP_OK;
}

void storage_runtime_print_events(FILE *output) {
    if (!output)
        return;
    char *text = malloc(2049);
    if (!text) {
        fprintf(output, "Not enough memory to read records.\n");
        return;
    }
    const char *message = NULL;
    size_t used = 0;
    long size = 0;
    xSemaphoreTake(file_mutex, portMAX_DELAY);
    if (!ready) message = "SD card unavailable; no records to show.\n";
    else {
        FILE *file = fopen(EVENT_FILE, "r");
        if (!file) message = "No event file yet.\n";
        else {
            if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
                fseek(file, size > 2048 ? size - 2048 : 0, SEEK_SET) != 0)
                message = "Could not read records.\n";
            else {
                used = fread(text, 1, 2048, file);
                if (ferror(file)) message = "Could not read records.\n";
            }
            fclose(file);
        }
    }
    xSemaphoreGive(file_mutex);
    /* Slow UART output must not hold the SD writer's mutex. */
    text[used] = 0;
    char *start = text;
    if (size > 2048) {
        char *newline = strchr(text, '\n');
        start = newline ? newline + 1 : text + used;
    }
    fputs(message ? message : start, output);
    free(text);
}

/* Trevor's single storage task is the only caller, so file writes are serialized. */
bool tr_storage_append(const tr_storage_event *event) {
    if (!event)
        return false;
    xSemaphoreTake(file_mutex, portMAX_DELAY);
    if (!ready && try_mount_card() != ESP_OK) {
        xSemaphoreGive(file_mutex);
        return false;
    }
    FILE *file = fopen(EVENT_FILE, "a");
    if (!file) {
        unmount_card();
        xSemaphoreGive(file_mutex);
        return false;
    }
    bool ok = false;
    if (event->kind == TR_EVENT_ROOM) {
        const tr_room_state *room = &event->room_state;
        ok = fprintf(file, "ROOM,%llu,%u,%u,%u,%" PRIu32 "\n",
                     (unsigned long long)event->received_ms, (unsigned)room->room,
                     (unsigned)room->occupied, (unsigned)room->light, room->off_delay_ms) > 0;
    } else if (event->kind == TR_EVENT_ACCESS) {
        ok = write_access_event(file, event);
    } else if (event->kind == TR_EVENT_SCHEDULE) {
        ok = write_schedule_event(file, event);
    }
    if (fflush(file) != 0)
        ok = false;
    if (fsync(fileno(file)) != 0)
        ok = false;
    if (fclose(file) != 0)
        ok = false;
    if (!ok) {
        ESP_LOGE(TAG, "could not append event (card may be full or removed)");
        unmount_card();
    }
    xSemaphoreGive(file_mutex);
    return ok;
}
