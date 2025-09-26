// src/device_config.c

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
#include "songs.h"   // for PART_1..PART_5, ALL_PARTS masks

static const char *TAG = "DEVICE_CONFIG";

// 3-bit hardware ID inputs (tie to GND to assert a bit low)
#define ID_GPIO_BIT0  GPIO_NUM_34  // LSB
#define ID_GPIO_BIT1  GPIO_NUM_35
#define ID_GPIO_BIT2  GPIO_NUM_36  // MSB

static device_role_t     s_role   = ROLE_UNKNOWN;
static config_method_t   s_method = CONFIG_METHOD_AUTO_ASSIGN;

// cache a parts mask for “performer” roles (derived from role, or overridden)
static uint8_t           s_part_mask = 0;

// Optional compile-time overrides:
//   -DDEVICE_ROLE=ROLE_CONDUCTOR (or ROLE_PART_1 .. ROLE_PART_4)
//   -DDEVICE_PART=PART_1         (bitmask, optional; auto-derived if omitted)

typedef struct {
    uint8_t mac[6];
    device_role_t role;
    const char *name;
} mac_entry_t;

static const mac_entry_t mac_table[] = {
    {{0xAA,0xBB,0xCC,0xDD,0xEE,0x00}, ROLE_CONDUCTOR, "M5GO-Conductor"},
    {{0xAA,0xBB,0xCC,0xDD,0xEE,0x01}, ROLE_PART_1,   "M5GO-Part1"},
    {{0xAA,0xBB,0xCC,0xDD,0xEE,0x02}, ROLE_PART_2,   "M5GO-Part2"},
    {{0xAA,0xBB,0xCC,0xDD,0xEE,0x03}, ROLE_PART_3,   "M5GO-Part3"},
    {{0xAA,0xBB,0xCC,0xDD,0xEE,0x04}, ROLE_PART_4,   "M5GO-Part4"},
};

static uint8_t part_mask_from_role(device_role_t role)
{
    switch (role) {
        case ROLE_PART_1: return PART_1;
        case ROLE_PART_2: return PART_2;
        case ROLE_PART_3: return PART_3;
        case ROLE_PART_4: return PART_4;
        default:          return 0;
    }
}

const char* device_config_get_role_name(device_role_t role)
{
    switch (role) {
        case ROLE_CONDUCTOR: return "Conductor";
        case ROLE_PART_1:    return "Part 1";
        case ROLE_PART_2:    return "Part 2";
        case ROLE_PART_3:    return "Part 3";
        case ROLE_PART_4:    return "Part 4";
        default:             return "Unknown";
    }
}

// Read 3-bit role from GPIOs (active-low bits with internal pull-ups)
static device_role_t read_gpio_id(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << ID_GPIO_BIT0) |
                        (1ULL << ID_GPIO_BIT1) |
                        (1ULL << ID_GPIO_BIT2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io);

    // bit value = 1 if pin is grounded (=0 level), else 0
    uint8_t id = 0;
    id |= (gpio_get_level(ID_GPIO_BIT0) == 0) ? 0x01 : 0x00;
    id |= (gpio_get_level(ID_GPIO_BIT1) == 0) ? 0x02 : 0x00;
    id |= (gpio_get_level(ID_GPIO_BIT2) == 0) ? 0x04 : 0x00;

    ESP_LOGI(TAG, "GPIO ID read: %u", (unsigned)id);

    switch (id) {
        case 0: return ROLE_CONDUCTOR;
        case 1: return ROLE_PART_1;
        case 2: return ROLE_PART_2;
        case 3: return ROLE_PART_3;
        case 4: return ROLE_PART_4;
        default: return ROLE_UNKNOWN;
    }
}

// Look up by MAC in mac_table[]
static device_role_t lookup_mac_address(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    ESP_LOGI(TAG, "Device MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    for (size_t i = 0; i < sizeof(mac_table)/sizeof(mac_table[0]); ++i) {
        if (memcmp(mac, mac_table[i].mac, 6) == 0) {
            ESP_LOGI(TAG, "MAC match: %s => %s",
                     mac_table[i].name, device_config_get_role_name(mac_table[i].role));
            return mac_table[i].role;
        }
    }

    ESP_LOGW(TAG, "MAC not found in table");
    return ROLE_UNKNOWN;
}

