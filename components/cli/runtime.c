#include "cli/runtime.h"
#include "auth/runtime.h"
#include "entrance/runtime.h"
#include "storage/runtime.h"
#include "trevor/runtime.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool admin_logged_in;
static int64_t admin_session_end;
static const char *TAG = "edward_cli";

static void print_help(void) {
    printf("Commands: help, status, inside, rooms, usage, users, records, schedule, login PIN, "
           "logout\n");
    printf(
        "After login: open HH:MM, close HH:MM, useradd NAME UIDHEX, userdel ID, confirm-empty\n");
}

static void print_status(void) {
    entrance_status entrance;
    tr_system_view system;
    if (entrance_runtime_status(&entrance))
        printf("Entrance: %s, inside=%u, clock=%s\n", entrance.is_open ? "OPEN" : "CLOSED",
               (unsigned)entrance.inside_count, entrance.clock_valid ? "ready" : "waiting");
    printf("SD card: %s\n", storage_runtime_available() ? "mounted" : "unavailable");
    if (tr_runtime_snapshot(&system))
        printf("Events: storage queue drops=%lu, writes failed=%lu\n",
               (unsigned long)system.storage_queue_drops,
               (unsigned long)system.storage_write_failures);
}

static void print_light_usage(void) {
    tr_system_view system;
    if (!tr_runtime_snapshot(&system))
        return;
    puts("Room light usage since this boot (time only; no kWh estimate):");
    for (unsigned i = 0; i < 2; i++) {
        uint64_t on_seconds = system.light_on_ms[i] / 1000;
        uint64_t saved_seconds = system.empty_building_saved_ms[i] / 1000;
        printf("Room %u: on %llu min %llu sec; prevented while building empty %llu min %llu sec\n",
               i + 1, (unsigned long long)(on_seconds / 60), (unsigned long long)(on_seconds % 60),
               (unsigned long long)(saved_seconds / 60), (unsigned long long)(saved_seconds % 60));
    }
}

static void print_inside_and_users(bool show_users) {
    system_user users[SYSTEM_MAX_USERS];
    bool inside[SYSTEM_MAX_USERS];
    size_t user_count = 0, inside_count = 0;
    if (!entrance_runtime_snapshot(users, inside, SYSTEM_MAX_USERS, &user_count, &inside_count,
                                   NULL))
        return;
    printf("Inside now: %u\n", (unsigned)inside_count);
    if (show_users)
        printf("Registered users: %u\n", (unsigned)user_count);
    for (size_t i = 0; i < SYSTEM_MAX_USERS; i++) {
        if (!users[i].enabled)
            continue;
        if (show_users || inside[i])
            printf("%u: %s%s\n", (unsigned)i, users[i].display_name, inside[i] ? " (inside)" : "");
    }
}

static bool parse_time(const char *text, uint16_t *minute) {
    unsigned hour = 0, part_minute = 0;
    char extra;
    if (sscanf(text, "%u:%u %c", &hour, &part_minute, &extra) != 2 || hour >= 24 ||
        part_minute >= 60)
        return false;
    *minute = (uint16_t)(hour * 60u + part_minute);
    return true;
}

static int hex_value(char digit) {
    if (digit >= '0' && digit <= '9')
        return digit - '0';
    if (digit >= 'A' && digit <= 'F')
        return digit - 'A' + 10;
    if (digit >= 'a' && digit <= 'f')
        return digit - 'a' + 10;
    return -1;
}

static void add_user(const char *name, const char *hex) {
    size_t name_length = strlen(name), hex_length = strlen(hex);
    if (!admin_logged_in) {
        puts("Denied. Log in first.");
        return;
    }
    if (!name_length || name_length >= SYSTEM_NAME_MAX_BYTES || hex_length < 8 ||
        hex_length > SYSTEM_UID_MAX_BYTES * 2 || hex_length % 2) {
        puts("Use useradd NAME UIDHEX; name has no spaces and UID has 8-20 hex digits.");
        return;
    }
    system_user users[SYSTEM_MAX_USERS], previous[SYSTEM_MAX_USERS];
    bool inside[SYSTEM_MAX_USERS];
    size_t user_count = 0, inside_count = 0;
    if (!entrance_runtime_snapshot(users, inside, SYSTEM_MAX_USERS, &user_count, &inside_count,
                                   NULL))
        return;
    /* Keep a copy so we can restore the live roster if saving fails. */
    memcpy(previous, users, sizeof(users));
    size_t slot = SYSTEM_MAX_USERS;
    for (size_t i = 0; i < SYSTEM_MAX_USERS; i++)
        if (!users[i].enabled) {
            slot = i;
            break;
        }
    if (slot == SYSTEM_MAX_USERS) {
        puts("User list is full.");
        return;
    }
    system_user user = {.uid_size = (uint8_t)(hex_length / 2), .enabled = true};
    for (size_t i = 0; i < user.uid_size; i++) {
        int high = hex_value(hex[i * 2]), low = hex_value(hex[i * 2 + 1]);
        if (high < 0 || low < 0) {
            puts("UID must contain only hexadecimal digits.");
            return;
        }
        user.uid[i] = (uint8_t)((high << 4) | low);
    }
    snprintf(user.display_name, sizeof(user.display_name), "%s", name);
    users[slot] = user;
    esp_err_t error = entrance_runtime_set_users(users);
    if (error == ESP_OK)
        error = storage_runtime_save_users(users);
    if (error != ESP_OK) {
        (void)entrance_runtime_set_users(previous);
        printf("User not saved: %s\n", esp_err_to_name(error));
    } else
        printf("Saved user %u: %s\n", (unsigned)slot, user.display_name);
}

