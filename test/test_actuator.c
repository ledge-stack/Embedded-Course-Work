#include "actuator/runtime.h"
#include "driver/gpio.h"
#include <assert.h>
#include <stdio.h>
#include <windows.h>

int main(void) {
    stub_gpio_input(32, 1);
    assert(actuator_runtime_start() == ESP_OK);
    assert(!gpio_get_level(32) && !gpio_get_level(12));
    puts("PASS both door indicators start closed");
    actuator_runtime_signal(SYSTEM_ENTRY, false);
    actuator_runtime_signal(SYSTEM_EXIT, false);
    assert(!gpio_get_level(32) && !gpio_get_level(12));
    actuator_runtime_signal(SYSTEM_ENTRY, true);
    assert(gpio_get_level(32) && !gpio_get_level(12));
    Sleep(1000);
    actuator_runtime_signal(SYSTEM_EXIT, true);
    assert(gpio_get_level(32) && gpio_get_level(12));
    Sleep(2100);
    assert(!gpio_get_level(32) && gpio_get_level(12));
    Sleep(1100);
    assert(!gpio_get_level(32) && !gpio_get_level(12));
    puts("PASS independent three-second entry and exit pulses; no polling call required");
    actuator_runtime_signal(SYSTEM_ENTRY, true);
    actuator_runtime_signal(SYSTEM_EXIT, true);
    actuator_runtime_close();
    assert(!gpio_get_level(32) && !gpio_get_level(12));
    puts("PASS immediate closure when access hours close");
    return 0;
}
