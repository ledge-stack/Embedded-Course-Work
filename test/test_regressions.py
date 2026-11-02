"""Host regressions using actual C function bodies; HTTP/NVS/GPIO are isolated.

Run with Python on Windows and CC pointing to TinyCC. No ESP32 simulation is
claimed: the original firmware build and Wokwi checks remain separate gates.
"""
import ctypes as C
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(os.environ.get('REGRESSION_SOURCE_ROOT', Path(__file__).resolve().parents[1]))
WORK = Path(os.environ.get('TEST_BUILD', tempfile.gettempdir()))
WORK.mkdir(parents=True, exist_ok=True)


def body(source, signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


web = (ROOT / 'components/entrance/dashboard.c').read_text()
cli = (ROOT / 'components/cli/runtime.c').read_text()
keypad = (ROOT / 'components/entrance/keypad.c').read_text()
storage = (ROOT / 'components/storage/runtime.c').read_text()
entrance = (ROOT / 'components/entrance/runtime.c').read_text()
mask = keypad[keypad.index('char masked['):keypad.index('screen_set("ADMIN LOGIN", masked')]
capacity = int(re.search(r'char masked\[(\d+)\]', mask).group(1))
mask = mask[mask.index(';') + 1:].replace('masked', 'sample.masked')
helpers = '\n'.join(body(web, signature) for signature in
                    ('static int form_hex(', 'static bool form_value(') if signature in web)

source = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <time.h>
#include <windows.h>
#include "common/system.h"
#include "entrance/core.h"
#define API __declspec(dllexport)
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NVS_NOT_FOUND 0x1102
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(x) (x)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define BODY_MAX 96
typedef struct { int content_len; const char *body; } httpd_req_t;
static int web_status;
static system_access_schedule saved_schedule;
static bool request_session(httpd_req_t *r, bool csrf) { (void)r; (void)csrf; return true; }
static int send_error(httpd_req_t *r, int status, const char *text) {
    (void)r; (void)text; web_status = status; return ESP_FAIL;
}
static bool receive_body(httpd_req_t *r, char *out, size_t cap) {
    if ((size_t)r->content_len >= cap) return false;
    memcpy(out, r->body, r->content_len + 1); return true;
}
static int probe_set_schedule(system_access_schedule s) {
    if (s.start_minute >= 1440 || s.end_minute >= 1440 || s.start_minute == s.end_minute)
        return ESP_FAIL;
    saved_schedule = s; return ESP_OK;
}
static int httpd_resp_set_type(httpd_req_t *r, const char *type) { (void)r; (void)type; return 0; }
static int httpd_resp_sendstr(httpd_req_t *r, const char *text) { (void)r; (void)text; return 0; }
#define entrance_runtime_set_schedule probe_set_schedule
''' + helpers + '\n' + body(web, 'static bool parse_time_value(') + '\n' + body(web, 'static esp_err_t schedule_handler(') + r'''
API int check_web(const char *text) {
    httpd_req_t req = {(int)strlen(text), text};
    web_status = 200; schedule_handler(&req); return web_status;
}
API int check_web_start(void) { return saved_schedule.start_minute; }
API int check_web_end(void) { return saved_schedule.end_minute; }
#undef entrance_runtime_set_schedule
''' + body(cli, 'static bool parse_time(') + r'''
API int check_time(const char *text) { uint16_t minute; return parse_time(text, &minute); }
API int check_mask(unsigned input_size) {
''' + f'    struct {{ char masked[{capacity}]; unsigned char guard[8]; }} sample;\n' + r'''
    memset(&sample, 0x7e, sizeof(sample)); strcpy(sample.masked, "PIN: ");
''' + mask + r'''
    for (unsigned i = 0; i < sizeof(sample.guard); i++)
        if (sample.guard[i] != 0x7e) return 0;
    return strlen(sample.masked) == 5 + input_size;
}

typedef unsigned nvs_handle_t;
static unsigned char nvs_bytes[sizeof(system_user) * SYSTEM_MAX_USERS];
static bool nvs_exists, fail_nvs, fail_sd, ready;
static int file_mutex, file_depth, output_while_locked;
static HANDLE config_locks[3], first_start, second_start;
static uint16_t nvs_start, nvs_end;
static char roster_file[1024], roster_temp[1024], event_file[1024];
#define ROSTER_FILE roster_file
#define ROSTER_TEMP_FILE roster_temp
#define EVENT_FILE event_file
#define ROSTER_MAGIC 0x434F4349u
typedef struct { uint32_t magic; uint16_t version, byte_count; } roster_header;
static void xSemaphoreTake(int m, unsigned t) {
    if (m) WaitForSingleObject(config_locks[m], t); else file_depth++;
}
static void xSemaphoreGive(int m) { if (m) ReleaseMutex(config_locks[m]); else file_depth--; }
static int nvs_open(const char *name, int mode, nvs_handle_t *h) {
    if (!strcmp(name, "access_cfg")) { *h = 2; return ESP_OK; }
    *h = 1;
    return mode == NVS_READONLY && !nvs_exists ? ESP_ERR_NVS_NOT_FOUND : ESP_OK;
}
static int nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *size) {
    (void)h; (void)key;
    if (!nvs_exists) return ESP_ERR_NVS_NOT_FOUND;
    if (*size < sizeof(nvs_bytes)) return ESP_ERR_INVALID_SIZE;
    memcpy(out, nvs_bytes, sizeof(nvs_bytes)); *size = sizeof(nvs_bytes); return ESP_OK;
}
static int nvs_set_blob(nvs_handle_t h, const char *key, const void *data, size_t size) {
    (void)h; (void)key;
    if (fail_nvs || size != sizeof(nvs_bytes)) return ESP_FAIL;
    memcpy(nvs_bytes, data, size); nvs_exists = true; return ESP_OK;
}
static int nvs_commit(nvs_handle_t h) { (void)h; return ESP_OK; }
static void nvs_close(nvs_handle_t h) { (void)h; }
static int nvs_set_u16(nvs_handle_t h, const char *key, uint16_t value) {
    (void)h;
    if (!strcmp(key, "start")) {
        nvs_start = value;
        if (value == 480) { SetEvent(first_start); WaitForSingleObject(second_start, 100); }
        else if (value == 540) SetEvent(second_start);
    } else nvs_end = value;
    return ESP_OK;
}
static bool storage_runtime_available(void) { return ready; }
static void unmount_card(void) { ready = false; }
static int probe_sync(int fd) { (void)fd; return 0; }
#define fsync probe_sync
static FILE *probe_fopen(const char *name, const char *mode) {
    if (fail_sd && mode[0] == 'w') return NULL;
    return fopen(name, mode);
}
#define fopen probe_fopen
''' + body(storage, 'esp_err_t storage_runtime_load_users(') + '\n' + body(storage, 'esp_err_t storage_runtime_save_users(') + r'''
static int probe_fputs(const char *text, FILE *out) {
    if (file_depth) output_while_locked = 1;
    return fputs(text, out);
}
#define fputs probe_fputs
''' + body(storage, 'void storage_runtime_print_events(') + r'''
#undef fputs
#undef fopen
API void reset_storage(const char *path) {
    snprintf(roster_file, sizeof(roster_file), "%s/users.bin", path);
    snprintf(roster_temp, sizeof(roster_temp), "%s/users.tmp", path);
    snprintf(event_file, sizeof(event_file), "%s/events.csv", path);
    nvs_exists = fail_nvs = fail_sd = ready = false;
    file_depth = output_while_locked = 0;
    memset(nvs_bytes, 0, sizeof(nvs_bytes));
}
API void set_sd(int mounted, int fail_write) { ready = mounted; fail_sd = fail_write; }
API void set_nvs_failure(int fail) { fail_nvs = fail; }
API int save_roster(int marker) {
    system_user users[SYSTEM_MAX_USERS] = {0}; users[0].uid[0] = marker;
    return storage_runtime_save_users(users);
}
API int load_roster(void) {
    system_user users[SYSTEM_MAX_USERS] = {0};
    return storage_runtime_load_users(users) == ESP_OK ? users[0].uid[0] : -1;
}
API void seed_legacy_sd(int marker) {
    system_user users[SYSTEM_MAX_USERS] = {0}; users[0].uid[0] = marker;
    roster_header h = {ROSTER_MAGIC, 1, sizeof(users)};
    FILE *f = fopen(roster_file, "wb"); if (!f) abort();
    fwrite(&h, sizeof(h), 1, f); fwrite(users, sizeof(users), 1, f); fclose(f);
}
API int check_recent_records(void) {
    FILE *f = fopen(event_file, "w"); if (!f) abort();
    for (int i = 0; i < 500; i++) fprintf(f, "event_%04d,some room or access information\n", i);
    fclose(f); ready = true;
    FILE *out = fopen(roster_temp, "w+b"); if (!out) return 0;
    storage_runtime_print_events(out);
    long size = ftell(out);
    rewind(out); char text[2049] = {0}; fread(text, 1, 2048, out); fclose(out);
    return size <= 2048 && strstr(text, "event_0499") && !output_while_locked && file_depth == 0;
}
static bool started = true, clock_ready = true;
static int state_mutex = 1, config_mutex = 2;
static entrance_state state;
bool entrance_schedule_valid(system_access_schedule s) {
    return s.start_minute < 1440 && s.end_minute < 1440 && s.start_minute != s.end_minute;
}
static bool tr_runtime_record_schedule_change(system_access_schedule a,
                                              system_access_schedule b, uint64_t utc) {
    (void)a; (void)b; (void)utc; return true;
}
''' + body(entrance, 'esp_err_t entrance_runtime_set_schedule(') + r'''
static DWORD WINAPI write_first_schedule(void *arg) {
    (void)arg; entrance_runtime_set_schedule((system_access_schedule){480,1080}); return 0;
}
static DWORD WINAPI write_second_schedule(void *arg) {
    (void)arg; entrance_runtime_set_schedule((system_access_schedule){540,1020}); return 0;
}
API int check_concurrent_schedule(void) {
    config_locks[1] = CreateMutex(NULL, FALSE, NULL);
    config_locks[2] = CreateMutex(NULL, FALSE, NULL);
    first_start = CreateEvent(NULL, TRUE, FALSE, NULL);
    second_start = CreateEvent(NULL, TRUE, FALSE, NULL);
    HANDLE a = CreateThread(NULL, 0, write_first_schedule, NULL, 0, NULL);
    WaitForSingleObject(first_start, 1000);
    HANDLE b = CreateThread(NULL, 0, write_second_schedule, NULL, 0, NULL);
    WaitForSingleObject(a, 2000); WaitForSingleObject(b, 2000);
    int consistent = nvs_start == state.schedule.start_minute && nvs_end == state.schedule.end_minute;
    CloseHandle(a); CloseHandle(b); CloseHandle(first_start); CloseHandle(second_start);
    CloseHandle(config_locks[1]); CloseHandle(config_locks[2]);
    return consistent;
}

