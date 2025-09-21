#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "orchestra.h"
#include "songs.h"

static const char *TAG = "ORCHESTRA";

// External function declarations
extern void audio_init(void);
extern void audio_play_song(uint8_t song_id);
extern void audio_stop(void);
extern void audio_set_volume(float vol);
extern bool audio_is_playing(void);

extern void rgb_init(void);
extern void rgb_set_all_color(uint32_t color);
extern void rgb_breathing_effect(uint32_t color, uint32_t duration_ms);

extern void display_init(void);
extern void display_animations_init(void);
extern void display_animations_start_idle(void);
extern void display_animations_start_playback(song_type_t song_type);
extern void display_animations_stop(void);
extern void display_animations_update_beat(float intensity);

extern esp_err_t espnow_init(uint8_t id);
extern esp_err_t espnow_broadcast(msg_type_t type, uint8_t song_id);

// Button GPIO pins (M5Stack Core)
#define BUTTON_A_PIN    39
#define BUTTON_B_PIN    38
#define BUTTON_C_PIN    37

// Device configuration
static uint8_t device_id = 0;  // Default device ID, can be set via GPIO or NVS
static bool is_playing = false;
static SemaphoreHandle_t orchestra_mutex;

// Song rotation for buttons
static const uint8_t button_a_songs[] = {SONG_JUPITER_HYMN, SONG_CARNIVAL_THEME};
static const uint8_t button_b_songs[] = {SONG_CANON_IN_D, SONG_CARNIVAL_VAR1, SONG_MEDALLION_CALLS};
static const uint8_t button_c_songs[] = {SONG_BLUE_BELLS, SONG_TV_TIME};

static uint8_t button_a_index = 0;
static uint8_t button_b_index = 0;
static uint8_t button_c_index = 0;

// Get device ID from GPIO pins (can be set with jumpers)
static uint8_t get_device_id(void) {
    // Could read from GPIO pins or NVS
    // For now, return default
    return device_id;
}

// Button interrupt handler
static void IRAM_ATTR button_isr_handler(void *arg) {
    uint32_t button_num = (uint32_t)arg;
    xTaskNotifyFromISR(NULL, button_num, eSetValueWithOverwrite, NULL);
}

// Button task to handle button presses
static void button_task(void *pvParameters) {
    uint32_t button_notification;
    TickType_t last_press_time[3] = {0, 0, 0};
    const TickType_t debounce_time = pdMS_TO_TICKS(200);

    while (1) {
        if (xTaskNotifyWait(0, ULONG_MAX, &button_notification, portMAX_DELAY) == pdTRUE) {
            TickType_t current_time = xTaskGetTickCount();

            // Debounce check
            if (current_time - last_press_time[button_notification] < debounce_time) {
                continue;
            }
            last_press_time[button_notification] = current_time;

            switch (button_notification) {
                case 0:  // Button A
                    orchestra_handle_button_a();
                    break;
                case 1:  // Button B
                    orchestra_handle_button_b();
                    break;
                case 2:  // Button C
                    orchestra_handle_button_c();
                    break;
            }
        }
    }
}

// Initialize button inputs
static void init_buttons(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_A_PIN) | (1ULL << BUTTON_B_PIN) | (1ULL << BUTTON_C_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);

    // Install ISR service
    gpio_install_isr_service(0);

    // Hook ISR handlers
    gpio_isr_handler_add(BUTTON_A_PIN, button_isr_handler, (void *)0);
    gpio_isr_handler_add(BUTTON_B_PIN, button_isr_handler, (void *)1);
    gpio_isr_handler_add(BUTTON_C_PIN, button_isr_handler, (void *)2);

    // Create button handling task
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "Buttons initialized");
}

