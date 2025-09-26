// src/display_animations.c
// Minimal, low-RAM display animations with role-colored equalizer
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "display_animations.h"
#include "device_config.h"

#ifndef DISPLAY_WIDTH
#define DISPLAY_WIDTH  320
#endif
#ifndef DISPLAY_HEIGHT
#define DISPLAY_HEIGHT 240
#endif

static const char *TAG = "DISPLAY_ANIM";

// Minimal animation context
static animation_context_t anim_ctx;
static SemaphoreHandle_t anim_mutex = NULL;

// One scanline buffer reused for pushes
static uint16_t scanline_buf[DISPLAY_WIDTH];

// Weak drawing hooks — implemented in display.c
__attribute__((weak)) void display_begin_frame(uint16_t w, uint16_t h) {(void)w;(void)h;}
__attribute__((weak)) void display_push_row(uint16_t y, const uint16_t *row, uint16_t w) {(void)y;(void)row;(void)w;}
__attribute__((weak)) void display_end_frame(void) {(void)0;}
__attribute__((weak)) void display_push_framebuffer(const uint16_t *fb, uint16_t w, uint16_t h) {(void)fb;(void)w;(void)h;}

// Cheap RGB565 helper
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3));
}

// Role color mapping (tuned to pop against dark bg during animation)
static inline void role_base_color(device_role_t role, uint8_t *r, uint8_t *g, uint8_t *b) {
    switch (role) {
        case ROLE_PART_1:    *r =  40; *g = 255; *b =  40; break;  // Green
        case ROLE_PART_2:    *r = 255; *g = 255; *b =  60; break;  // Yellow
        case ROLE_PART_3:    *r =  60; *g = 210; *b = 255; break;  // Cyan/Blue
        case ROLE_PART_4:    *r = 255; *g = 140; *b =  60; break;  // Orange
        case ROLE_CONDUCTOR: *r = 190; *g =  60; *b = 190; break;  // Purple
        default:             *r = 180; *g = 120; *b = 200; break;  // Fallback
    }
}

// Render one equalizer frame; color & intensity derive from (fixed) role + beat
static void render_equalizer_frame(uint32_t frame)
{
    const int bars = 12;
    const int gap = 2;
    const int bar_w = (DISPLAY_WIDTH - (bars + 1) * gap) / bars;

    // Snapshot shared state once
    float beat_f;
    device_role_t role;
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    beat_f = anim_ctx.beat_intensity;
    role   = anim_ctx.device_role;  // fixed at init
    xSemaphoreGive(anim_mutex);

    // Beat in 0..1000 fixed-point
    uint32_t base_i = (beat_f <= 0.0f) ? 0u : (beat_f >= 1.0f ? 1000u : (uint32_t)(beat_f * 1000.0f + 0.5f));

    // Base color per role
    uint8_t base_r, base_g, base_b;
    role_base_color(role, &base_r, &base_g, &base_b);

    display_begin_frame(DISPLAY_WIDTH, DISPLAY_HEIGHT);

    for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
        // Dark background for EQ (separate from idle blue)
        uint16_t bg = rgb565(10, 10, 30);
        for (int x = 0; x < DISPLAY_WIDTH; ++x) scanline_buf[x] = bg;

        for (int b = 0; b < bars; ++b) {
            // small variance for motion
            uint32_t variance_percent = (uint32_t)((b * 37 + (int)frame) % 101);
            uint32_t multiplier = 500 + (variance_percent * 150) / 100;   // 0.50 .. 0.65
            uint32_t level_percent = (base_i * multiplier) / 1000;        // 0 .. 650

            int height = (int)((level_percent * DISPLAY_HEIGHT) / 1000);
            int bx = gap + b * (bar_w + gap);
            int bar_top = DISPLAY_HEIGHT - height;

            if (y >= bar_top) {
                uint32_t tint_q = 850 + (300 * b) / (bars ? bars : 1);     // 0.85 .. 1.15 (scaled 1000)
                uint32_t temp   = 350 + (650 * level_percent) / 1000;      // 0.35 .. 1.00 (scaled 1000)

                uint32_t comp_r = (uint32_t)base_r * temp * tint_q / 1000000u;
                uint32_t comp_g = (uint32_t)base_g * temp * tint_q / 1000000u;
                uint32_t comp_b = (uint32_t)base_b * temp * tint_q / 1000000u;
                if (comp_r > 255) comp_r = 255;
                if (comp_g > 255) comp_g = 255;
                if (comp_b > 255) comp_b = 255;

                uint16_t col = rgb565((uint8_t)comp_r, (uint8_t)comp_g, (uint8_t)comp_b);
                for (int px = 0; px < bar_w; ++px) {
                    int x = bx + px;
                    if ((unsigned)x < DISPLAY_WIDTH) scanline_buf[x] = col;
                }
            }
        }

        display_push_row(y, scanline_buf, DISPLAY_WIDTH);
        if ((y & 7) == 0) vTaskDelay(0);
    }

    display_end_frame();
}

