/**
 * @file rf.h
 * @brief RF 433MHz Receiver with pairing support
 * 
 * Based on RFCodes by Matthias Hertel (BSD 3-Clause License)
 * https://github.com/mathertel/RFCodes
 */

#ifndef RF_H
#define RF_H

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rfcodes/rfcodes.h"
#include "relays.h"
#include "pairing.h"
#include "status_led.h"

#define RF_TAG "RF433"

// Pin configuration
#define RF_RCV_PIN 5
#define RF_SEND_PIN -1  // Set to GPIO number if you want to transmit, -1 to disable

// Global instances
static signal_parser_t rf_parser;
static signal_collector_t rf_collector;

// Last received code for debouncing
static char last_rf_code[MAX_SEQUENCE_LENGTH + PROTNAME_LEN + 2] = {0};
static uint32_t last_rf_time = 0;
#define RF_DEBOUNCE_MS 200  // Quick debounce for duplicate signals

// Per-button hold detection - tracks last toggle time for each relay
static uint32_t last_toggle_time[4] = {0, 0, 0, 0};

/**
 * @brief Extract address from EV1527 sequence
 * @param sequence Full sequence (s + 20 address + 4 data)
 * @param address Output buffer for address (must be 21 bytes)
 * @param data Output for 4-bit data value
 * @return true if valid EV1527 sequence
 */
static bool rf_parse_ev1527(const char *sequence, char *address, uint8_t *data) {
    if (strlen(sequence) != 25 || sequence[0] != 's') {
        return false;
    }
    
    // Extract address (20 bits)
    strncpy(address, sequence + 1, 20);
    address[20] = '\0';
    
    // Extract and convert data (4 bits)
    *data = 0;
    for (int i = 0; i < 4; i++) {
        *data = (*data << 1) | (sequence[21 + i] == '1' ? 1 : 0);
    }
    
    return true;
}

/**
 * @brief Callback function called when a valid RF code is received
 * @param code The received code string in format "<protocol> <sequence>"
 */
static void rf_code_received_callback(const char *code) {
    uint32_t now = esp_timer_get_time() / 1000;  // Convert to ms
    
    // Basic debounce: ignore if same code received within short time
    if (strcmp(code, last_rf_code) == 0 && (now - last_rf_time < RF_DEBOUNCE_MS)) {
        return;
    }
    
    // Store for debouncing
    strncpy(last_rf_code, code, sizeof(last_rf_code) - 1);
    last_rf_code[sizeof(last_rf_code) - 1] = '\0';
    last_rf_time = now;
    
    ESP_LOGI(RF_TAG, "Received: %s", code);
    
    // Parse the code to extract protocol and sequence
    const char *space = strchr(code, ' ');
    if (!space) {
        return;
    }
    
    char protocol[PROTNAME_LEN] = {0};
    int proto_len = space - code;
    if (proto_len >= PROTNAME_LEN) {
        return;
    }
    strncpy(protocol, code, proto_len);
    const char *sequence = space + 1;
    
    ESP_LOGI(RF_TAG, "Protocol: %s, Sequence: %s", protocol, sequence);
    
    // Only handle EV1527 protocol
    if (strcmp(protocol, "ev1527") != 0) {
        return;
    }
    
    // Parse EV1527 sequence
    char address[21];
    uint8_t data;
    if (!rf_parse_ev1527(sequence, address, &data)) {
        ESP_LOGW(RF_TAG, "Invalid EV1527 sequence");
        return;
    }
    
    ESP_LOGD(RF_TAG, "Address: %s, Data: 0x%X", address, data);
    
    // Check if in pairing mode
    if (pairing_is_active()) {
        ESP_LOGI(RF_TAG, "Pairing mode: Learning address %s", address);
        if (pairing_save_address(address)) {
            ESP_LOGI(RF_TAG, "Remote paired successfully!");
            pairing_exit_mode();
            status_led_set(LED_STATUS_NORMAL);
        } else {
            ESP_LOGE(RF_TAG, "Failed to save pairing");
        }
        return;
    }
    
    // Check if paired
    if (!pairing_is_paired()) {
        ESP_LOGW(RF_TAG, "No remote paired - ignoring");
        return;
    }
    
    // Verify address matches paired remote
    if (strcmp(address, pairing_get_address()) != 0) {
        ESP_LOGW(RF_TAG, "Unknown remote address: %s (expected: %s)", 
                 address, pairing_get_address());
        return;
    }
    
    // Map data bits to relay numbers
    // Expected: A=1000(8), B=0100(4), C=0010(2), D=0001(1)
    int relay_num = -1;
    const char *button_name = NULL;
    
    switch (data) {
        case 0x8:  // 1000 - Button A
            relay_num = 0;
            button_name = "A";
            break;
        case 0x4:  // 0100 - Button B
            relay_num = 1;
            button_name = "B";
            break;
        case 0x2:  // 0010 - Button C
            relay_num = 2;
            button_name = "C";
            break;
        case 0x1:  // 0001 - Button D
            relay_num = 3;
            button_name = "D";
            break;
        default:
            ESP_LOGW(RF_TAG, "Unknown button data: 0x%X", data);
            return;
    }
    
    // Check if relay exists
    if (relay_num >= NUM_RELAYS) {
        ESP_LOGW(RF_TAG, "Button %s maps to relay %d, but only %d relays configured", 
                 button_name, relay_num + 1, NUM_RELAYS);
        return;
    }
    
    // Hold detection: prevent rapid toggling when button is held
    if (now - last_toggle_time[relay_num] < RF_HOLD_TIMEOUT_MS) {
        ESP_LOGD(RF_TAG, "Button %s held - ignoring (last toggle %u ms ago)", 
                 button_name, now - last_toggle_time[relay_num]);
        return;
    }
    
    // Toggle the relay
    uint8_t current_state = relay_get(relay_num);
    uint8_t new_state = !current_state;
    relay_set(relay_num, new_state);
    
    // Update last toggle time
    last_toggle_time[relay_num] = now;
    
    ESP_LOGI(RF_TAG, "Button %s pressed -> Relay %d toggled %s", 
             button_name, relay_num + 1, new_state ? "ON" : "OFF");
}

