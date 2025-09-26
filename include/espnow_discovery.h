// src/espnow_discovery.h
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_now.h"
#include "esp_err.h"
#include "device_config.h"

// ---- discovery message types (if you already have these, keep yours) ----
typedef enum {
    DISCO_MSG_ANNOUNCE = 0,
    DISCO_MSG_ROLE_REQUEST,
    DISCO_MSG_ROLE_ASSIGN,
    DISCO_MSG_ROLL_CALL,
    DISCO_MSG_PRESENT,
    DISCO_MSG_READY,
} discovery_msg_type_t;

typedef struct {
    discovery_msg_type_t type;
    device_role_t        role;
    uint8_t              mac_address[6];
    char                 device_name[32];
    uint32_t             timestamp;     // ms
} discovery_msg_t;

typedef struct {
    uint8_t      mac_address[6];
    device_role_t role;
    char         name[32];
    bool         is_online;
    uint32_t     last_seen;             // ticks
} peer_device_t;

// ---- public APIs used elsewhere ----
esp_err_t espnow_discovery_init(void);
esp_err_t espnow_discovery_start(void);
esp_err_t espnow_discovery_announce(void);
esp_err_t espnow_discovery_request_role(void);
esp_err_t espnow_discovery_assign_role(const uint8_t *mac, device_role_t role);
esp_err_t espnow_discovery_roll_call(void);
uint8_t   espnow_discovery_get_online_count(void);
bool      espnow_discovery_all_devices_ready(void);
const peer_device_t* espnow_discovery_get_peers(void);

// ---- IMPORTANT: RX callback prototype so others can call it ----
void espnow_discovery_recv_cb(const esp_now_recv_info_t *recv_info,
                              const uint8_t *data, int len);