static void animation_task(void *arg)
{
    (void)arg;
    uint32_t frame = 0;
    const TickType_t frame_dt = pdMS_TO_TICKS(40); // ~25 FPS

    while (1) {
        bool active;
        xSemaphoreTake(anim_mutex, portMAX_DELAY);
        active = anim_ctx.active;
        xSemaphoreGive(anim_mutex);

        if (!active) {
            // Idle: full-screen solid BLUE once per loop (no flicker)
            uint16_t c = rgb565(0, 0, 200); // bright-ish blue
            display_begin_frame(DISPLAY_WIDTH, DISPLAY_HEIGHT);
            for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
                for (int x = 0; x < DISPLAY_WIDTH; ++x) scanline_buf[x] = c;
                display_push_row(y, scanline_buf, DISPLAY_WIDTH);
                if ((y & 7) == 0) vTaskDelay(0);
            }
            display_end_frame();
            // Sleep a bit so we don't keep repainting when idle
            vTaskDelay(pdMS_TO_TICKS(250));
        } else {
            render_equalizer_frame(frame);
            frame++;
            vTaskDelay(frame_dt);
        }
    }
}

void display_animations_init(void)
{
    memset(&anim_ctx, 0, sizeof(anim_ctx));
    anim_mutex = xSemaphoreCreateMutex();
    if (!anim_mutex) {
        ESP_LOGE(TAG, "Failed to create anim mutex");
        return;
    }

    // Capture device role ONCE
    anim_ctx.device_role    = device_config_get_role();
    anim_ctx.active         = false;            // start idle (blue)
    anim_ctx.song_type      = SONG_TYPE_SOLO;
    anim_ctx.beat_intensity = 0.0f;

    xTaskCreate(animation_task, "animation_task", 2048, NULL, 3, NULL);
    ESP_LOGI(TAG, "Display animations initialized; role=%d", (int)anim_ctx.device_role);
}

void display_animations_start_idle(void)
{
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    anim_ctx.active = false;
    anim_ctx.beat_intensity = 0.0f;
    xSemaphoreGive(anim_mutex);
    ESP_LOGI(TAG, "Animations: start idle (blue)");
}

void display_animations_start_playback(song_type_t song_type)
{
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    anim_ctx.active = true;
    anim_ctx.song_type = song_type;
    anim_ctx.beat_intensity = 0.0f;
    xSemaphoreGive(anim_mutex);
    ESP_LOGI(TAG, "Animations: start playback (type=%d)", (int)song_type);
}

void display_animations_stop(void)
{
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    anim_ctx.active = false;
    xSemaphoreGive(anim_mutex);
    ESP_LOGI(TAG, "Animations: stop");
}

void display_animations_update_beat(float intensity)
{
    if (intensity < 0.f) intensity = 0.f;
    if (intensity > 1.f) intensity = 1.f;
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    anim_ctx.beat_intensity = intensity;
    xSemaphoreGive(anim_mutex);
}
