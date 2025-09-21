#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "songs.h"

static const char *TAG = "AUDIO";

// M5Stack Core speaker configuration
#define SPEAKER_PIN     25
#define SAMPLE_RATE     44100
#define DMA_BUF_COUNT   8
#define DMA_BUF_LEN     64

// Audio state
static bool audio_playing = false;
static float volume = 0.15f;
static TaskHandle_t playback_task_handle = NULL;

// Generate sine wave samples for a frequency
static void generate_tone(int16_t *buffer, size_t samples, uint16_t freq) {
    static float phase = 0;
    float phase_increment = 2.0f * M_PI * freq / SAMPLE_RATE;

    for (size_t i = 0; i < samples; i++) {
        buffer[i] = (int16_t)(sinf(phase) * 32767 * volume);
        phase += phase_increment;
        if (phase >= 2.0f * M_PI) {
            phase -= 2.0f * M_PI;
        }
    }
}

// Initialize I2S for audio output
static void audio_init_i2s(void) {
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = true,
    };

    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM_0, NULL));

    ESP_LOGI(TAG, "I2S audio initialized");
}

// Play a single note
static void play_note(uint16_t frequency, uint16_t duration_ms) {
    if (frequency == 0) {
        // REST note
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        return;
    }

    size_t samples_per_ms = SAMPLE_RATE / 1000;
    size_t total_samples = samples_per_ms * duration_ms;
    size_t buffer_size = 512;
    int16_t *buffer = malloc(buffer_size * sizeof(int16_t));

    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        return;
    }

    size_t remaining = total_samples;
    while (remaining > 0 && audio_playing) {
        size_t to_generate = (remaining > buffer_size) ? buffer_size : remaining;
        generate_tone(buffer, to_generate, frequency);

        size_t bytes_written;
        i2s_write(I2S_NUM_0, buffer, to_generate * sizeof(int16_t), &bytes_written, portMAX_DELAY);

        remaining -= to_generate;
    }

    free(buffer);
}

// Playback task
static void playback_task(void *pvParameters) {
    uint8_t song_id = (uint8_t)(uintptr_t)pvParameters;

    if (song_id >= total_songs) {
        ESP_LOGE(TAG, "Invalid song ID: %d", song_id);
        vTaskDelete(NULL);
        return;
    }

    const song_t *song = &songs[song_id];
    ESP_LOGI(TAG, "Playing: %s", song->name);

    audio_playing = true;

    // Play all notes in the song
    for (uint16_t i = 0; i < song->note_count && audio_playing; i++) {
        play_note(song->notes[i].frequency, song->notes[i].duration_ms);
    }

    audio_playing = false;
    ESP_LOGI(TAG, "Finished playing: %s", song->name);

    playback_task_handle = NULL;
    vTaskDelete(NULL);
}

// Initialize audio system
void audio_init(void) {
    audio_init_i2s();
    ESP_LOGI(TAG, "Audio system initialized");
}

// Forward declaration
void audio_stop(void);

// Start playing a song
void audio_play_song(uint8_t song_id) {
    // Stop any currently playing song
    audio_stop();

    // Start new playback task
    xTaskCreate(playback_task, "playback", 4096, (void *)(uintptr_t)song_id, 10, &playback_task_handle);
}

// Stop playing
void audio_stop(void) {
    audio_playing = false;

    // Wait for playback task to finish
    if (playback_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (playback_task_handle != NULL) {
            vTaskDelete(playback_task_handle);
            playback_task_handle = NULL;
        }
    }

    ESP_LOGI(TAG, "Audio stopped");
}

// Set volume (0.0 to 1.0)
void audio_set_volume(float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    volume = vol;
    ESP_LOGI(TAG, "Volume set to %.2f", volume);
}

// Check if audio is currently playing
bool audio_is_playing(void) {
    return audio_playing;
}