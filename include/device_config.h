#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

// Device role definitions
typedef enum {
    ROLE_CONDUCTOR = 0,  // Device 0 - can initiate songs
    ROLE_PART_1 = 1,     // Device 1 - First violin/melody
    ROLE_PART_2 = 2,     // Device 2 - Second violin
    ROLE_PART_3 = 3,     // Device 3 - Viola
    ROLE_PART_4 = 4,     // Device 4 - Cello
    ROLE_PART_5 = 5,     // Device 5 - Bass
    ROLE_UNKNOWN = 0xFF
} device_role_t;

// Device info structure
typedef struct {
    uint8_t mac_address[6];
    device_role_t role;
    char name[32];
    bool is_online;
    uint32_t last_seen;
} device_info_t;

// Configuration methods
typedef enum {
    CONFIG_METHOD_GPIO = 0,     // Read from GPIO pins
    CONFIG_METHOD_MAC_TABLE,    // Use MAC address lookup table
    CONFIG_METHOD_AUTO_ASSIGN,  // Automatic assignment based on discovery order
    CONFIG_METHOD_NVS           // Stored in NVS
} config_method_t;

// Function declarations
device_role_t device_config_get_role(void);
void device_config_set_role(device_role_t role);
esp_err_t device_config_init(config_method_t method);
const char* device_config_get_role_name(device_role_t role);
bool device_config_should_play_part(device_role_t role, uint8_t parts_mask);

#endif // DEVICE_CONFIG_H