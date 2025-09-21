#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "espnow_discovery.h"
#include "device_config.h"

static const char *TAG = "ESPNOW_DISCO";

#define MAX_PEERS 5
#define DISCOVERY_TIMEOUT_MS 5000
#define HEARTBEAT_INTERVAL_MS 2000
#define PEER_TIMEOUT_MS 10000

// Broadcast MAC address
static const uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Peer devices list
static peer_device_t peers[MAX_PEERS];
static uint8_t peer_count = 0;
static SemaphoreHandle_t peers_mutex;

// Discovery state
static bool discovery_active = false;
static bool is_conductor = false;
static QueueHandle_t discovery_queue;

// Get our MAC address
static void get_own_mac(uint8_t *mac) {
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
}

// Find peer by MAC address
static peer_device_t* find_peer_by_mac(const uint8_t *mac) {
    for (int i = 0; i < peer_count; i++) {
        if (memcmp(peers[i].mac_address, mac, ESP_NOW_ETH_ALEN) == 0) {
            return &peers[i];
        }
    }
    return NULL;
}

// Add or update peer
static esp_err_t add_or_update_peer(const uint8_t *mac, device_role_t role, const char *name) {
    xSemaphoreTake(peers_mutex, portMAX_DELAY);

    peer_device_t *peer = find_peer_by_mac(mac);

    if (peer == NULL && peer_count < MAX_PEERS) {
        // Add new peer
        peer = &peers[peer_count++];
        memcpy(peer->mac_address, mac, ESP_NOW_ETH_ALEN);

        // Add to ESP-NOW peer list
        esp_now_peer_info_t peer_info = {0};
        memcpy(peer_info.peer_addr, mac, ESP_NOW_ETH_ALEN);
        peer_info.channel = 0;
        peer_info.encrypt = false;
        esp_now_add_peer(&peer_info);

        ESP_LOGI(TAG, "Added new peer: %02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    if (peer != NULL) {
        // Update peer info
        peer->role = role;
        peer->is_online = true;
        peer->last_seen = xTaskGetTickCount();
        if (name != NULL) {
            strncpy(peer->name, name, sizeof(peer->name) - 1);
        }
    }

    xSemaphoreGive(peers_mutex);
    return ESP_OK;
}

// Send discovery message
static esp_err_t send_discovery_msg(const uint8_t *dest_mac, discovery_msg_type_t type) {
    discovery_msg_t msg = {0};
    msg.type = type;
    msg.role = device_config_get_role();
    msg.timestamp = esp_timer_get_time() / 1000;  // Convert to ms
    get_own_mac(msg.mac_address);
    snprintf(msg.device_name, sizeof(msg.device_name), "M5GO-%s",
             device_config_get_role_name(msg.role));

    return esp_now_send(dest_mac, (uint8_t *)&msg, sizeof(msg));
}

// Process received discovery message
static void process_discovery_msg(const uint8_t *src_mac, const discovery_msg_t *msg) {
    uint8_t own_mac[6];
    get_own_mac(own_mac);

    // Don't process our own messages
    if (memcmp(src_mac, own_mac, ESP_NOW_ETH_ALEN) == 0) {
        return;
    }

    ESP_LOGI(TAG, "Discovery msg type %d from %02X:%02X:%02X:%02X:%02X:%02X, role=%d",
             msg->type, src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5],
             msg->role);

    switch (msg->type) {
        case DISCO_MSG_ANNOUNCE:
            // Another device announced itself
            add_or_update_peer(msg->mac_address, msg->role, msg->device_name);

            // If we're the conductor and they need a role, assign one
            if (is_conductor && msg->role == ROLE_UNKNOWN) {
                // Assign next available role
                uint8_t assigned_role = ROLE_UNKNOWN;
                for (int i = ROLE_PART_1; i <= ROLE_PART_5; i++) {
                    bool role_taken = false;
                    for (int j = 0; j < peer_count; j++) {
                        if (peers[j].role == i) {
                            role_taken = true;
                            break;
                        }
                    }
                    if (!role_taken) {
                        assigned_role = i;
                        break;
                    }
                }

                if (assigned_role != ROLE_UNKNOWN) {
                    espnow_discovery_assign_role(msg->mac_address, assigned_role);
                }
            }
            break;

        case DISCO_MSG_ROLE_REQUEST:
            // Device requesting role assignment
            if (is_conductor) {
                // Conductor assigns roles
                // Similar logic as above
            }
            break;

        case DISCO_MSG_ROLE_ASSIGN:
            // We're being assigned a role
            if (device_config_get_role() == ROLE_UNKNOWN) {
                device_config_set_role(msg->role);
                ESP_LOGI(TAG, "Assigned role: %s", device_config_get_role_name(msg->role));
                // Send confirmation
                send_discovery_msg(broadcast_mac, DISCO_MSG_ANNOUNCE);
            }
            break;

        case DISCO_MSG_ROLL_CALL:
            // Respond to roll call
            send_discovery_msg(src_mac, DISCO_MSG_PRESENT);
            break;

        case DISCO_MSG_PRESENT:
            // Device responded to roll call
            add_or_update_peer(msg->mac_address, msg->role, msg->device_name);
            break;

        case DISCO_MSG_READY:
            // Device is ready
            peer_device_t *peer = find_peer_by_mac(msg->mac_address);
            if (peer != NULL) {
                peer->is_online = true;
                peer->last_seen = xTaskGetTickCount();
            }
            break;
    }
}

