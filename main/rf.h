/**
 * @file rf.h
 * @brief RF 433MHz Receiver using RFCodes library
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
    
    // Handle EV1527 protocol for relay control
    if (strcmp(protocol, "ev1527") == 0) {
        // Match against configured button codes
        int relay_num = -1;
        const char *button_name = NULL;
        
        #ifdef RF_BUTTON_A
        if (strcmp(sequence, RF_BUTTON_A) == 0) {
            relay_num = 0;
            button_name = "A";
        }
        #endif
        
        #ifdef RF_BUTTON_B
        if (strcmp(sequence, RF_BUTTON_B) == 0) {
            relay_num = 1;
            button_name = "B";
        }
        #endif
        
        #ifdef RF_BUTTON_C
        if (strcmp(sequence, RF_BUTTON_C) == 0) {
            relay_num = 2;
            button_name = "C";
        }
        #endif
        
        #ifdef RF_BUTTON_D
        if (strcmp(sequence, RF_BUTTON_D) == 0) {
            relay_num = 3;
            button_name = "D";
        }
        #endif
        
        // Unknown button code
        if (relay_num < 0) {
            ESP_LOGD(RF_TAG, "Unknown button sequence: %s", sequence);
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
}

/**
 * @brief Initialize the RF receiver
 */
void rf_receiver_init(void) {
    ESP_LOGI(RF_TAG, "Initializing RF433 receiver with RFCodes library");
    ESP_LOGI(RF_TAG, "RFCodes version: %s", RFCODES_VERSION);
    ESP_LOGI(RF_TAG, "Original author: %s", RFCODES_ORIGINAL_AUTHOR);
    
    // Initialize the signal parser
    signal_parser_init(&rf_parser);
    
    // Load protocols you want to decode
    // Uncomment/add protocols as needed:
    signal_parser_load(&rf_parser, &protocol_ev1527, 0);  // EV1527 - common remote control chip
    signal_parser_load(&rf_parser, &protocol_sc5, 0);     // SC5272 - another common chip
    signal_parser_load(&rf_parser, &protocol_it1, 0);     // Intertechno old protocol
    signal_parser_load(&rf_parser, &protocol_it2, 0);     // Intertechno new protocol
    
    // Attach callback for received codes
    signal_parser_attach_callback(&rf_parser, rf_code_received_callback);
    
    // Initialize the signal collector (handles GPIO and interrupts)
    signal_collector_init(&rf_collector, &rf_parser, RF_RCV_PIN, RF_SEND_PIN, 0);
    
    ESP_LOGI(RF_TAG, "RF receiver initialized on GPIO %d", RF_RCV_PIN);
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

/**
 * @brief Get the number of buffered RF timings
 * @return Number of pending timings in buffer
 */
uint32_t rf_get_buffer_count(void) {
    return signal_collector_get_buffer_count(&rf_collector);
}

/**
 * @brief Helper to print binary representation of a value
 * @param value Value to print
 * @param bits Number of bits to print
 */
void rf_print_binary(uint32_t value, int bits) {
    for (int i = bits - 1; i >= 0; i--) {
        putchar((value & (1 << i)) ? '1' : '0');
        if (i > 0 && i % 4 == 0) {
            putchar(' ');
        }
    }
}

#endif // RF_H