/**
 * @file status_led.h
 * @brief Status LED indicator for system state
 */

#ifndef STATUS_LED_H
#define STATUS_LED_H

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "config.h"

#define STATUS_LED_PIN 2  // GPIO2 - Built-in LED on most ESP8266 boards (D4)

typedef enum {
    LED_STATUS_BOOTING,      // Very fast blink (100ms) - WiFi connecting
    LED_STATUS_UNPAIRED,     // Slow blink (1000ms) - No remote paired
    LED_STATUS_PAIRING,      // Fast blink (250ms) - Pairing mode active
    LED_STATUS_NORMAL        // Solid OFF - Paired and stable
} led_status_t;

static led_status_t current_led_status = LED_STATUS_BOOTING;
static uint32_t last_led_toggle = 0;
static bool led_state = false;

/**
 * @brief Initialize status LED
 */
void status_led_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << STATUS_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(STATUS_LED_PIN, 1);  // LED off (active low on most ESP8266)
}

/**
 * @brief Set LED status mode
 */
void status_led_set(led_status_t status) {
    current_led_status = status;
    
    // If switching to normal mode, turn LED off immediately
    if (status == LED_STATUS_NORMAL) {
        gpio_set_level(STATUS_LED_PIN, 1);  // OFF
        led_state = false;
    }
}

/**
 * @brief Update LED state (call from main loop)
 */
void status_led_update(void) {
    uint32_t now = esp_timer_get_time() / 1000;  // Convert to ms
    uint32_t interval = 0;
    
    switch (current_led_status) {
        case LED_STATUS_BOOTING:
            interval = 100;  // Very fast blink
            break;
        case LED_STATUS_UNPAIRED:
            interval = 1000;  // Slow blink
            break;
        case LED_STATUS_PAIRING:
            interval = 250;  // Fast blink
            break;
        case LED_STATUS_NORMAL:
            return;  // No blinking, stay off
    }
    
    if (now - last_led_toggle >= interval) {
        led_state = !led_state;
        gpio_set_level(STATUS_LED_PIN, led_state ? 0 : 1);  // Inverted (active low)
        last_led_toggle = now;
    }
}

#endif // STATUS_LED_H
