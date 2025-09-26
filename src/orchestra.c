#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "orchestra.h"
#include "songs.h"
#include "espnow_discovery.h"

#include "device_config.h"   // <-- for device_config_get_role(), ROLE_*

static const char *TAG = "ORCHESTRA";

// External function declarations
extern void audio_init(void);
extern void audio_play_song(uint8_t song_id);
// New role-aware playback (implemented in audio.c)
extern void audio_play_song_for_role(uint8_t song_id, uint8_t role);
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

// Device / state
static uint8_t device_id = 0;
static bool is_playing = false;
static SemaphoreHandle_t orchestra_mutex;

// role cache
static device_role_t s_role = ROLE_UNKNOWN;
static bool s_is_conductor = false;

// button task handle so ISR can notify it
static TaskHandle_t s_btn_task_handle = NULL;

// Song rotation for buttons (used by conductor only)
static const uint8_t button_a_songs[] = {SONG_JUPITER_HYMN, SONG_CARNIVAL_THEME};
static const uint8_t button_b_songs[] = {SONG_CANON_IN_D, SONG_CARNIVAL_VAR1, SONG_MEDALLION_CALLS};
static const uint8_t button_c_songs[] = {SONG_BLUE_BELLS, SONG_TV_TIME};
static uint8_t button_a_index = 0;
static uint8_t button_b_index = 0;
static uint8_t button_c_index = 0;

// If you later want GPIO/NVS based IDs, wire them here
// (previously had a get_device_id helper that was unused; removed to avoid
// -Werror=unused-function build failures)


// -------- Buttons (conductor only) --------
static void IRAM_ATTR button_isr_handler(void *arg) {
    uint32_t btn = (uint32_t)arg; // 0=A,1=B,2=C
    BaseType_t hpw = pdFALSE;
    if (s_btn_task_handle) {
        xTaskNotifyFromISR(s_btn_task_handle, btn, eSetValueWithOverwrite, &hpw);
        if (hpw == pdTRUE) portYIELD_FROM_ISR();
    }
}

static void button_task(void *pvParameters) {
    uint32_t btn;
    TickType_t last_press[3] = {0,0,0};
    const TickType_t debounce = pdMS_TO_TICKS(200);

    while (1) {
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &btn, portMAX_DELAY) == pdTRUE) {
            TickType_t now = xTaskGetTickCount();
            if (btn < 3 && (now - last_press[btn] >= debounce)) {
                last_press[btn] = now;
                switch (btn) {
                    case 0: orchestra_handle_button_a(); break;
                    case 1: orchestra_handle_button_b(); break;
                    case 2: orchestra_handle_button_c(); break;
                    default: break;
                }
            }
        }
    }
}

static void init_buttons(void) {
    // NOTE: GPIOs 37/38/39 are input-only and have NO internal pull-ups.
    // M5Stack board provides external resistors. Keep pull-ups disabled.
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_A_PIN) | (1ULL << BUTTON_B_PIN) | (1ULL << BUTTON_C_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_A_PIN, button_isr_handler, (void*)0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_B_PIN, button_isr_handler, (void*)1));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_C_PIN, button_isr_handler, (void*)2));

    xTaskCreate(button_task, "button_task", 2048, NULL, 10, &s_btn_task_handle);
    ESP_LOGI(TAG, "Buttons initialized (conductor)");
}

// ------------- Public API -------------
void orchestra_init(void) {
    ESP_LOGI(TAG, "Initializing Orchestra…");

    orchestra_mutex = xSemaphoreCreateMutex();

    // Load role first; defaults to whatever your device_config picked
    s_role = device_config_get_role();
    s_is_conductor = (s_role == ROLE_CONDUCTOR);
    // Use role value as device_id to avoid collisions: ROLE_CONDUCTOR=0, ROLE_PART_1..4 = 1..4
    device_id = (uint8_t)s_role;
    ESP_LOGI(TAG, "Device role=%d (%s), id=%u",
             (int)s_role, device_config_get_role_name(s_role), device_id);

    // Subsystems
    audio_init();                  // safe on conductor; we just won't call play
    rgb_init();
    display_init();
    display_animations_init();
    ESP_ERROR_CHECK(espnow_init(device_id));

    // Only the conductor owns buttons; performers ignore local inputs
    if (s_is_conductor) {
        init_buttons();
    }

    // Default volume
    // Default volume (kept in sync with audio.c default)
    audio_set_volume(0.08f);

    // Idle visuals
    display_animations_start_idle();
    rgb_set_all_color(COLOR_IDLE);

    ESP_LOGI(TAG, "Orchestra initialized");
}

