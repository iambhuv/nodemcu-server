#ifndef RELAYS_H
#define RELAYS_H

#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "pairing.h"

// Current relay states
static uint8_t relay_states[NUM_RELAYS] = {0};

void relays_init(void) {
  gpio_config_t io_conf = {
      .pin_bit_mask = 0,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };

  // Build the pin mask
  for (int i = 0; i < NUM_RELAYS; i++) {
    io_conf.pin_bit_mask |= (1ULL << relays[i]);
  }

  gpio_config(&io_conf);

  // Try to load saved states from NVS
  if (pairing_load_relay_states(relay_states, NUM_RELAYS)) {
    ESP_LOGI(TAG, "Restored relay states from NVS");
    // Apply restored states
    for (int i = 0; i < NUM_RELAYS; i++) {
      gpio_set_level(relays[i], relay_states[i]);
      ESP_LOGI(TAG, "Relay %d (GPIO %d) restored -> %s", i + 1, relays[i], 
               relay_states[i] ? "ON" : "OFF");
    }
  } else {
    ESP_LOGI(TAG, "No saved states, initializing relays to OFF");
    // Turn all off initially
    for (int i = 0; i < NUM_RELAYS; i++) {
      relay_states[i] = 0;
      gpio_set_level(relays[i], 0);
    }
  }
}

// Control by relay number (0-based index)
void relay_set(uint8_t relay_num, uint8_t state) {
  if (relay_num >= NUM_RELAYS) {
    ESP_LOGE(TAG, "Invalid relay number: %d", relay_num);
    return;
  }

  uint8_t pin = relays[relay_num];
  gpio_set_level(pin, state);
  relay_states[relay_num] = state;

  // Save state to NVS
  pairing_save_relay_states(relay_states, NUM_RELAYS);

  ESP_LOGI(TAG, "Relay %d (GPIO %d) -> %s", relay_num + 1, pin, state ? "ON" : "OFF");
}

// Get relay state
uint8_t relay_get(uint8_t relay_num) {
  if (relay_num >= NUM_RELAYS) {
    return 0;
  }
  return relay_states[relay_num];
}

#endif /* RELAYS_H */