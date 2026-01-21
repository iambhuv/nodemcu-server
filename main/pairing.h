/**
 * @file pairing.h
 * @brief RF Remote pairing and NVS storage management
 */

#ifndef PAIRING_H
#define PAIRING_H

#include <stdbool.h>
#include <stdint.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "config.h"

#define PAIRING_TAG "PAIRING"
#define NVS_NAMESPACE "relay_ctrl"
#define NVS_KEY_RF_ADDR "rf_address"
#define NVS_KEY_RELAY_STATE "relay_state"

// Pairing state
typedef struct {
    bool is_paired;
    char rf_address[21];  // 20-bit address as string + null terminator
    bool pairing_mode_active;
    uint32_t pairing_mode_start_time;
} pairing_state_t;

static pairing_state_t pairing_state = {
    .is_paired = false,
    .rf_address = {0},
    .pairing_mode_active = false,
    .pairing_mode_start_time = 0
};

#define PAIRING_MODE_TIMEOUT_MS 30000  // 30 seconds

/**
 * @brief Initialize NVS and load saved pairing data
 */
void pairing_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(PAIRING_TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Try to load saved RF address
    nvs_handle_t nvs_handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t required_size = sizeof(pairing_state.rf_address);
        err = nvs_get_str(nvs_handle, NVS_KEY_RF_ADDR, pairing_state.rf_address, &required_size);
        if (err == ESP_OK) {
            pairing_state.is_paired = true;
            ESP_LOGI(PAIRING_TAG, "Loaded paired RF address: %s", pairing_state.rf_address);
        } else {
            ESP_LOGI(PAIRING_TAG, "No paired remote found");
        }
        nvs_close(nvs_handle);
    }
}

/**
 * @brief Save RF address to NVS
 */
bool pairing_save_address(const char *address) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(PAIRING_TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_str(nvs_handle, NVS_KEY_RF_ADDR, address);
    if (err != ESP_OK) {
        ESP_LOGE(PAIRING_TAG, "Failed to save address: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        strncpy(pairing_state.rf_address, address, sizeof(pairing_state.rf_address) - 1);
        pairing_state.is_paired = true;
        ESP_LOGI(PAIRING_TAG, "Saved RF address: %s", address);
        return true;
    }
    return false;
}

/**
 * @brief Clear paired remote from NVS
 */
void pairing_clear(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_erase_key(nvs_handle, NVS_KEY_RF_ADDR);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
    
    pairing_state.is_paired = false;
    memset(pairing_state.rf_address, 0, sizeof(pairing_state.rf_address));
    ESP_LOGI(PAIRING_TAG, "Cleared pairing data");
}

/**
 * @brief Enter pairing mode
 */
void pairing_enter_mode(void) {
    pairing_state.pairing_mode_active = true;
    pairing_state.pairing_mode_start_time = esp_timer_get_time() / 1000;
    ESP_LOGI(PAIRING_TAG, "Entered pairing mode (30s timeout)");
}

/**
 * @brief Exit pairing mode
 */
void pairing_exit_mode(void) {
    pairing_state.pairing_mode_active = false;
    ESP_LOGI(PAIRING_TAG, "Exited pairing mode");
}

/**
 * @brief Check if pairing mode has timed out
 */
void pairing_check_timeout(void) {
    if (pairing_state.pairing_mode_active) {
        uint32_t now = esp_timer_get_time() / 1000;
        if (now - pairing_state.pairing_mode_start_time > PAIRING_MODE_TIMEOUT_MS) {
            ESP_LOGI(PAIRING_TAG, "Pairing mode timeout");
            pairing_exit_mode();
        }
    }
}

/**
 * @brief Check if currently in pairing mode
 */
bool pairing_is_active(void) {
    return pairing_state.pairing_mode_active;
}

/**
 * @brief Check if a remote is paired
 */
bool pairing_is_paired(void) {
    return pairing_state.is_paired;
}

/**
 * @brief Get the paired RF address
 */
const char* pairing_get_address(void) {
    return pairing_state.rf_address;
}

/**
 * @brief Save relay states to NVS
 */
void pairing_save_relay_states(const uint8_t *states, size_t count) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(PAIRING_TAG, "Failed to open NVS for relay states");
        return;
    }

    // Save as a blob
    err = nvs_set_blob(nvs_handle, NVS_KEY_RELAY_STATE, states, count);
    if (err == ESP_OK) {
        nvs_commit(nvs_handle);
        ESP_LOGD(PAIRING_TAG, "Saved relay states");
    }
    nvs_close(nvs_handle);
}

/**
 * @brief Load relay states from NVS
 */
bool pairing_load_relay_states(uint8_t *states, size_t count) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return false;
    }

    size_t required_size = count;
    err = nvs_get_blob(nvs_handle, NVS_KEY_RELAY_STATE, states, &required_size);
    nvs_close(nvs_handle);

    if (err == ESP_OK && required_size == count) {
        ESP_LOGI(PAIRING_TAG, "Loaded relay states from NVS");
        return true;
    }
    return false;
}

#endif // PAIRING_H