void orchestra_play_song(uint8_t song_id) {
    if (song_id >= total_songs) {
        ESP_LOGW(TAG, "Invalid song ID: %u", song_id);
        return;
    }

    xSemaphoreTake(orchestra_mutex, portMAX_DELAY);

    if (is_playing) {
        audio_stop();
        is_playing = false;
    }

    const song_t *song = &songs[song_id];
    ESP_LOGI(TAG, "Play request: %s (type=%d) role=%d",
             song->name, song->type, (int)s_role);

    // Log what part(s) this device will play for clarity
    if (!s_is_conductor) {
        uint8_t part_bit = (1U << device_id);
        ESP_LOGI(TAG, "Device %u will play parts matching mask 0x%02X (song parts_mask=0x%02X)",
                 device_id, part_bit, song->parts_mask);
    }

    // LED color per song type
    uint32_t led_color = COLOR_IDLE;
    switch (song->type) {
        case SONG_TYPE_QUINTET: led_color = COLOR_QUINTET; break;
        case SONG_TYPE_DUET:    led_color = COLOR_DUET;    break;
        case SONG_TYPE_SOLO:    led_color = COLOR_SOLO;    break;
        default: break;
    }

    rgb_set_all_color(led_color);
    display_animations_start_playback(song->type);
    // Ensure animations know our role so they pick the right colors
    display_animations_update_beat(0.0f);

    bool should_play = false;

    if (s_is_conductor) {
        // Conductor is silent. Never call audio_play_song() here.
        should_play = false;
        ESP_LOGI(TAG, "Conductor: visual-only, no audio output.");
    } else {
        // Performer: decide by song type / parts mask
        if (song->type == SONG_TYPE_QUINTET) {
            // All parts play
            should_play = true;
        } else {
            // SOLO and DUET respect the song's parts_mask. This lets the
            // conductor trigger specific parts on different devices.
            if (device_config_should_play_part(s_role, song->parts_mask)) {
                should_play = true;
            }
        }
    }

    if (should_play) {
        // Use role-aware playback so each performer produces a different part
        audio_play_song_for_role(song_id, (uint8_t)s_role);
        is_playing = true;
    }

    ESP_LOGI(TAG, "Playback decision: should_play=%d (role=%d)", (int)should_play, (int)s_role);

    xSemaphoreGive(orchestra_mutex);
}

void orchestra_stop(void) {
    xSemaphoreTake(orchestra_mutex, portMAX_DELAY);

    // Performers stop audio; conductor had none
    if (!s_is_conductor) {
        audio_stop();
    }

    display_animations_stop();
    vTaskDelay(pdMS_TO_TICKS(80));
    display_animations_start_idle();
    rgb_set_all_color(COLOR_IDLE);
    is_playing = false;

    ESP_LOGI(TAG, "Stopped");

    xSemaphoreGive(orchestra_mutex);
}

void orchestra_set_volume(float volume) {
    audio_set_volume(volume);
}

// -------- Button handlers (conductor only) --------
void orchestra_handle_button_a(void) {
    if (!s_is_conductor) return;  // performers ignore buttons
    ESP_LOGI(TAG, "Btn A");

    uint8_t song_id = button_a_songs[button_a_index];
    button_a_index = (button_a_index + 1) % (sizeof(button_a_songs)/sizeof(button_a_songs[0]));

    // Broadcast only; conductor stays silent locally
    espnow_broadcast(MSG_SYNC_START, song_id);

    // Optional: update visuals locally to reflect the selection/start
    const song_t *song = &songs[song_id];
    display_animations_start_playback(song->type);
}

void orchestra_handle_button_b(void) {
    if (!s_is_conductor) return;
    ESP_LOGI(TAG, "Btn B");

    uint8_t song_id = button_b_songs[button_b_index];
    button_b_index = (button_b_index + 1) % (sizeof(button_b_songs)/sizeof(button_b_songs[0]));

    espnow_broadcast(MSG_SYNC_START, song_id);

    const song_t *song = &songs[song_id];
    display_animations_start_playback(song->type);
}

void orchestra_handle_button_c(void) {
    if (!s_is_conductor) return;
    ESP_LOGI(TAG, "Btn C");

    uint8_t song_id = button_c_songs[button_c_index];
    button_c_index = (button_c_index + 1) % (sizeof(button_c_songs)/sizeof(button_c_songs[0]));

    espnow_broadcast(MSG_SYNC_START, song_id);

    const song_t *song = &songs[song_id];
    display_animations_start_playback(song->type);
}
