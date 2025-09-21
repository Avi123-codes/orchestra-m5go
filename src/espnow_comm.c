#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "orchestra.h"

static const char *TAG = "ESPNOW";

// Broadcast MAC address
static const uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// Queue for receiving ESP-NOW messages
static QueueHandle_t espnow_queue;

// Device ID (can be set via NVS or GPIO)
static uint8_t device_id = 0;

// ESP-NOW send callback
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "Send failed to %02X:%02X:%02X:%02X:%02X:%02X",
                 mac_addr[0], mac_addr[1], mac_addr[2],
                 mac_addr[3], mac_addr[4], mac_addr[5]);
    }
}

// ESP-NOW receive callback
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len != sizeof(espnow_msg_t)) {
        ESP_LOGW(TAG, "Received invalid message size: %d", len);
        return;
    }

    espnow_msg_t msg;
    memcpy(&msg, data, sizeof(msg));

    // Send message to queue for processing
    if (xQueueSend(espnow_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Queue full, dropping message");
    }
}

// Task to process ESP-NOW messages
static void espnow_task(void *pvParameters) {
    espnow_msg_t msg;

    while (1) {
        if (xQueueReceive(espnow_queue, &msg, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Received message type: %d, from device: %d", msg.type, msg.sender_id);

            switch (msg.type) {
                case MSG_SYNC_START:
                    ESP_LOGI(TAG, "Starting song %d", msg.song_id);
                    orchestra_play_song(msg.song_id);
                    break;

                case MSG_SYNC_STOP:
                    ESP_LOGI(TAG, "Stopping playback");
                    orchestra_stop();
                    break;

                case MSG_SONG_SELECT:
                    ESP_LOGI(TAG, "Song selected: %d", msg.song_id);
                    // Prepare for playback
                    break;

                case MSG_HEARTBEAT:
                    // Handle heartbeat for synchronization
                    break;

                default:
                    ESP_LOGW(TAG, "Unknown message type: %d", msg.type);
                    break;
            }
        }
    }
}

// Initialize ESP-NOW
esp_err_t espnow_init(uint8_t id) {
    device_id = id;

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    // Add broadcast peer
    esp_now_peer_info_t peer = {
        .channel = 0,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    // Create message queue
    espnow_queue = xQueueCreate(10, sizeof(espnow_msg_t));
    if (espnow_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");
        return ESP_FAIL;
    }

    // Start ESP-NOW task
    xTaskCreate(espnow_task, "espnow_task", 4096, NULL, 10, NULL);

    ESP_LOGI(TAG, "ESP-NOW initialized, device ID: %d", device_id);
    return ESP_OK;
}

// Broadcast a message to all devices
esp_err_t espnow_broadcast(msg_type_t type, uint8_t song_id) {
    espnow_msg_t msg = {
        .type = type,
        .song_id = song_id,
        .timestamp = xTaskGetTickCount(),
        .sender_id = device_id
    };

    return esp_now_send(broadcast_mac, (uint8_t *)&msg, sizeof(msg));
}