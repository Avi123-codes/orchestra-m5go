#ifndef ESPNOW_DISCOVERY_H
#define ESPNOW_DISCOVERY_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_now.h"
#include "device_config.h"

// Discovery protocol messages
typedef enum {
    DISCO_MSG_ANNOUNCE = 0x10,   // Device announcement
    DISCO_MSG_ROLE_REQUEST,       // Request role assignment
    DISCO_MSG_ROLE_ASSIGN,        // Assign role to device
    DISCO_MSG_ROLL_CALL,          // Check who's online
    DISCO_MSG_PRESENT,            // Response to roll call
    DISCO_MSG_READY,              // Device ready to play
} discovery_msg_type_t;

// Discovery message structure
typedef struct {
    discovery_msg_type_t type;
    uint8_t mac_address[6];
    device_role_t role;
    char device_name[32];
    uint32_t timestamp;
} discovery_msg_t;

// Peer device structure
typedef struct {
    uint8_t mac_address[ESP_NOW_ETH_ALEN];
    device_role_t role;
    bool is_online;
    uint32_t last_seen;
    char name[32];
} peer_device_t;

// Function declarations
esp_err_t espnow_discovery_init(void);
esp_err_t espnow_discovery_start(void);
esp_err_t espnow_discovery_announce(void);
esp_err_t espnow_discovery_request_role(void);
esp_err_t espnow_discovery_assign_role(const uint8_t *mac, device_role_t role);
esp_err_t espnow_discovery_roll_call(void);
uint8_t espnow_discovery_get_online_count(void);
bool espnow_discovery_all_devices_ready(void);
const peer_device_t* espnow_discovery_get_peers(void);

#endif // ESPNOW_DISCOVERY_H