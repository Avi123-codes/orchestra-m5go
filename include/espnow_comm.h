// include/espnow_comm.h
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "orchestra.h"   // for msg_type_t, espnow_msg_t (single source of truth)

// Initialize ESP-NOW layer (id is an optional local identifier you can use in messages)
esp_err_t espnow_init(uint8_t id);

// Broadcast a control message to all peers (FF:FF:FF:FF:FF:FF)
esp_err_t espnow_broadcast(msg_type_t type, uint8_t song_id);

// (Optional) If later you want unicast helpers, declare them here.
// esp_err_t espnow_send_to(const uint8_t mac[6], msg_type_t type, uint8_t song_id);