// NVS helpers
static device_role_t read_nvs_role(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("orchestra", NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open RO failed (%s), no stored role", esp_err_to_name(err));
        return ROLE_UNKNOWN;
    }

    uint8_t role_u8 = ROLE_UNKNOWN;
    size_t  len     = sizeof(role_u8);
    err = nvs_get_blob(h, "device_role", &role_u8, &len);
    nvs_close(h);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "NVS role: %u", (unsigned)role_u8);
        return (device_role_t)role_u8;
    }
    ESP_LOGW(TAG, "NVS has no role (%s)", esp_err_to_name(err));
    return ROLE_UNKNOWN;
}

static esp_err_t save_nvs_role(device_role_t role)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("orchestra", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open RW failed: %s", esp_err_to_name(err));
        return err;
    }
    uint8_t role_u8 = (uint8_t)role;
    err = nvs_set_blob(h, "device_role", &role_u8, sizeof(role_u8));
    if (err == ESP_OK) {
        err = nvs_commit(h);
        ESP_LOGI(TAG, "Saved role to NVS: %u", (unsigned)role_u8);
    } else {
        ESP_LOGE(TAG, "nvs_set_blob failed: %s", esp_err_to_name(err));
    }
    nvs_close(h);
    return err;
}

// Public API
esp_err_t device_config_init(config_method_t method)
{
    s_method = method;

    if (method == CONFIG_METHOD_NVS || method == CONFIG_METHOD_AUTO_ASSIGN) {
        esp_err_t nvs_ok = nvs_flash_init();
        if (nvs_ok == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ok == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            nvs_ok = nvs_flash_init();
        }
        ESP_ERROR_CHECK(nvs_ok);
    }

    // 1) Compile-time overrides take precedence
#if defined(DEVICE_ROLE)
    s_role = (device_role_t)DEVICE_ROLE;
    s_part_mask =
    #if defined(DEVICE_PART)
        (uint8_t)DEVICE_PART;
    #else
        part_mask_from_role(s_role);
    #endif
    ESP_LOGI(TAG, "Compile-time ROLE=%s PART_MASK=0x%02X",
             device_config_get_role_name(s_role), (unsigned)s_part_mask);
    return ESP_OK;
#endif

    // 2) Otherwise, resolve by method
    switch (method) {
        case CONFIG_METHOD_GPIO:
            s_role = read_gpio_id();
            break;

        case CONFIG_METHOD_MAC_TABLE:
            s_role = lookup_mac_address();
            break;

        case CONFIG_METHOD_NVS:
            s_role = read_nvs_role();
            break;

        case CONFIG_METHOD_AUTO_ASSIGN:
            s_role = ROLE_UNKNOWN; // set later by discovery/control flow
            ESP_LOGI(TAG, "Auto-assign: waiting for assignment");
            break;

        default:
            ESP_LOGE(TAG, "Unknown config method");
            return ESP_FAIL;
    }

    if (s_role == ROLE_UNKNOWN && method != CONFIG_METHOD_AUTO_ASSIGN) {
        ESP_LOGW(TAG, "Failed to determine role, falling back to AUTO_ASSIGN");
        s_method = CONFIG_METHOD_AUTO_ASSIGN;
    } else if (s_role != ROLE_UNKNOWN) {
        s_part_mask = part_mask_from_role(s_role);
        ESP_LOGI(TAG, "Role resolved: %s (%d) part_mask=0x%02X",
                 device_config_get_role_name(s_role), s_role, (unsigned)s_part_mask);
    }

    return ESP_OK;
}

device_role_t device_config_get_role(void)
{
    return s_role;
}

uint8_t device_config_get_part_mask(void)
{
    return s_part_mask;
}

void device_config_set_role(device_role_t role)
{
    s_role = role;
    s_part_mask = part_mask_from_role(role);
    ESP_LOGI(TAG, "Role set to: %s (%d), part_mask=0x%02X",
             device_config_get_role_name(role), role, (unsigned)s_part_mask);

    if (s_method == CONFIG_METHOD_AUTO_ASSIGN || s_method == CONFIG_METHOD_NVS) {
        (void)save_nvs_role(role);
    }
}

// Does THIS device play for the provided parts selection?
bool device_config_should_play_part(device_role_t role, uint8_t parts_mask)
{
    switch (role) {
        case ROLE_PART_1: return (parts_mask & PART_1) != 0;
        case ROLE_PART_2: return (parts_mask & PART_2) != 0;
        case ROLE_PART_3: return (parts_mask & PART_3) != 0;
        case ROLE_PART_4: return (parts_mask & PART_4) != 0;
        default:          return false;  // conductor/unknown do not “play”
    }
}
