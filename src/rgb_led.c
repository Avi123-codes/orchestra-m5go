#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt.h"
#include "esp_log.h"
#include "orchestra.h"

static const char *TAG = "RGB_LED";

// M5GO has SK6812 RGB LEDs
#define RGB_LED_PIN     15
#define NUM_LEDS        10  // M5GO has 10 RGB LEDs on the sides
#define RMT_TX_CHANNEL  RMT_CHANNEL_0

// SK6812 timing (similar to WS2812)
#define T0H_NS  300
#define T0L_NS  900
#define T1H_NS  600
#define T1L_NS  600
#define RESET_US 80

// Convert RGB to GRB for SK6812
#define RGB_TO_GRB(r, g, b) ((g << 16) | (r << 8) | b)

static uint32_t led_colors[NUM_LEDS];
static rmt_item32_t led_data[NUM_LEDS * 24 + 1];  // 24 bits per LED + reset

// Convert byte to RMT items
static void byte_to_rmt(uint8_t byte, rmt_item32_t *item) {
    for (int i = 7; i >= 0; i--) {
        if (byte & (1 << i)) {
            // Send 1
            item->level0 = 1;
            item->duration0 = T1H_NS / 10 * RMT_DEFAULT_CLK_DIV / 1000;
            item->level1 = 0;
            item->duration1 = T1L_NS / 10 * RMT_DEFAULT_CLK_DIV / 1000;
        } else {
            // Send 0
            item->level0 = 1;
            item->duration0 = T0H_NS / 10 * RMT_DEFAULT_CLK_DIV / 1000;
            item->level1 = 0;
            item->duration1 = T0L_NS / 10 * RMT_DEFAULT_CLK_DIV / 1000;
        }
        item++;
    }
}

// Update LED strip
static void rgb_update(void) {
    int idx = 0;

    // Convert colors to RMT items
    for (int i = 0; i < NUM_LEDS; i++) {
        uint32_t grb = led_colors[i];
        byte_to_rmt((grb >> 16) & 0xFF, &led_data[idx]);  // Green
        idx += 8;
        byte_to_rmt((grb >> 8) & 0xFF, &led_data[idx]);   // Red
        idx += 8;
        byte_to_rmt(grb & 0xFF, &led_data[idx]);          // Blue
        idx += 8;
    }

    // Add reset signal
    led_data[idx].level0 = 0;
    led_data[idx].duration0 = RESET_US * 10 * RMT_DEFAULT_CLK_DIV;
    led_data[idx].level1 = 0;
    led_data[idx].duration1 = 0;

    // Send data
    ESP_ERROR_CHECK(rmt_write_items(RMT_TX_CHANNEL, led_data, idx + 1, true));
}

// Forward declaration
void rgb_set_all_color(uint32_t color);

// Initialize RGB LED system
void rgb_init(void) {
    rmt_config_t config = {
        .rmt_mode = RMT_MODE_TX,
        .channel = RMT_TX_CHANNEL,
        .gpio_num = RGB_LED_PIN,
        .clk_div = RMT_DEFAULT_CLK_DIV,
        .mem_block_num = 1,
        .tx_config = {
            .carrier_freq_hz = 0,
            .carrier_level = RMT_CARRIER_LEVEL_LOW,
            .idle_level = RMT_IDLE_LEVEL_LOW,
            .carrier_en = false,
            .loop_en = false,
            .idle_output_en = true,
        }
    };

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    // Set all LEDs to idle color (blue)
    rgb_set_all_color(COLOR_IDLE);

    ESP_LOGI(TAG, "RGB LED system initialized");
}

// Set all LEDs to the same color
void rgb_set_all_color(uint32_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    uint32_t grb = RGB_TO_GRB(r, g, b);

    for (int i = 0; i < NUM_LEDS; i++) {
        led_colors[i] = grb;
    }

    rgb_update();
    ESP_LOGI(TAG, "Set all LEDs to color 0x%06lX", (unsigned long)color);
}

// Set individual LED color
void rgb_set_led_color(uint8_t led_num, uint32_t color) {
    if (led_num >= NUM_LEDS) {
        ESP_LOGW(TAG, "Invalid LED number: %d", led_num);
        return;
    }

    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    led_colors[led_num] = RGB_TO_GRB(r, g, b);

    rgb_update();
}

// Create a breathing effect
void rgb_breathing_effect(uint32_t color, uint32_t duration_ms) {
    uint8_t base_r = (color >> 16) & 0xFF;
    uint8_t base_g = (color >> 8) & 0xFF;
    uint8_t base_b = color & 0xFF;

    const int steps = 50;
    const int step_delay = duration_ms / (steps * 2);

    // Fade in
    for (int i = 0; i <= steps; i++) {
        float factor = (float)i / steps;
        uint8_t r = base_r * factor;
        uint8_t g = base_g * factor;
        uint8_t b = base_b * factor;
        uint32_t current_color = (r << 16) | (g << 8) | b;
        rgb_set_all_color(current_color);
        vTaskDelay(pdMS_TO_TICKS(step_delay));
    }

    // Fade out
    for (int i = steps; i >= 0; i--) {
        float factor = (float)i / steps;
        uint8_t r = base_r * factor;
        uint8_t g = base_g * factor;
        uint8_t b = base_b * factor;
        uint32_t current_color = (r << 16) | (g << 8) | b;
        rgb_set_all_color(current_color);
        vTaskDelay(pdMS_TO_TICKS(step_delay));
    }
}