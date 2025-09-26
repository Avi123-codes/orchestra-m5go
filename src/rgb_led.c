#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt.h"
#include "esp_log.h"
#include "orchestra.h"

static const char *TAG = "RGB_LED";

// --- Hardware ---
#define RGB_LED_PIN     15
#define NUM_LEDS        10   // M5GO has 10 side LEDs
#define RMT_TX_CHANNEL  RMT_CHANNEL_0

// --- Timing for SK6812 / WS2812 style LEDs ---
#define T0H_NS   300   // 0-bit high time
#define T0L_NS   900   // 0-bit low time
#define T1H_NS   600   // 1-bit high time
#define T1L_NS   600   // 1-bit low time
#define RESET_US 80    // Reset time in µs

#ifndef RMT_DEFAULT_CLK_DIV
#define RMT_DEFAULT_CLK_DIV 2   // 80 MHz base → 25 ns per tick when div=2
#endif

// Convert RGB to GRB for SK6812
#define RGB_TO_GRB(r, g, b) ((uint32_t)((g << 16) | (r << 8) | (b)))

// Forward declarations (avoid implicit-declaration errors)
static void rgb_update(void);
void rgb_set_all_color(uint32_t color);
void rgb_set_led_color(uint8_t led_num, uint32_t color);
void rgb_breathing_effect(uint32_t color, uint32_t duration_ms);

static uint32_t     led_colors[NUM_LEDS];
static rmt_item32_t led_data[NUM_LEDS * 24 + 1];  // 24 bits/LED + reset

// ---- Helpers to convert timing ----
static inline uint16_t rmt_ticks_from_ns(uint32_t ns, uint8_t clk_div) {
    // APB = 80 MHz → 12.5 ns per tick at div=1
    // duration (ticks) = ns / (12.5 ns * div) = (ns * 80) / (1000 * div)
    uint32_t ticks = (ns * 80U) / (clk_div * 1000U);
    if (ticks > 0x7FFF) ticks = 0x7FFF; // limit to 15 bits
    return (uint16_t)ticks;
}
static inline uint16_t rmt_ticks_from_us(uint32_t us, uint8_t clk_div) {
    // 1 µs = 80 ticks at div=1
    uint32_t ticks = (us * 80U) / clk_div;
    if (ticks > 0x7FFF) ticks = 0x7FFF; // limit to 15 bits
    return (uint16_t)ticks;
}

// ---- Encode a byte into 8 RMT items ----
static void byte_to_rmt(uint8_t byte, rmt_item32_t *item) {
    for (int i = 7; i >= 0; i--) {
        if (byte & (1 << i)) {
            // Bit '1'
            item->level0    = 1;
            item->duration0 = rmt_ticks_from_ns(T1H_NS, RMT_DEFAULT_CLK_DIV);
            item->level1    = 0;
            item->duration1 = rmt_ticks_from_ns(T1L_NS, RMT_DEFAULT_CLK_DIV);
        } else {
            // Bit '0'
            item->level0    = 1;
            item->duration0 = rmt_ticks_from_ns(T0H_NS, RMT_DEFAULT_CLK_DIV);
            item->level1    = 0;
            item->duration1 = rmt_ticks_from_ns(T0L_NS, RMT_DEFAULT_CLK_DIV);
        }
        item++;
    }
}

// ---- Push all LED colors ----
static void rgb_update(void) {
    int idx = 0;
    for (int i = 0; i < NUM_LEDS; i++) {
        uint32_t grb = led_colors[i];
        byte_to_rmt((grb >> 16) & 0xFF, &led_data[idx]); idx += 8; // G
        byte_to_rmt((grb >>  8) & 0xFF, &led_data[idx]); idx += 8; // R
        byte_to_rmt((grb      ) & 0xFF, &led_data[idx]); idx += 8; // B
    }

    // Reset pulse (low)
    led_data[idx].level0    = 0;
    led_data[idx].duration0 = rmt_ticks_from_us(RESET_US, RMT_DEFAULT_CLK_DIV);
    led_data[idx].level1    = 0;
    led_data[idx].duration1 = 0;

    ESP_ERROR_CHECK(rmt_write_items(RMT_TX_CHANNEL, led_data, idx + 1, true));
}

// ---- Public API ----
void rgb_init(void) {
    rmt_config_t config = {
        .rmt_mode      = RMT_MODE_TX,
        .channel       = RMT_TX_CHANNEL,
        .gpio_num      = RGB_LED_PIN,
        .clk_div       = RMT_DEFAULT_CLK_DIV,
        .mem_block_num = 1,
        .tx_config = {
            .carrier_en       = false,
            .loop_en          = false,
            .idle_output_en   = true,
            .idle_level       = RMT_IDLE_LEVEL_LOW,
            .carrier_freq_hz  = 0,
            .carrier_level    = RMT_CARRIER_LEVEL_LOW,
            .carrier_duty_percent = 50,
        },
    };
    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    rgb_set_all_color(COLOR_IDLE);  // default on boot
    ESP_LOGI(TAG, "RGB LED system initialized");
}

void rgb_set_all_color(uint32_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8)  & 0xFF;
    uint8_t b =  color        & 0xFF;
    uint32_t grb = RGB_TO_GRB(r, g, b);

    for (int i = 0; i < NUM_LEDS; i++) led_colors[i] = grb;
    rgb_update();
}

void rgb_set_led_color(uint8_t led_num, uint32_t color) {
    if (led_num >= NUM_LEDS) return;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8)  & 0xFF;
    uint8_t b =  color        & 0xFF;
    led_colors[led_num] = RGB_TO_GRB(r, g, b);
    rgb_update();
}

void rgb_breathing_effect(uint32_t color, uint32_t duration_ms) {
    uint8_t r0 = (color >> 16) & 0xFF;
    uint8_t g0 = (color >> 8)  & 0xFF;
    uint8_t b0 =  color        & 0xFF;
    const int steps = 50;
    int step_delay = duration_ms / (steps * 2);

    for (int i = 0; i <= steps; i++) {
        float f = (float)i / steps;
        rgb_set_all_color(((uint8_t)(r0*f) << 16) |
                          ((uint8_t)(g0*f) << 8)  |
                          ((uint8_t)(b0*f)));
        vTaskDelay(pdMS_TO_TICKS(step_delay));
    }
    for (int i = steps; i >= 0; i--) {
        float f = (float)i / steps;
        rgb_set_all_color(((uint8_t)(r0*f) << 16) |
                          ((uint8_t)(g0*f) << 8)  |
                          ((uint8_t)(b0*f)));
        vTaskDelay(pdMS_TO_TICKS(step_delay));
    }
}