static jmp_buf stop_serial;
static const int *serial_input;
static int serial_count, serial_pos, command_count, deleted;
static char commands[8][96];
static int fake_getchar(void) { return serial_pos < serial_count ? serial_input[serial_pos++] : EOF; }
static char *fake_fgets(char *buf, int cap, FILE *f) { (void)buf; (void)cap; (void)f; return NULL; }
static void fake_clearerr(FILE *f) { (void)f; }
static void vTaskDelay(unsigned ms) { (void)ms; if (serial_pos >= serial_count) longjmp(stop_serial, 1); }
static void vTaskDelete(void *task) { (void)task; deleted = 1; longjmp(stop_serial, 1); }
static void print_help(void) {}
static void handle_command(char *text) {
    if (command_count < 8) snprintf(commands[command_count++], 96, "%s", text);
}
#define getchar fake_getchar
#define fgets fake_fgets
#define clearerr fake_clearerr
''' + body(cli, 'static void serial_task(') + r'''
API int check_serial(const int *tokens, int count) {
    serial_input = tokens; serial_count = count; serial_pos = command_count = deleted = 0;
    memset(commands, 0, sizeof(commands));
    if (!setjmp(stop_serial)) serial_task(NULL);
    return deleted ? -1 : command_count;
}
API const char *serial_command(int i) { return commands[i]; }
'''

generated = WORK / 'cocis_regressions.c'
generated.write_text(source)
dll = WORK / 'cocis_regressions.dll'
subprocess.run([os.environ.get('CC', 'tcc'), '-shared', '-I' + str(ROOT / 'include'),
                str(generated), '-o', str(dll)], check=True)
lib = C.CDLL(str(dll))
for name in ('check_web', 'check_time', 'reset_storage'):
    getattr(lib, name).argtypes = [C.c_char_p]
lib.check_serial.argtypes = [C.POINTER(C.c_int), C.c_int]
lib.serial_command.argtypes = [C.c_int]
lib.serial_command.restype = C.c_char_p


class RegressionTests(unittest.TestCase):
    def test_records_are_bounded_and_printed_after_releasing_sd_lock(self):
        with tempfile.TemporaryDirectory(dir=WORK) as directory:
            lib.reset_storage(os.fsencode(directory))
            self.assertEqual(lib.check_recent_records(), 1)

    def test_concurrent_schedule_keeps_nvs_and_live_hours_consistent(self):
        self.assertEqual(lib.check_concurrent_schedule(), 1)

    def test_keypad_all_four_pin_digits_fit(self):
        for digits in range(5):
            with self.subTest(digits=digits):
                self.assertEqual(lib.check_mask(digits), 1)

    def test_dashboard_accepts_browser_encoded_hours(self):
        self.assertEqual(lib.check_web(b'start=08%3A00&end=18%3A00'), 200)
        self.assertEqual((lib.check_web_start(), lib.check_web_end()), (480, 1080))

    def test_dashboard_accepts_reordered_fields(self):
        self.assertEqual(lib.check_web(b'end=06%3a00&start=22%3a00'), 200)
        self.assertEqual((lib.check_web_start(), lib.check_web_end()), (1320, 360))

    def test_dashboard_rejects_malformed_or_duplicate_values(self):
        for value in (b'start=08%ZZ00&end=18%3A00', b'start=08%00&end=18%3A00',
                      b'start=08:00&end=18:00&start=09:00', b'xstart=08:00&end=18:00',
                      b'start=24:00&end=18:00', b'start=08:00&end=08:00'):
            with self.subTest(value=value):
                self.assertEqual(lib.check_web(value), 400)

    def test_cli_time_rejects_overflow_and_malformed_input(self):
        for text in (b'4294967305:00', b'24:00', b'09:60', b'9:00', b'+9:00', b'09:00x'):
            self.assertEqual(lib.check_time(text), 0, text)
        self.assertEqual(lib.check_time(b'09:00'), 1)
        self.assertEqual(lib.check_time(b'23:59'), 1)

    def serial(self, tokens):
        array = (C.c_int * len(tokens))(*tokens)
        return lib.check_serial(array, len(tokens))

    def test_cli_survives_idle_and_partial_reads(self):
        tokens = [-1, -1] + list(b'he') + [-1] + list(b'lp\r\nstatus\n') + [-1]
        self.assertEqual(self.serial(tokens), 2)
        self.assertEqual(lib.serial_command(0), b'help')
        self.assertEqual(lib.serial_command(1), b'status')

    def test_cli_discards_whole_overlong_line(self):
        self.assertEqual(self.serial(list(b'x' * 120 + b'\nhelp\n') + [-1]), 1)
        self.assertEqual(lib.serial_command(0), b'help')

    def test_new_nvs_roster_wins_over_reinserted_old_sd(self):
        with tempfile.TemporaryDirectory(dir=WORK) as directory:
            lib.reset_storage(os.fsencode(directory))
            lib.seed_legacy_sd(1)
            self.assertEqual(lib.save_roster(2), 0)
            lib.set_sd(1, 0)
            self.assertEqual(lib.load_roster(), 2)

    def test_sd_mirror_failure_does_not_undo_committed_nvs(self):
        with tempfile.TemporaryDirectory(dir=WORK) as directory:
            lib.reset_storage(os.fsencode(directory))
            lib.set_sd(1, 1)
            self.assertEqual(lib.save_roster(3), 0)
            self.assertEqual(lib.load_roster(), 3)

    def test_legacy_sd_roster_is_still_readable_without_nvs(self):
        with tempfile.TemporaryDirectory(dir=WORK) as directory:
            lib.reset_storage(os.fsencode(directory))
            lib.seed_legacy_sd(4)
            lib.set_sd(1, 0)
            self.assertEqual(lib.load_roster(), 4)

    def test_failed_nvs_save_preserves_previous_roster(self):
        with tempfile.TemporaryDirectory(dir=WORK) as directory:
            lib.reset_storage(os.fsencode(directory))
            self.assertEqual(lib.save_roster(5), 0)
            lib.set_nvs_failure(1)
            self.assertNotEqual(lib.save_roster(6), 0)
            self.assertEqual(lib.load_roster(), 5)


if __name__ == '__main__':
    unittest.main(verbosity=2)
