#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "orchestra.h"

static const char *TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "Orchestra M5GO Starting...");

    // Initialize orchestra system
    orchestra_init();

    ESP_LOGI(TAG, "Orchestra M5GO Ready!");

    // Main loop - the system is event-driven
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}