// ESP-NOW receive callback for discovery
void espnow_discovery_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(discovery_msg_t)) {
        discovery_msg_t msg;
        memcpy(&msg, data, sizeof(msg));

        // Check if it's a discovery message
        if (msg.type >= DISCO_MSG_ANNOUNCE && msg.type <= DISCO_MSG_READY) {
            if (xQueueSend(discovery_queue, &msg, 0) != pdTRUE) {
                ESP_LOGW(TAG, "Discovery queue full");
            }
        }
    }
}

// Discovery task
static void discovery_task(void *pvParameters) {
    discovery_msg_t msg;

    while (1) {
        if (xQueueReceive(discovery_queue, &msg, pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS)) == pdTRUE) {
            process_discovery_msg(msg.mac_address, &msg);
        }

        // Periodic tasks
        if (discovery_active) {
            // Send heartbeat/announce
            send_discovery_msg(broadcast_mac, DISCO_MSG_ANNOUNCE);

            // Check for timed-out peers
            uint32_t now = xTaskGetTickCount();
            xSemaphoreTake(peers_mutex, portMAX_DELAY);
            for (int i = 0; i < peer_count; i++) {
                if (now - peers[i].last_seen > pdMS_TO_TICKS(PEER_TIMEOUT_MS)) {
                    peers[i].is_online = false;
                }
            }
            xSemaphoreGive(peers_mutex);
        }
    }
}

// Initialize discovery system
esp_err_t espnow_discovery_init(void) {
    peers_mutex = xSemaphoreCreateMutex();
    if (peers_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }

    discovery_queue = xQueueCreate(10, sizeof(discovery_msg_t));
    if (discovery_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");
        return ESP_FAIL;
    }

    // Check if we're the conductor
    is_conductor = (device_config_get_role() == ROLE_CONDUCTOR);

    // Start discovery task
    xTaskCreate(discovery_task, "discovery_task", 4096, NULL, 10, NULL);

    ESP_LOGI(TAG, "Discovery initialized, conductor=%d", is_conductor);
    return ESP_OK;
}

// Start discovery process
esp_err_t espnow_discovery_start(void) {
    discovery_active = true;

    // Send initial announcement
    send_discovery_msg(broadcast_mac, DISCO_MSG_ANNOUNCE);

    // If we need a role, request one
    if (device_config_get_role() == ROLE_UNKNOWN) {
        vTaskDelay(pdMS_TO_TICKS(100));
        send_discovery_msg(broadcast_mac, DISCO_MSG_ROLE_REQUEST);
    }

    ESP_LOGI(TAG, "Discovery started");
    return ESP_OK;
}

// Send announcement
esp_err_t espnow_discovery_announce(void) {
    return send_discovery_msg(broadcast_mac, DISCO_MSG_ANNOUNCE);
}

// Request role assignment
esp_err_t espnow_discovery_request_role(void) {
    return send_discovery_msg(broadcast_mac, DISCO_MSG_ROLE_REQUEST);
}

// Assign role to a device
esp_err_t espnow_discovery_assign_role(const uint8_t *mac, device_role_t role) {
    discovery_msg_t msg = {0};
    msg.type = DISCO_MSG_ROLE_ASSIGN;
    msg.role = role;
    msg.timestamp = esp_timer_get_time() / 1000;
    get_own_mac(msg.mac_address);

    return esp_now_send(mac, (uint8_t *)&msg, sizeof(msg));
}

// Send roll call
esp_err_t espnow_discovery_roll_call(void) {
    return send_discovery_msg(broadcast_mac, DISCO_MSG_ROLL_CALL);
}

// Get online device count
uint8_t espnow_discovery_get_online_count(void) {
    uint8_t count = 0;
    xSemaphoreTake(peers_mutex, portMAX_DELAY);
    for (int i = 0; i < peer_count; i++) {
        if (peers[i].is_online) {
            count++;
        }
    }
    xSemaphoreGive(peers_mutex);
    return count;
}

// Check if all devices are ready
bool espnow_discovery_all_devices_ready(void) {
    // For quintet, we need all 5 devices
    return espnow_discovery_get_online_count() >= 4;  // 4 peers + ourselves = 5
}

// Get peer list
const peer_device_t* espnow_discovery_get_peers(void) {
    return peers;
}