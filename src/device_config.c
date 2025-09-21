#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "device_config.h"

static const char *TAG = "DEVICE_CONFIG";

// GPIO pins for hardware device ID configuration (using pulldown resistors)
#define ID_GPIO_BIT0  GPIO_NUM_34  // LSB
#define ID_GPIO_BIT1  GPIO_NUM_35
#define ID_GPIO_BIT2  GPIO_NUM_36  // MSB

static device_role_t current_role = ROLE_UNKNOWN;
static config_method_t current_method = CONFIG_METHOD_AUTO_ASSIGN;

// MAC address table for your specific M5GO devices
// Replace these with your actual M5GO MAC addresses
static const device_info_t mac_table[] = {
    // Device 0 - Conductor/Part 1
    {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x00}, ROLE_CONDUCTOR, "M5GO-Conductor", false, 0},
    // Device 1 - Part 1 (First Violin)
    {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01}, ROLE_PART_1, "M5GO-Part1", false, 0},
    // Device 2 - Part 2 (Second Violin)
    {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x02}, ROLE_PART_2, "M5GO-Part2", false, 0},
    // Device 3 - Part 3 (Viola)
    {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x03}, ROLE_PART_3, "M5GO-Part3", false, 0},
    // Device 4 - Part 4 (Cello)
    {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x04}, ROLE_PART_4, "M5GO-Part4", false, 0},
};

// Get device role name
const char* device_config_get_role_name(device_role_t role) {
    switch (role) {
        case ROLE_CONDUCTOR: return "Conductor";
        case ROLE_PART_1: return "Part 1";
        case ROLE_PART_2: return "Part 2";
        case ROLE_PART_3: return "Part 3";
        case ROLE_PART_4: return "Part 4";
        case ROLE_PART_5: return "Part 5";
        default: return "Unknown";
    }
}

// Read device ID from GPIO pins
static device_role_t read_gpio_id(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ID_GPIO_BIT0) | (1ULL << ID_GPIO_BIT1) | (1ULL << ID_GPIO_BIT2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,  // Use pullup - ground pins to set ID
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io_conf);

    // Read 3-bit ID (0-7)
    uint8_t id = 0;
    id |= (gpio_get_level(ID_GPIO_BIT0) == 0) ? 0x01 : 0x00;
    id |= (gpio_get_level(ID_GPIO_BIT1) == 0) ? 0x02 : 0x00;
    id |= (gpio_get_level(ID_GPIO_BIT2) == 0) ? 0x04 : 0x00;

    ESP_LOGI(TAG, "GPIO ID read: %d", id);

    if (id <= ROLE_PART_5) {
        return (device_role_t)id;
    }
    return ROLE_UNKNOWN;
}

// Look up device role by MAC address
static device_role_t lookup_mac_address(void) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    ESP_LOGI(TAG, "Device MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    for (int i = 0; i < sizeof(mac_table) / sizeof(mac_table[0]); i++) {
        if (memcmp(mac, mac_table[i].mac_address, 6) == 0) {
            ESP_LOGI(TAG, "Found device in MAC table: %s", mac_table[i].name);
            return mac_table[i].role;
        }
    }

    ESP_LOGW(TAG, "MAC address not found in table");
    return ROLE_UNKNOWN;
}

// Read device role from NVS
static device_role_t read_nvs_role(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("orchestra", NVS_READONLY, &nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS not initialized or no role stored");
        return ROLE_UNKNOWN;
    }

    uint8_t role = ROLE_UNKNOWN;
    size_t length = sizeof(role);
    err = nvs_get_blob(nvs_handle, "device_role", &role, &length);
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Read role from NVS: %d", role);
        return (device_role_t)role;
    }

    return ROLE_UNKNOWN;
}

// Save device role to NVS
static esp_err_t save_nvs_role(device_role_t role) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("orchestra", NVS_READWRITE, &nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS");
        return err;
    }

    uint8_t role_byte = (uint8_t)role;
    err = nvs_set_blob(nvs_handle, "device_role", &role_byte, sizeof(role_byte));

    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        ESP_LOGI(TAG, "Saved role to NVS: %d", role);
    }

    nvs_close(nvs_handle);
    return err;
}

// Initialize device configuration
esp_err_t device_config_init(config_method_t method) {
    current_method = method;

    switch (method) {
        case CONFIG_METHOD_GPIO:
            current_role = read_gpio_id();
            break;

        case CONFIG_METHOD_MAC_TABLE:
            current_role = lookup_mac_address();
            break;

        case CONFIG_METHOD_NVS:
            current_role = read_nvs_role();
            break;

        case CONFIG_METHOD_AUTO_ASSIGN:
            // Start with unknown, will be assigned during discovery
            current_role = ROLE_UNKNOWN;
            ESP_LOGI(TAG, "Auto-assign mode - waiting for role assignment");
            break;

        default:
            ESP_LOGE(TAG, "Unknown config method");
            return ESP_FAIL;
    }

    if (current_role == ROLE_UNKNOWN && method != CONFIG_METHOD_AUTO_ASSIGN) {
        ESP_LOGW(TAG, "Failed to determine role, falling back to auto-assign");
        current_method = CONFIG_METHOD_AUTO_ASSIGN;
    } else if (current_role != ROLE_UNKNOWN) {
        ESP_LOGI(TAG, "Device role: %s (%d)",
                 device_config_get_role_name(current_role), current_role);
    }

    return ESP_OK;
}

// Get current device role
device_role_t device_config_get_role(void) {
    return current_role;
}

// Set device role (and optionally save to NVS)
void device_config_set_role(device_role_t role) {
    current_role = role;
    ESP_LOGI(TAG, "Role set to: %s (%d)",
             device_config_get_role_name(role), role);

    // Optionally save to NVS for persistence
    if (current_method == CONFIG_METHOD_AUTO_ASSIGN) {
        save_nvs_role(role);
    }
}

// Check if this device should play in a multi-part song
bool device_config_should_play_part(device_role_t role, uint8_t parts_mask) {
    if (role == ROLE_UNKNOWN || role > ROLE_PART_5) {
        return false;
    }

    // Check if this role's bit is set in the parts mask
    return (parts_mask & (1 << role)) != 0;
}