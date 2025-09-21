#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "orchestra.h"

// M5Stack display is 320x240 ILI9342C
#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 240

// Display SPI pins (M5Stack Core)
#define LCD_MOSI_PIN   23
#define LCD_CLK_PIN    18
#define LCD_CS_PIN     14
#define LCD_DC_PIN     27
#define LCD_RST_PIN    33
#define LCD_BL_PIN     32

static const char *TAG = "DISPLAY";
static spi_device_handle_t spi;
static bool animation_running = false;
static song_type_t current_song_type = SONG_TYPE_SOLO;

// Simple LCD command function (placeholder - needs full ILI9342C implementation)
__attribute__((unused))
static void lcd_cmd(uint8_t cmd) {
    gpio_set_level(LCD_DC_PIN, 0);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_polling_transmit(spi, &t);
}

__attribute__((unused))
static void lcd_data(uint8_t data) {
    gpio_set_level(LCD_DC_PIN, 1);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data,
    };
    spi_device_polling_transmit(spi, &t);
}

// Initialize display
static void display_init_hardware(void) {
    // Configure GPIO pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LCD_DC_PIN) | (1ULL << LCD_RST_PIN) | (1ULL << LCD_BL_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    // Reset display
    gpio_set_level(LCD_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LCD_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Configure SPI
    spi_bus_config_t buscfg = {
        .mosi_io_num = LCD_MOSI_PIN,
        .miso_io_num = -1,
        .sclk_io_num = LCD_CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2,
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = LCD_CS_PIN,
        .queue_size = 1,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfg, &spi));

    // Turn on backlight
    gpio_set_level(LCD_BL_PIN, 1);

    ESP_LOGI(TAG, "Display hardware initialized");
}

// Draw a filled rectangle (simplified)
static void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // This is a placeholder - needs full implementation
    // Would send commands to set window and write pixel data
}

// Draw animation frame based on song type
static void draw_animation_frame(uint32_t frame_num) {
    // This function is simplified - actual implementation in display_animations.c
    // Just draw basic rectangles for now

    switch (current_song_type) {
        case SONG_TYPE_QUINTET:
            // Draw green rectangle
            draw_rect(10, 10, 50, 50, 0x07E0); // Green in RGB565
            break;

        case SONG_TYPE_DUET:
            // Draw yellow rectangles
            for (int x = 0; x < DISPLAY_WIDTH; x += 20) {
                uint16_t y1 = DISPLAY_HEIGHT / 3 + (int)(sin((x + frame_num * 5) * 0.05) * 30);
                draw_rect(x, y1, 3, 3, 0xFFE0); // Yellow in RGB565
            }
            break;

        case SONG_TYPE_SOLO:
            // Draw purple rectangle
            draw_rect(DISPLAY_WIDTH/2 - 25, DISPLAY_HEIGHT/2 - 25, 50, 50, 0xC81F); // Purple in RGB565
            break;
    }
}

// Animation task
static void animation_task(void *pvParameters) {
    uint32_t frame = 0;

    while (1) {
        if (animation_running) {
            draw_animation_frame(frame++);
            vTaskDelay(pdMS_TO_TICKS(50)); // 20 FPS
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// Initialize display system
void display_init(void) {
    display_init_hardware();

    // Start animation task
    xTaskCreate(animation_task, "animation", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Display system initialized");
}

// Start animation for a song type
void display_start_animation(song_type_t type) {
    current_song_type = type;
    animation_running = true;
    ESP_LOGI(TAG, "Started animation for song type %d", type);
}

// Stop animation
void display_stop_animation(void) {
    animation_running = false;
    ESP_LOGI(TAG, "Stopped animation");
}

// Show idle screen
void display_idle(void) {
    animation_running = false;
    // Clear screen and show Tinkercademy logo
    // This would need actual image data and drawing implementation
}