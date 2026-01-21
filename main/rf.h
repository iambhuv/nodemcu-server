// RF 433MHz Receiver
#ifndef RF_H
#define RF_H

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "gpio.h"
#include "relays.h"

#define RF_RCV_PIN 5
static QueueHandle_t rf_queue = NULL;

// This function MUST have IRAM_ATTR to avoid crashes during Flash ops
static void IRAM_ATTR rf_gpio_isr_handler(void* arg) {
  static uint32_t last_time = 0;
  uint32_t now = esp_timer_get_time(); // Get time in microseconds
  uint32_t duration = now - last_time;
  last_time = now;

  if (rf_queue != NULL) {
    xQueueSendFromISR(rf_queue, &duration, NULL);
  }
}

void print_binary(uint32_t value, int bits) {
  for (int i = bits - 1; i >= 0; i--) {
    // Use a bitmask to check if the i-th bit is set
    putchar((value & (1 << i)) ? '1' : '0');

    // Add a space every 4 bits for better "vibes" (readability)
    if (i > 0 && i % 4 == 0)
      putchar(' ');
  }
}

void rf_receiver_init(void) {
  rf_queue = xQueueCreate(128, sizeof(uint32_t));

  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << RF_RCV_PIN),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_ANYEDGE, // Watch for every pulse change
  };
  gpio_config(&io_conf);

  gpio_install_isr_service(0);
  gpio_isr_handler_add(RF_RCV_PIN, rf_gpio_isr_handler, NULL);
}

void rf_decode_task(void* pvParameters) {
  uint32_t duration;
  uint32_t current_code = 0;
  int bit_count = 0;
  bool sync_found = false;
  uint32_t first_half = 0; // Move this out of the nested logic

  static uint32_t last_valid_code = 0;
  static uint32_t last_valid_time = 0;

  while (1) {
    if (xQueueReceive(rf_queue, &duration, portMAX_DELAY)) {

      // 1. SYNC DETECTION
      if (duration > 7000 && duration < 13000) {
        sync_found = true;
        current_code = 0;
        bit_count = 0;
        first_half = 0; // RESET THIS HERE
        continue;
      }

      if (sync_found) {
        if (first_half == 0) {
          first_half = duration;
        } else {
          // PWM Decoding Logic
          if (first_half < 500 && duration > 700) {
            current_code = (current_code << 1);
          } else if (first_half > 700 && duration < 500) {
            current_code = (current_code << 1) | 1;
          } else {
            // Garbage bit - reset and wait for next sync
            sync_found = false;
            continue;
          }

          bit_count++;
          first_half = 0;

          if (bit_count == 24) {
            uint32_t now = esp_timer_get_time() / 1000;

            if (current_code != 0 && current_code != 0xFFFFFF) {
              // Only act if code matches twice within 200ms
              if (current_code == last_valid_code && (now - last_valid_time < 200)) {
                // FIXED LOGGING SYNTAX
                ESP_LOGI("RF_DECODE", "Hex: 0x%06X", (unsigned int)current_code);

                // 2. Print the Binary "Vibe"
                printf("Binary: [ ");
                print_binary(current_code, 24);
                printf(" ]\n");
                // Call your relay logic here safely
              }
              last_valid_code = current_code;
              last_valid_time = now;
            }

            sync_found = false; // Look for next packet
          }
        }
      }
    }
  }
}

#endif // RF_H