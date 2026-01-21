/**
 * @file relay_config.h
 * @brief Relay configuration storage - names, icons, rooms
 *
 * Stores per-relay configuration in NVS with persistence across reboots.
 */

#ifndef RELAY_CONFIG_H
#define RELAY_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "config.h"

#define RELAY_CONFIG_TAG "RELAY_CFG"
#define NVS_KEY_RELAY_CONFIG "relay_cfg"

// Maximum lengths for configuration strings
#define RELAY_NAME_MAX_LEN 32
#define RELAY_ROOM_MAX_LEN 24
#define RELAY_ICON_MAX_LEN 16

// Default icons (can be used by clients for display)
typedef enum {
    ICON_LIGHT = 0,
    ICON_FAN,
    ICON_OUTLET,
    ICON_SWITCH,
    ICON_TV,
    ICON_AC,
    ICON_CUSTOM
} relay_icon_t;

// Per-relay configuration
typedef struct __attribute__((packed)) {
    char name[RELAY_NAME_MAX_LEN];     // Human-readable name (e.g., "Living Room Light")
    char room[RELAY_ROOM_MAX_LEN];     // Room assignment (e.g., "Living Room")
    uint8_t icon;                       // Icon type for UI display
    uint8_t alexa_enabled;             // Whether this relay is exposed to Alexa
} relay_config_entry_t;

// Full configuration structure
typedef struct __attribute__((packed)) {
    uint8_t version;                   // Config version for migration
    uint8_t relay_count;               // Number of relays
    relay_config_entry_t relays[NUM_RELAYS];
} relay_config_t;

#define RELAY_CONFIG_VERSION 1

// Global configuration state
static relay_config_t relay_config = {0};
static bool relay_config_dirty = false;
static uint32_t relay_config_last_change = 0;
#define RELAY_CONFIG_SAVE_DELAY_MS 3000

/**
 * @brief Initialize relay configuration with defaults
 */
static void relay_config_set_defaults(void) {
    relay_config.version = RELAY_CONFIG_VERSION;
    relay_config.relay_count = NUM_RELAYS;

    const char* default_names[] = {"Switch 1", "Switch 2", "Switch 3", "Switch 4"};

    for (int i = 0; i < NUM_RELAYS; i++) {
        snprintf(relay_config.relays[i].name, RELAY_NAME_MAX_LEN, "%s", default_names[i]);
        snprintf(relay_config.relays[i].room, RELAY_ROOM_MAX_LEN, "Home");
        relay_config.relays[i].icon = ICON_SWITCH;
        relay_config.relays[i].alexa_enabled = 1;  // Enabled by default
    }

    ESP_LOGI(RELAY_CONFIG_TAG, "Set default relay configuration");
}

/**
 * @brief Load relay configuration from NVS
 */
bool relay_config_load(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(RELAY_CONFIG_TAG, "NVS open failed, using defaults");
        relay_config_set_defaults();
        return false;
    }

    size_t required_size = sizeof(relay_config_t);
    err = nvs_get_blob(nvs_handle, NVS_KEY_RELAY_CONFIG, &relay_config, &required_size);
    nvs_close(nvs_handle);

    if (err == ESP_OK && relay_config.version == RELAY_CONFIG_VERSION) {
        ESP_LOGI(RELAY_CONFIG_TAG, "Loaded relay configuration from NVS");
        for (int i = 0; i < NUM_RELAYS; i++) {
            ESP_LOGI(RELAY_CONFIG_TAG, "  Relay %d: '%s' (room: %s, alexa: %s)",
                     i, relay_config.relays[i].name,
                     relay_config.relays[i].room,
                     relay_config.relays[i].alexa_enabled ? "yes" : "no");
        }
        return true;
    }

    ESP_LOGW(RELAY_CONFIG_TAG, "No valid config found, using defaults");
    relay_config_set_defaults();
    return false;
}

/**
 * @brief Save relay configuration to NVS
 */
