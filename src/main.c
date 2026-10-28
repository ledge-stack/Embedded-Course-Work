#include "trevor/runtime.h"
#include "entrance/runtime.h"

void app_main(void) {
    ESP_ERROR_CHECK(tr_runtime_start());
    ESP_ERROR_CHECK(entrance_runtime_start());
}
