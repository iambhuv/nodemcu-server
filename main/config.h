#ifndef CONFIG_H
#define CONFIG_H

#include "mdns.h"
#include <stdint.h>

static const char *TAG = "switch-1";

static const mdns_txt_item_t mdns_txt[] = {
    { "type",   "switch" },
    { "relays", "4" },
    { "proto",  "v1" },
    { "fw",     "1.0.0" },
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

#endif /* CONFIG_H */