bool relay_config_save(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(RELAY_CONFIG_TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_blob(nvs_handle, NVS_KEY_RELAY_CONFIG, &relay_config, sizeof(relay_config_t));
    if (err != ESP_OK) {
        ESP_LOGE(RELAY_CONFIG_TAG, "Failed to save config: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(RELAY_CONFIG_TAG, "Saved relay configuration to NVS");
        relay_config_dirty = false;
        return true;
    }

    return false;
}

/**
 * @brief Check if configuration needs saving (call periodically)
 */
void relay_config_check_save(void) {
    if (relay_config_dirty) {
        uint32_t now = esp_timer_get_time() / 1000;
        if (now - relay_config_last_change >= RELAY_CONFIG_SAVE_DELAY_MS) {
            relay_config_save();
        }
    }
}

/**
 * @brief Mark configuration as modified
 */
static void relay_config_mark_dirty(void) {
    relay_config_dirty = true;
    relay_config_last_change = esp_timer_get_time() / 1000;
}

/**
 * @brief Get relay name
 */
const char* relay_config_get_name(uint8_t relay_id) {
    if (relay_id >= NUM_RELAYS) {
        return "Unknown";
    }
    return relay_config.relays[relay_id].name;
}

/**
 * @brief Set relay name
 */
bool relay_config_set_name(uint8_t relay_id, const char* name) {
    if (relay_id >= NUM_RELAYS || name == NULL) {
        return false;
    }

    // Ensure null termination and copy
    strncpy(relay_config.relays[relay_id].name, name, RELAY_NAME_MAX_LEN - 1);
    relay_config.relays[relay_id].name[RELAY_NAME_MAX_LEN - 1] = '\0';

    relay_config_mark_dirty();
    ESP_LOGI(RELAY_CONFIG_TAG, "Relay %d renamed to '%s'", relay_id, relay_config.relays[relay_id].name);

    return true;
}

/**
 * @brief Get relay room
 */
const char* relay_config_get_room(uint8_t relay_id) {
    if (relay_id >= NUM_RELAYS) {
        return "Unknown";
    }
    return relay_config.relays[relay_id].room;
}

/**
 * @brief Set relay room
 */
bool relay_config_set_room(uint8_t relay_id, const char* room) {
    if (relay_id >= NUM_RELAYS || room == NULL) {
        return false;
    }

    strncpy(relay_config.relays[relay_id].room, room, RELAY_ROOM_MAX_LEN - 1);
    relay_config.relays[relay_id].room[RELAY_ROOM_MAX_LEN - 1] = '\0';

    relay_config_mark_dirty();
    ESP_LOGI(RELAY_CONFIG_TAG, "Relay %d room set to '%s'", relay_id, relay_config.relays[relay_id].room);

    return true;
}

/**
 * @brief Get relay icon
 */
uint8_t relay_config_get_icon(uint8_t relay_id) {
    if (relay_id >= NUM_RELAYS) {
        return ICON_SWITCH;
    }
    return relay_config.relays[relay_id].icon;
}

/**
 * @brief Set relay icon
 */
bool relay_config_set_icon(uint8_t relay_id, uint8_t icon) {
    if (relay_id >= NUM_RELAYS) {
        return false;
    }

    relay_config.relays[relay_id].icon = icon;
    relay_config_mark_dirty();

    return true;
}

/**
 * @brief Check if relay is Alexa-enabled
 */
bool relay_config_alexa_enabled(uint8_t relay_id) {
    if (relay_id >= NUM_RELAYS) {
        return false;
    }
    return relay_config.relays[relay_id].alexa_enabled != 0;
}

/**
 * @brief Set Alexa enabled state for relay
 */
bool relay_config_set_alexa(uint8_t relay_id, bool enabled) {
    if (relay_id >= NUM_RELAYS) {
        return false;
    }

    relay_config.relays[relay_id].alexa_enabled = enabled ? 1 : 0;
    relay_config_mark_dirty();
    ESP_LOGI(RELAY_CONFIG_TAG, "Relay %d Alexa %s", relay_id, enabled ? "enabled" : "disabled");

    return true;
}

/**
 * @brief Get full relay configuration entry
 */
const relay_config_entry_t* relay_config_get(uint8_t relay_id) {
    if (relay_id >= NUM_RELAYS) {
        return NULL;
    }
    return &relay_config.relays[relay_id];
}

/**
 * @brief Get relay count
 */
uint8_t relay_config_count(void) {
    return NUM_RELAYS;
}

#endif // RELAY_CONFIG_H
