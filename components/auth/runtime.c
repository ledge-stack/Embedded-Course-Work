#include "auth/runtime.h"
#include "mbedtls/platform_util.h"
#include "psa/crypto.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

#define PIN_ITERATIONS 10000
#define PIN_SIZE 4
#define LOCKOUT_MS 60000u

static const unsigned char salt[16]={
    0x4c,0x65,0x6f,0x6e,0x2d,0x65,0x6e,0x74,
    0x72,0x61,0x6e,0x63,0x65,0x2d,0x76,0x31
};
static const unsigned char verifier[32]={
    0x7c,0x4a,0x53,0x29,0x94,0x3e,0xd5,0xae,
    0xe1,0x68,0xfe,0xd5,0x5d,0x80,0x98,0xfe,
    0xfc,0x4a,0xc0,0x32,0xee,0xdb,0xd8,0x14,
    0x62,0x16,0x13,0xa5,0x0c,0xb1,0xf2,0x6d
};
static SemaphoreHandle_t auth_mutex;
static unsigned failures;
static TickType_t locked_until_ticks;

void auth_runtime_init(void) {
    if(!auth_mutex) auth_mutex=xSemaphoreCreateMutex();
}

bool auth_runtime_verify_pin(const char *pin) {
    if(!auth_mutex || !pin)return false;
    size_t length=strlen(pin);
    unsigned char derived[32]={0};
    bool matches=false;
    xSemaphoreTake(auth_mutex,portMAX_DELAY);
    TickType_t now=xTaskGetTickCount();
    if((int32_t)(now-locked_until_ticks)<0) { xSemaphoreGive(auth_mutex);return false; }
    if(length==PIN_SIZE) {
        psa_key_derivation_operation_t operation=PSA_KEY_DERIVATION_OPERATION_INIT;
        psa_status_t status=psa_crypto_init();
        if(status==PSA_SUCCESS)
            status=psa_key_derivation_setup(&operation,PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256));
        /* PSA requires the PBKDF2 cost, salt, then password inputs. */
        if(status==PSA_SUCCESS)
            status=psa_key_derivation_input_integer(&operation,PSA_KEY_DERIVATION_INPUT_COST,
                PIN_ITERATIONS);
        if(status==PSA_SUCCESS)
            status=psa_key_derivation_input_bytes(&operation,PSA_KEY_DERIVATION_INPUT_SALT,
                salt,sizeof(salt));
        if(status==PSA_SUCCESS)
            status=psa_key_derivation_input_bytes(&operation,PSA_KEY_DERIVATION_INPUT_PASSWORD,
                (const uint8_t *)pin,length);
        if(status==PSA_SUCCESS)
            status=psa_key_derivation_output_bytes(&operation,derived,sizeof(derived));
        psa_key_derivation_abort(&operation);
        if(status==PSA_SUCCESS) {
            unsigned char diff=0;
            for(size_t i=0;i<sizeof(derived);i++) diff|=(unsigned char)(derived[i]^verifier[i]);
            matches=diff==0;
        }
    }
    if(matches) failures=0;
    else if(++failures>=5) {
        failures=0;
        locked_until_ticks=now+pdMS_TO_TICKS(LOCKOUT_MS);
    }
    xSemaphoreGive(auth_mutex);
    mbedtls_platform_zeroize(derived,sizeof(derived));
    return matches;
}
