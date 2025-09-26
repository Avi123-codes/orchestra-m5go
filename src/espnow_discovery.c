// src/espnow_discovery.c
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

#define MAX_PEERS               5
#define HEARTBEAT_INTERVAL_MS   2000
#define PEER_TIMEOUT_MS         10000

// Broadcast MAC
static const uint8_t kBroadcastMac[ESP_NOW_ETH_ALEN] =
        {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// Peer devices list
static peer_device_t peers[MAX_PEERS];
static uint8_t       peer_count   = 0;
static SemaphoreHandle_t peers_mutex;

// Our role
static bool          s_is_conductor = false;

// Queue of discovery messages (fed by espnow_discovery_recv_cb, serviced here)
static QueueHandle_t s_discovery_queue = NULL;

// ────────────────────────────────────────────────────────────────────────────

static void get_own_mac(uint8_t *mac)
{
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
}

static peer_device_t* find_peer_by_mac(const uint8_t *mac)
{
    for (int i = 0; i < peer_count; i++) {
        if (memcmp(peers[i].mac_address, mac, ESP_NOW_ETH_ALEN) == 0) {
            return &peers[i];
        }
    }
    return NULL;
}

static esp_err_t add_or_update_peer(const uint8_t *mac, device_role_t role, const char *name)
{
    xSemaphoreTake(peers_mutex, portMAX_DELAY);

    peer_device_t *peer = find_peer_by_mac(mac);

    if (!peer && peer_count < MAX_PEERS) {
        // Add to our local table
        peer = &peers[peer_count++];
        memcpy(peer->mac_address, mac, ESP_NOW_ETH_ALEN);

        // Ensure the peer exists in ESP-NOW peer list, ignore EXIST
        esp_now_peer_info_t pi = {0};
        memcpy(pi.peer_addr, mac, ESP_NOW_ETH_ALEN);
        pi.channel = 0;         // keep current channel
        pi.encrypt = false;
        (void)esp_now_add_peer(&pi);

        ESP_LOGI(TAG, "Peer added: %02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    if (peer) {
        peer->role = role;
        peer->is_online = true;
        peer->last_seen = xTaskGetTickCount();
        if (name) {
            strncpy(peer->name, name, sizeof(peer->name) - 1);
            peer->name[sizeof(peer->name) - 1] = '\0';
        }
    }

    xSemaphoreGive(peers_mutex);
    return ESP_OK;
}

static esp_err_t send_discovery_msg(const uint8_t *dest_mac, discovery_msg_type_t type)
{
    discovery_msg_t msg = {0};
    msg.type = type;
    msg.role = device_config_get_role();
    msg.timestamp = (uint32_t)(esp_timer_get_time() / 1000ULL);  // μs → ms
    get_own_mac(msg.mac_address);
    snprintf(msg.device_name, sizeof(msg.device_name), "M5GO-%s",
             device_config_get_role_name(msg.role));

    return esp_now_send(dest_mac, (const uint8_t *)&msg, sizeof(msg));
}

static void handle_discovery_msg(const uint8_t *src_mac, const discovery_msg_t *msg)
{
    uint8_t own_mac[6];
    get_own_mac(own_mac);
    if (memcmp(src_mac, own_mac, ESP_NOW_ETH_ALEN) == 0) {
        return; // ignore own messages
    }

    ESP_LOGI(TAG, "DISC: type=%d from %02X:%02X:%02X:%02X:%02X:%02X role=%d",
             msg->type, src_mac[0], src_mac[1], src_mac[2],
             src_mac[3], src_mac[4], src_mac[5], (int)msg->role);

    switch (msg->type) {
        case DISCO_MSG_ANNOUNCE:
            // add/update peer
            add_or_update_peer(msg->mac_address, msg->role, msg->device_name);

            // Conductor assigns role to unknowns
            if (s_is_conductor && msg->role == ROLE_UNKNOWN) {
                device_role_t assign = ROLE_UNKNOWN;
                for (int r = ROLE_PART_1; r <= ROLE_PART_4; ++r) {
                    bool taken = false;
                    for (int i = 0; i < peer_count; ++i) {
                        if (peers[i].role == (device_role_t)r) { taken = true; break; }
                    }
                    if (!taken) { assign = (device_role_t)r; break; }
                }
                if (assign != ROLE_UNKNOWN) {
                    (void)espnow_discovery_assign_role(msg->mac_address, assign);
                    ESP_LOGI(TAG, "Assigned role %s to %02X:%02X:%02X:%02X:%02X:%02X",
                             device_config_get_role_name(assign),
                             msg->mac_address[0], msg->mac_address[1], msg->mac_address[2],
                             msg->mac_address[3], msg->mac_address[4], msg->mac_address[5]);
                }
            }
            break;

        case DISCO_MSG_ROLE_REQUEST:
            if (s_is_conductor) {
                // same assignment policy as above
                device_role_t assign = ROLE_UNKNOWN;
                for (int r = ROLE_PART_1; r <= ROLE_PART_4; ++r) {
                    bool taken = false;
                    for (int i = 0; i < peer_count; ++i) {
                        if (peers[i].role == (device_role_t)r) { taken = true; break; }
                    }
                    if (!taken) { assign = (device_role_t)r; break; }
                }
                if (assign != ROLE_UNKNOWN) {
                    (void)espnow_discovery_assign_role(msg->mac_address, assign);
                }
            }
            break;

        case DISCO_MSG_ROLE_ASSIGN:
            // If we’re unknown, accept assigned role & re-announce
            if (device_config_get_role() == ROLE_UNKNOWN) {
                device_config_set_role(msg->role);
                (void)send_discovery_msg(kBroadcastMac, DISCO_MSG_ANNOUNCE);
            }
            break;

        case DISCO_MSG_ROLL_CALL:
            // Respond only to sender (unicast reply)
            (void)send_discovery_msg(src_mac, DISCO_MSG_PRESENT);
            break;

        case DISCO_MSG_PRESENT:
            // mark as online
            add_or_update_peer(msg->mac_address, msg->role, msg->device_name);
            break;

        case DISCO_MSG_READY:
            // optional “ready” state
            add_or_update_peer(msg->mac_address, msg->role, msg->device_name);
            break;

        default:
            break;
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Public recv hook (called from the unified recv cb in espnow_comm.c)
// ────────────────────────────────────────────────────────────────────────────
void espnow_discovery_recv_cb(const esp_now_recv_info_t *recv_info,
                              const uint8_t *data, int len)
{
    if (len != (int)sizeof(discovery_msg_t)) return;

    discovery_msg_t msg;
    memcpy(&msg, data, sizeof(msg));

    // feed queue; src MAC is in recv_info->src_addr
    if (s_discovery_queue) {
        // Pack both src MAC and message into one struct to pass via queue
        typedef struct {
            uint8_t src_mac[ESP_NOW_ETH_ALEN];
            discovery_msg_t msg;
        } disco_q_t;

        disco_q_t q = {0};
        memcpy(q.src_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN);
        q.msg = msg;

        if (xQueueSend(s_discovery_queue, &q, 0) != pdTRUE) {
            ESP_LOGW(TAG, "discovery queue full");
        }
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Discovery task
// ────────────────────────────────────────────────────────────────────────────
static void discovery_task(void *arg)
{
    typedef struct {
        uint8_t src_mac[ESP_NOW_ETH_ALEN];
        discovery_msg_t msg;
    } disco_q_t;

    disco_q_t item;

    TickType_t last_hb = xTaskGetTickCount();

    while (1) {
        // Receive with heartbeat timeout
        if (xQueueReceive(s_discovery_queue, &item,
                          pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS)) == pdTRUE) {
            handle_discovery_msg(item.src_mac, &item.msg);
        }

        // Periodic announce/heartbeat
        TickType_t now = xTaskGetTickCount();
        if (now - last_hb >= pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS)) {
            (void)send_discovery_msg(kBroadcastMac, DISCO_MSG_ANNOUNCE);
            last_hb = now;

            // Cull timeouts
            xSemaphoreTake(peers_mutex, portMAX_DELAY);
            for (int i = 0; i < peer_count; ++i) {
                if (now - peers[i].last_seen > pdMS_TO_TICKS(PEER_TIMEOUT_MS)) {
                    peers[i].is_online = false;
                }
            }
            xSemaphoreGive(peers_mutex);
        }
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Public API
// ────────────────────────────────────────────────────────────────────────────

esp_err_t espnow_discovery_init(void)
{
    peers_mutex = xSemaphoreCreateMutex();
    if (!peers_mutex) {
        ESP_LOGE(TAG, "mutex create failed");
        return ESP_FAIL;
    }

    s_discovery_queue = xQueueCreate(16, sizeof(struct {
        uint8_t src_mac[ESP_NOW_ETH_ALEN];
        discovery_msg_t msg;
    }));
    if (!s_discovery_queue) {
        ESP_LOGE(TAG, "queue create failed");
        return ESP_FAIL;
    }

    s_is_conductor = (device_config_get_role() == ROLE_CONDUCTOR);

    xTaskCreate(discovery_task, "discovery_task", 4096, NULL, 9, NULL);

    ESP_LOGI(TAG, "Discovery ready (conductor=%d)", (int)s_is_conductor);
    return ESP_OK;
}

esp_err_t espnow_discovery_start(void)
{
    // Kick things off
    (void)send_discovery_msg(kBroadcastMac, DISCO_MSG_ANNOUNCE);

    // If we need a role, request one
    if (device_config_get_role() == ROLE_UNKNOWN) {
        vTaskDelay(pdMS_TO_TICKS(100));
        (void)send_discovery_msg(kBroadcastMac, DISCO_MSG_ROLE_REQUEST);
    }

    ESP_LOGI(TAG, "Discovery started");
    return ESP_OK;
}

esp_err_t espnow_discovery_announce(void)
{
    return send_discovery_msg(kBroadcastMac, DISCO_MSG_ANNOUNCE);
}

esp_err_t espnow_discovery_request_role(void)
{
    return send_discovery_msg(kBroadcastMac, DISCO_MSG_ROLE_REQUEST);
}

esp_err_t espnow_discovery_assign_role(const uint8_t *mac, device_role_t role)
{
    discovery_msg_t msg = {0};
    msg.type = DISCO_MSG_ROLE_ASSIGN;
    msg.role = role;
    msg.timestamp = (uint32_t)(esp_timer_get_time() / 1000ULL);
    get_own_mac(msg.mac_address);
    return esp_now_send(mac, (const uint8_t *)&msg, sizeof(msg));
}

esp_err_t espnow_discovery_roll_call(void)
{
    return send_discovery_msg(kBroadcastMac, DISCO_MSG_ROLL_CALL);
}

uint8_t espnow_discovery_get_online_count(void)
{
    uint8_t cnt = 0;
    xSemaphoreTake(peers_mutex, portMAX_DELAY);
    for (int i = 0; i < peer_count; ++i) {
        if (peers[i].is_online) ++cnt;
    }
    xSemaphoreGive(peers_mutex);
    return cnt;
}

// For a 5-device orchestra (1 conductor + 4 performers), we consider “ready”
// when we see at least 4 peers online (others) — tweak if you prefer exact roles.
bool espnow_discovery_all_devices_ready(void)
{
    return espnow_discovery_get_online_count() >= 4;
}

const peer_device_t* espnow_discovery_get_peers(void)
{
    return peers;
}