/**
 * @brief Initialize the RF receiver
 */
void rf_receiver_init(void) {
    ESP_LOGI(RF_TAG, "Initializing RF433 receiver with RFCodes library");
    ESP_LOGI(RF_TAG, "RFCodes version: %s", RFCODES_VERSION);
    
    // Initialize the signal parser
    signal_parser_init(&rf_parser);
    
    // Load EV1527 protocol only
    signal_parser_load(&rf_parser, &protocol_ev1527, 0);
    
    // Attach callback for received codes
    signal_parser_attach_callback(&rf_parser, rf_code_received_callback);
    
    // Initialize the signal collector (handles GPIO and interrupts)
    signal_collector_init(&rf_collector, &rf_parser, RF_RCV_PIN, RF_SEND_PIN, 0);
    
    ESP_LOGI(RF_TAG, "RF receiver initialized on GPIO %d", RF_RCV_PIN);
    
    // Check pairing status
    if (pairing_is_paired()) {
        ESP_LOGI(RF_TAG, "Remote paired: %s", pairing_get_address());
    } else {
        ESP_LOGW(RF_TAG, "No remote paired - touch pairing wires to pair");
    }
}

/**
 * @brief RF decode task - processes received signals
 * @param pvParameters Task parameters (unused)
 */
void rf_decode_task(void *pvParameters) {
    ESP_LOGI(RF_TAG, "RF decode task started");
    
    while (1) {
        // Process any buffered RF signals
        signal_collector_loop(&rf_collector);
        
        // Small delay to prevent task starvation
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Send an RF code (if transmitter is configured)
 * @param code Code string in format "<protocol> <sequence>"
 * @return true if sent successfully, false otherwise
 */
bool rf_send_code(const char *code) {
    if (RF_SEND_PIN < 0) {
        ESP_LOGW(RF_TAG, "RF transmitter not configured");
        return false;
    }
    
    ESP_LOGI(RF_TAG, "Sending: %s", code);
    signal_collector_send(&rf_collector, code);
    return true;
}

#endif // RF_H