// Initialize the orchestra system
void orchestra_init(void) {
    ESP_LOGI(TAG, "Initializing Orchestra M5GO...");

    // Create mutex
    orchestra_mutex = xSemaphoreCreateMutex();

    // Get device ID
    device_id = get_device_id();
    ESP_LOGI(TAG, "Device ID: %d", device_id);

    // Initialize subsystems
    audio_init();
    rgb_init();
    display_init();
    display_animations_init();
    espnow_init(device_id);
    init_buttons();

    // Set default volume
    audio_set_volume(0.15f);

    // Show idle state with animations
    display_animations_start_idle();
    rgb_set_all_color(COLOR_IDLE);

    ESP_LOGI(TAG, "Orchestra M5GO initialized successfully");
}

// Play a song
void orchestra_play_song(uint8_t song_id) {
    if (song_id >= total_songs) {
        ESP_LOGW(TAG, "Invalid song ID: %d", song_id);
        return;
    }

    xSemaphoreTake(orchestra_mutex, portMAX_DELAY);

    if (is_playing) {
        orchestra_stop();
    }

    const song_t *song = &songs[song_id];
    ESP_LOGI(TAG, "Playing song: %s (type: %d)", song->name, song->type);

    // Set LED color based on song type
    uint32_t led_color;
    switch (song->type) {
        case SONG_TYPE_QUINTET:
            led_color = COLOR_QUINTET;
            break;
        case SONG_TYPE_DUET:
            led_color = COLOR_DUET;
            break;
        case SONG_TYPE_SOLO:
            led_color = COLOR_SOLO;
            break;
        default:
            led_color = COLOR_IDLE;
    }

    // Update LEDs and display
    rgb_set_all_color(led_color);
    display_animations_start_playback(song->type);

    // Check if this device should play this song
    bool should_play = false;

    if (song->type == SONG_TYPE_SOLO) {
        // All devices play solo songs
        should_play = true;
    } else if (song->type == SONG_TYPE_QUINTET) {
        // All devices play quintet
        should_play = true;
    } else if (song->type == SONG_TYPE_DUET) {
        // Check if this device is part of the duet
        if (song->parts_mask & (1 << device_id)) {
            should_play = true;
        }
    }

    if (should_play) {
        audio_play_song(song_id);
        is_playing = true;
    }

    xSemaphoreGive(orchestra_mutex);
}

// Stop playing
void orchestra_stop(void) {
    xSemaphoreTake(orchestra_mutex, portMAX_DELAY);

    audio_stop();
    display_animations_stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    display_animations_start_idle();
    rgb_set_all_color(COLOR_IDLE);
    is_playing = false;

    ESP_LOGI(TAG, "Playback stopped");

    xSemaphoreGive(orchestra_mutex);
}

// Set volume
void orchestra_set_volume(float volume) {
    audio_set_volume(volume);
}

// Handle Button A press
void orchestra_handle_button_a(void) {
    ESP_LOGI(TAG, "Button A pressed");

    // Cycle through button A songs
    uint8_t song_id = button_a_songs[button_a_index];
    button_a_index = (button_a_index + 1) % (sizeof(button_a_songs) / sizeof(button_a_songs[0]));

    // Broadcast to all devices to play this song
    espnow_broadcast(MSG_SYNC_START, song_id);

    // Play locally as well
    orchestra_play_song(song_id);
}

// Handle Button B press
void orchestra_handle_button_b(void) {
    ESP_LOGI(TAG, "Button B pressed");

    // Cycle through button B songs
    uint8_t song_id = button_b_songs[button_b_index];
    button_b_index = (button_b_index + 1) % (sizeof(button_b_songs) / sizeof(button_b_songs[0]));

    // Broadcast to all devices to play this song
    espnow_broadcast(MSG_SYNC_START, song_id);

    // Play locally as well
    orchestra_play_song(song_id);
}

// Handle Button C press
void orchestra_handle_button_c(void) {
    ESP_LOGI(TAG, "Button C pressed");

    // Cycle through button C songs
    uint8_t song_id = button_c_songs[button_c_index];
    button_c_index = (button_c_index + 1) % (sizeof(button_c_songs) / sizeof(button_c_songs[0]));

    // Broadcast to all devices to play this song
    espnow_broadcast(MSG_SYNC_START, song_id);

    // Play locally as well
    orchestra_play_song(song_id);
}