static void delete_user(const char *argument) {
    if (!admin_logged_in) {
        puts("Denied. Log in first.");
        return;
    }
    char *end = NULL;
    unsigned long slot = strtoul(argument, &end, 10);
    if (!argument[0] || !end || *end || slot >= SYSTEM_MAX_USERS) {
        puts("Use userdel ID from 0 to 31.");
        return;
    }
    system_user users[SYSTEM_MAX_USERS], previous[SYSTEM_MAX_USERS];
    bool inside[SYSTEM_MAX_USERS];
    size_t user_count = 0, inside_count = 0;
    if (!entrance_runtime_snapshot(users, inside, SYSTEM_MAX_USERS, &user_count, &inside_count,
                                   NULL))
        return;
    if (!users[slot].enabled) {
        puts("That user slot is empty.");
        return;
    }
    /* Do not leave the live list changed if persistent saving fails. */
    memcpy(previous, users, sizeof(users));
    memset(&users[slot], 0, sizeof(users[slot]));
    esp_err_t error = entrance_runtime_set_users(users);
    if (error == ESP_OK)
        error = storage_runtime_save_users(users);
    if (error != ESP_OK) {
        (void)entrance_runtime_set_users(previous);
        puts("User not deleted. The person may still be inside.");
    } else
        puts("User deleted.");
}

static void handle_command(char *line) {
    /* Parse one command and its first argument, then call the matching handler. */
    char command[16] = {0}, argument[40] = {0};
    (void)sscanf(line, "%15s %39s", command, argument);
    int64_t now = esp_timer_get_time() / 1000;
    if (admin_logged_in && now >= admin_session_end) {
        admin_logged_in = false;
        puts("Admin session expired.");
    } else if (admin_logged_in)
        admin_session_end = now + 60000;
    if (!strcmp(command, "help"))
        print_help();
    else if (!strcmp(command, "status"))
        print_status();
    else if (!strcmp(command, "inside"))
        print_inside_and_users(false);
    else if (!strcmp(command, "users"))
        print_inside_and_users(true);
    else if (!strcmp(command, "rooms")) {
        tr_system_view view;
        if (tr_runtime_snapshot(&view))
            for (unsigned i = 0; i < 2; i++)
                printf("Room %u: online=%u motion=%u light=%u delay=%lu ms\n", i + 1,
                       view.rooms[i].online, view.rooms[i].state.occupied,
                       view.rooms[i].state.light, (unsigned long)view.rooms[i].state.off_delay_ms);
    } else if (!strcmp(command, "usage"))
        print_light_usage();
    else if (!strcmp(command, "records"))
        storage_runtime_print_events(stdout);
    else if (!strcmp(command, "schedule")) {
        entrance_status status;
        if (entrance_runtime_status(&status))
            printf("Access hours: %02u:%02u-%02u:%02u Kampala time\n",
                   status.schedule.start_minute / 60, status.schedule.start_minute % 60,
                   status.schedule.end_minute / 60, status.schedule.end_minute % 60);
    } else if (!strcmp(command, "login")) {
        admin_logged_in = auth_runtime_verify_pin(argument);
        admin_session_end = now + 60000;
        if (!admin_logged_in)
            ESP_LOGW(TAG, "admin login failed or is locked out");
        puts(admin_logged_in ? "Admin login accepted for 60 seconds." : "Admin login denied.");
    } else if (!strcmp(command, "logout")) {
        admin_logged_in = false;
        puts("Logged out.");
    } else if (!strcmp(command, "open") || !strcmp(command, "close")) {
        uint16_t minute;
        if (!admin_logged_in)
            puts("Denied. Use login PIN first.");
        else if (!parse_time(argument, &minute))
            puts("Use a valid 24-hour time such as 08:00.");
        else {
            entrance_status status;
            if (!entrance_runtime_status(&status))
                puts("Entrance is not ready.");
            else {
                system_access_schedule next = status.schedule;
                if (!strcmp(command, "open"))
                    next.start_minute = minute;
                else
                    next.end_minute = minute;
                esp_err_t error = entrance_runtime_set_schedule(next);
                puts(error == ESP_OK ? "Schedule saved."
                                     : "Schedule rejected; check that start and end differ.");
            }
        }
    } else if (!strcmp(command, "useradd")) {
        char name[SYSTEM_NAME_MAX_BYTES] = {0}, uid[2 * SYSTEM_UID_MAX_BYTES + 1] = {0};
        if (sscanf(line, "%*15s %23s %20s", name, uid) == 2)
            add_user(name, uid);
        else
            puts("Use useradd NAME UIDHEX.");
    } else if (!strcmp(command, "userdel")) {
        delete_user(argument);
    } else if (!strcmp(command, "confirm-empty")) {
        if (!admin_logged_in)
            puts("Denied. Log in first.");
        else if (entrance_runtime_confirm_empty() == ESP_OK)
            puts("Occupancy marked confirmed empty. Use only after checking the building.");
        else
            puts("Not empty: the entrance list still has people recorded inside.");
    } else
        puts("Unknown command. Type help.");
}

static void serial_task(void *arg) {
    (void)arg;
    char line[96];
    print_help();
    while (fgets(line, sizeof(line), stdin)) {
        handle_command(line);
        memset(line, 0, sizeof(line));
    }
    vTaskDelete(NULL);
}

esp_err_t cli_runtime_start(void) {
    return xTaskCreate(serial_task, "edward_serial_cli", 4096, NULL, 2, NULL) == pdPASS
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}
