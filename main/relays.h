#ifndef RELAYS_H
#define RELAYS_H

#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"

void relays_init(void) {
  gpio_config_t io_conf = {
    .pin_bit_mask = 0,
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };

  // Build the pin mask
  for (int i = 0; i < NUM_RELAYS; i++) {
    io_conf.pin_bit_mask |= (1ULL << relays[i]);
  }

  gpio_config(&io_conf);

  // Turn all off initially
  for (int i = 0; i < NUM_RELAYS; i++) {
    gpio_set_level(relays[i], 0);
  }
}

// Control by relay number (1-4)
void relay_set(uint8_t relay_num, uint8_t state) {
  if (relay_num < 0 || relay_num > NUM_RELAYS) {
    ESP_LOGE(TAG, "Invalid relay number: %d", relay_num);
    return;
  }

  uint8_t pin = relays[relay_num];
  gpio_set_level(pin, state);
  
  ESP_LOGI(TAG, "Relay %d (GPIO %d) -> %s", relay_num, pin, state ? "ON" : "OFF");
}

// Get relay state
uint8_t relay_get(uint8_t relay_num) {
  if (relay_num > NUM_RELAYS) {
    return 0;
  }
  return gpio_get_level(relays[relay_num]);
}

#endif /* RELAYS_H */