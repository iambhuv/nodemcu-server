#ifndef CONFIG_H
#define CONFIG_H

#include "mdns.h"
#include <stdint.h>

static const char* TAG = "switch-2";

#define MDNS_HOSTNAME "switch-2"
#define MDNS_INSTANCE "switch_2"
#define MDNS_SERVICE "_homeiot"
#define MDNS_PROTO "_tcp"

static const mdns_txt_item_t mdns_txt[] = {
    {"type", "switch"},
    {"relays", "4"},
    {"proto", "v2"},
    {"fw", "1.1.0"},
    {"alexa", "yes"},
};

/**
 * Enter your local wifi ssid and password
 * More advanced pairing system will be added later
 */
#define WIFI_SSID ""
#define WIFI_PASS ""

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
 * Pairing Button Configuration
 * 
 * Touch these two wires/pins together to enter pairing mode
 * One pin will be set as INPUT_PULLUP, the other as OUTPUT LOW
 * When they touch, it triggers pairing mode
 */
#define PAIRING_PIN_INPUT  0   // GPIO0 (D3) - with pullup
#define PAIRING_PIN_OUTPUT 16  // GPIO16 (D0) - set to LOW

/**
 * RF433 Remote Configuration (EV1527 Protocol)
 * 
 * Only supports remotes with shared address (proper EV1527 implementation)
 * Remote address is learned via pairing mode and stored in NVS
 * 
 * To pair a new remote:
 * 1. Touch pairing wires together (LED will blink fast)
 * 2. Press any button on remote within 30 seconds
 * 3. Remote is now paired (LED goes solid)
 * 
 * Button data bits expected:
 * A = 1000 (0x8)
 * B = 0100 (0x4)
 * C = 0010 (0x2)
 * D = 0001 (0x1)
 */

/**
 * RF Hold Detection
 * Minimum time (ms) between toggles to prevent rapid toggling when button is held
 */
#define RF_HOLD_TIMEOUT_MS 500

#endif /* CONFIG_H */