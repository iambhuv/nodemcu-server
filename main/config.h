#ifndef CONFIG_H
#define CONFIG_H

#include "mdns.h"
#include <stdint.h>

static const char* TAG = "switch-1";

#define MDNS_HOSTNAME "switch-1"
#define MDNS_INSTANCE "switch_1"
#define MDNS_SERVICE "_homeiot"
#define MDNS_PROTO "_tcp"

static const mdns_txt_item_t mdns_txt[] = {
    {"type", "switch"},
    {"relays", "4"},
    {"proto", "v1"},
    {"fw", "1.0.0"},
};

/**
 * Enter your local wifi ssid and password
 * More advanced pairing system will be added later
 */
#define WIFI_SSID "Coding Wala 4G"
#define WIFI_PASS "Coding@2022"

/*
 * Port on which TCP server will run
 */
#define RELAY_PORT 3736

/**
 * GPIO pin number for relays in order
 *
 * Data Pin to GPIO Map (for ESP8266)
 *
 * - D0 -> 16
 * - D1 ->  5
 * - D2 ->  4
 * - D3 ->  0
 * - D4 ->  2
 * - D5 -> 14
 * - D6 -> 12
 * - D7 -> 13
 * - D8 -> 15
 *
 */
static const uint8_t relays[] = {4, 14, 12, 13};

/**
 * Number of relays currently available
 */
#define NUM_RELAYS (sizeof(relays) / sizeof(relays[0]))

/**
 * RF433 Remote Configuration (EV1527 Protocol)
 * 
 * Each button on your remote sends a unique 24-bit code.
 * To configure, press each button and copy the sequence from logs.
 * 
 * Format: "s<20 address bits><4 data bits>"
 * 
 * Set to NULL to disable a button.
 */
#define RF_BUTTON_A "s010101010101010100001100"  // Button A -> Relay 1
#define RF_BUTTON_B "s010101010101010100000011"  // Button B -> Relay 2
#define RF_BUTTON_C "s010101010101010111000000"  // Button C -> Relay 3
#define RF_BUTTON_D "s010101010101010100110000"  // Button D -> Relay 4

/**
 * RF Hold Detection
 * Minimum time (ms) between toggles to prevent rapid toggling when button is held
 */
#define RF_HOLD_TIMEOUT_MS 500

#endif /* CONFIG_H */