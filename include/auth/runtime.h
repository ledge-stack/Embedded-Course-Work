#ifndef AUTH_RUNTIME_H
#define AUTH_RUNTIME_H

#include <stdbool.h>

void auth_runtime_init(void);
/* Shared by the entrance keypad and Wi-Fi dashboard. The demo verifier is
 * PBKDF2-HMAC-SHA256, never a plaintext PIN. */
bool auth_runtime_verify_pin(const char *pin);

#endif
