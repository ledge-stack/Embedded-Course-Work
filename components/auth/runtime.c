#include "auth/runtime.h"
#include "mbedtls/platform_util.h"
#include "psa/crypto.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

#define PIN_ITERATIONS 1000
#define PIN_SIZE 4
#define LOCKOUT_MS 60000u

static const unsigned char salt[16] = {0x4c, 0x65, 0x6f, 0x6e, 0x2d, 0x65, 0x6e, 0x74,
                                       0x72, 0x61, 0x6e, 0x63, 0x65, 0x2d, 0x76, 0x31};
/* Store a derived verifier, not the four-digit demo PIN itself. */
static const unsigned char verifier[32] = {
    0x4b, 0x1d, 0xcf, 0xa6, 0x34, 0x6f, 0xea, 0x24, 0xb1, 0xbc, 0xfe, 0x07, 0x8d, 0x69, 0xc7, 0xe8,
    0x59, 0xfd, 0x4b, 0xcd, 0xe9, 0xd4, 0xdf, 0x63, 0x2e, 0xba, 0xb7, 0x36, 0x7b, 0x81, 0x85, 0xe3};
static SemaphoreHandle_t auth_mutex;
static unsigned failures;
static TickType_t locked_until_ticks;

void auth_runtime_init(void) {
    if (!auth_mutex)
        auth_mutex = xSemaphoreCreateMutex();
}

bool auth_runtime_verify_pin(const char *pin) {
    if (!auth_mutex || !pin)
        return false;
    size_t length = strlen(pin);
    unsigned char derived[32] = {0};
    bool matches = false;
    xSemaphoreTake(auth_mutex, portMAX_DELAY);
    TickType_t now = xTaskGetTickCount();
    /* Five wrong attempts start a one-minute lockout. */
    if ((int32_t)(now - locked_until_ticks) < 0) {
        xSemaphoreGive(auth_mutex);
        return false;
    }
    if (length == PIN_SIZE) {
        psa_key_derivation_operation_t operation = PSA_KEY_DERIVATION_OPERATION_INIT;
        psa_status_t status = psa_crypto_init();
        if (status == PSA_SUCCESS)
            status = psa_key_derivation_setup(&operation, PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256));
        /* PSA requires the PBKDF2 cost, salt, then password inputs. */
        if (status == PSA_SUCCESS)
            status = psa_key_derivation_input_integer(&operation, PSA_KEY_DERIVATION_INPUT_COST,
                                                      PIN_ITERATIONS);
        if (status == PSA_SUCCESS)
            status = psa_key_derivation_input_bytes(&operation, PSA_KEY_DERIVATION_INPUT_SALT, salt,
                                                    sizeof(salt));
        if (status == PSA_SUCCESS)
            status = psa_key_derivation_input_bytes(&operation, PSA_KEY_DERIVATION_INPUT_PASSWORD,
                                                    (const uint8_t *)pin, length);
        if (status == PSA_SUCCESS)
            status = psa_key_derivation_output_bytes(&operation, derived, sizeof(derived));
        psa_key_derivation_abort(&operation);
        if (status == PSA_SUCCESS) {
            /* Compare every byte so the first mismatch does not end the check. */
            unsigned char diff = 0;
            for (size_t i = 0; i < sizeof(derived); i++)
                diff |= (unsigned char)(derived[i] ^ verifier[i]);
            matches = diff == 0;
        }
    }
    if (matches)
        failures = 0;
    else if (++failures >= 5) {
        failures = 0;
        locked_until_ticks = now + pdMS_TO_TICKS(LOCKOUT_MS);
    }
    xSemaphoreGive(auth_mutex);
    mbedtls_platform_zeroize(derived, sizeof(derived));
    return matches;
}
