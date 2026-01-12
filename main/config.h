#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

static const char *TAG = "main";

/**
 * Enter your local wifi ssid and password
 * More advanced pairing system will be added later
 */
#define WIFI_SSID ""
#define WIFI_PASS ""

/**
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

#endif /* CONFIG_H */