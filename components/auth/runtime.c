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
        unsigned char u[32]={0},next[32]={0},block[4]={0,0,0,1};
        unsigned char first[sizeof(salt)+sizeof(block)];
        memcpy(first,salt,sizeof(salt));memcpy(first+sizeof(salt),block,sizeof(block));
        psa_key_attributes_t attributes=PSA_KEY_ATTRIBUTES_INIT;
        psa_set_key_type(&attributes,PSA_KEY_TYPE_HMAC);
        psa_set_key_bits(&attributes,(psa_key_bits_t)(length*8u));
        psa_set_key_usage_flags(&attributes,PSA_KEY_USAGE_SIGN_MESSAGE);
        psa_set_key_algorithm(&attributes,PSA_ALG_HMAC(PSA_ALG_SHA_256));
        psa_key_id_t key=0;size_t mac_size=0;
        if(psa_crypto_init()==PSA_SUCCESS &&
           psa_import_key(&attributes,(const uint8_t *)pin,length,&key)==PSA_SUCCESS &&
           psa_mac_compute(key,PSA_ALG_HMAC(PSA_ALG_SHA_256),first,sizeof(first),u,sizeof(u),&mac_size)==PSA_SUCCESS &&
           mac_size==sizeof(u)) {
            memcpy(derived,u,sizeof(u));
            for(unsigned iteration=1;iteration<PIN_ITERATIONS;iteration++) {
                if(psa_mac_compute(key,PSA_ALG_HMAC(PSA_ALG_SHA_256),u,sizeof(u),next,sizeof(next),&mac_size)!=PSA_SUCCESS ||
                   mac_size!=sizeof(u)) { memset(derived,0,sizeof(derived));break; }
                memcpy(u,next,sizeof(u));
                for(size_t i=0;i<sizeof(derived);i++)derived[i]^=u[i];
            }
            unsigned char diff=0;
            for(size_t i=0;i<sizeof(derived);i++) diff|=(unsigned char)(derived[i]^verifier[i]);
            matches=diff==0;
        }
        if(key)psa_destroy_key(key);
        psa_reset_key_attributes(&attributes);
        mbedtls_platform_zeroize(u,sizeof(u));
        mbedtls_platform_zeroize(next,sizeof(next));
        mbedtls_platform_zeroize(first,sizeof(first));
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
