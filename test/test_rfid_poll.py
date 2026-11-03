"""Exercise the real RFID request/poll functions with simulated reader registers."""
import ctypes as C
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
code = (ROOT / 'components/entrance/mfrc522.c').read_text(encoding='utf-8')


def function(signature):
    start = code.index(signature)
    end = code.index('{', start) + 1
    depth = 1
    while depth:
        depth += (code[end] == '{') - (code[end] == '}')
        end += 1
    return code[start:end]


transfer = function('static bool transceive(')
has_timeout = 'bool *timed_out' in transfer
enums = code[code.index('enum {'):code.index('static spi_device_handle_t devices')]
source = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define API __declspec(dllexport)
#define ESP_OK 0
#define ESP_FAIL -1
typedef void *spi_device_handle_t;
static int mode, warnings;
static spi_device_handle_t devices[2] = {(void *)1, (void *)2};
static char last_poll_issue[2][128];
static int reg_write(spi_device_handle_t d, uint8_t r, uint8_t v) { (void)d; (void)r; (void)v; return 0; }
''' + enums + r'''
static int reg_read(spi_device_handle_t d, uint8_t reg, uint8_t *out) {
    (void)d;
    if (mode == 1) return ESP_FAIL;
    if (reg == REG_COM_IRQ) *out = mode == 0 ? 1 : mode == 2 ? 0x20 : mode == 4 ? 0x02 : 0;
    else if (reg == REG_FIFO_LEVEL) *out = 2;
    else *out = 0;
    return ESP_OK;
}
static int fifo_write(spi_device_handle_t d, const uint8_t *p, size_t n) { (void)d; (void)p; (void)n; return 0; }
static int fifo_read(spi_device_handle_t d, uint8_t *p, size_t n) { (void)d; memset(p, 0, n); return 0; }
static void vTaskDelay(unsigned ticks) { (void)ticks; }
static void log_poll_issue(uint8_t reader, const char *text) { (void)reader; (void)text; warnings++; }
static bool select_level(spi_device_handle_t d, uint8_t s, uint8_t b[5], uint8_t *sak) { (void)d; (void)s; (void)b; (void)sak; return false; }
static void halt_card(spi_device_handle_t d) { (void)d; }
''' + transfer + '\n' + function('bool mfrc522_poll(') + r'''
API int poll_warnings(int test_mode) {
    uint8_t uid[10], size;
    mode = test_mode; warnings = 0;
    mfrc522_poll(0, uid, &size);
    return warnings;
}
API int transfer_result(int test_mode) {
    uint8_t request = 0x26, reply[2], bits;
    size_t received = 0;
    bool timed_out = false;
    mode = test_mode;
''' + ('    bool ok = transceive(devices[0], &request, 1, 7, reply, 2, &received, &bits, &timed_out);\n'
       if has_timeout else
       '    bool ok = transceive(devices[0], &request, 1, 7, reply, 2, &received, &bits);\n') + r'''
    return ok ? 2 : timed_out ? 1 : 0;
}
'''
work = Path(os.environ.get('TEST_BUILD', tempfile.gettempdir()))
work.mkdir(parents=True, exist_ok=True)
cfile, dll = work/'rfid_poll.c', work/'rfid_poll.dll'
cfile.write_text(source, encoding='utf-8')
subprocess.run([os.environ.get('CC', 'tcc'), '-shared', str(cfile), '-o', str(dll)], check=True)
lib = C.CDLL(str(dll))


class ReaderTests(unittest.TestCase):
    def test_idle_reader_is_quiet(self):
        self.assertEqual(lib.poll_warnings(0), 0)

    def test_card_absence_is_distinguished_from_spi_error(self):
        self.assertEqual(lib.transfer_result(0), 1)
        self.assertEqual(lib.transfer_result(1), 0)
        self.assertEqual(lib.poll_warnings(1), 1)

    def test_software_timeout_is_also_idle(self):
        self.assertEqual(lib.transfer_result(3), 1)

    def test_valid_request_reply_is_not_a_timeout(self):
        self.assertEqual(lib.transfer_result(2), 2)

    def test_reader_error_is_still_reported(self):
        self.assertEqual(lib.transfer_result(4), 0)
        self.assertEqual(lib.poll_warnings(4), 1)


if __name__ == '__main__':
    unittest.main(verbosity=2)
