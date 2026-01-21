#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "rf.h"
#include "server.h"
#include "relays.h"
#include "pairing.h"
#include "status_led.h"
#include "mdns.c"
#include "relay_config.h"
#include "http_server.h"
#include "alexa.h"

// Pairing button monitoring task
void pairing_button_task(void *pvParameters) {
    // Configure pairing pins
    gpio_config_t input_conf = {
        .pin_bit_mask = (1ULL << PAIRING_PIN_INPUT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&input_conf);
    
    gpio_config_t output_conf = {
        .pin_bit_mask = (1ULL << PAIRING_PIN_OUTPUT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&output_conf);
    gpio_set_level(PAIRING_PIN_OUTPUT, 0);  // Set to LOW
    
    ESP_LOGI(TAG, "Pairing button task started (touch GPIO%d and GPIO%d)", 
             PAIRING_PIN_INPUT, PAIRING_PIN_OUTPUT);
    
    while (1) {
        // Check if pairing pins are touched together
        if (gpio_get_level(PAIRING_PIN_INPUT) == 0) {  // Pulled low by output pin
            if (!pairing_is_active()) {
                ESP_LOGI(TAG, "Pairing button pressed - entering pairing mode");
                pairing_enter_mode();
                status_led_set(LED_STATUS_PAIRING);
            }
            vTaskDelay(pdMS_TO_TICKS(500));  // Debounce
        }
        
        // Check for pairing timeout
        pairing_check_timeout();
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// LED update and housekeeping task
void led_task(void *pvParameters) {
    while (1) {
        status_led_update();
        relays_check_save();         // Periodically save relay states if dirty
        relay_config_check_save();   // Periodically save relay config if dirty
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting relay controller");
    
    // Initialize status LED first
    status_led_init();
    status_led_set(LED_STATUS_BOOTING);
    
    // Initialize NVS and pairing
    pairing_init();

    // Load relay configuration (names, rooms, etc.)
    relay_config_load();

    // Initialize relays (will restore saved states)
    relays_init();
    
    // Initialize RF receiver
    rf_receiver_init();
    
    // Set LED status based on pairing state
    if (pairing_is_paired()) {
        status_led_set(LED_STATUS_NORMAL);
    } else {
        status_led_set(LED_STATUS_UNPAIRED);
    }
    
    ESP_LOGI(TAG, "ESP8266 WiFi + Web Server starting...");
    
    // Initialize WiFi
    wifi_init_sta();
    
    // WiFi connected, set normal status if paired
    if (pairing_is_paired()) {
        status_led_set(LED_STATUS_NORMAL);
    }
    
    // Start tasks
    xTaskCreate(relay_server_task, "binary_server", 4096, NULL, 5, NULL);
    xTaskCreate(http_server_task, "http_server", 4096, NULL, 5, NULL);
    xTaskCreate(mdns_task, "mdns_task", 2048, NULL, 5, NULL);
    xTaskCreate(rf_decode_task, "rf_task", 2048, NULL, 6, NULL);
    xTaskCreate(pairing_button_task, "pairing_task", 2048, NULL, 4, NULL);
    xTaskCreate(led_task, "led_task", 1024, NULL, 3, NULL);

    // Initialize Alexa support (starts its own tasks)
    alexa_init();

    ESP_LOGI(TAG, "All tasks started");
    ESP_LOGI(TAG, "Web interface: http://%s.local/", MDNS_HOSTNAME);
    ESP_LOGI(TAG, "Binary protocol: port %d", RELAY_PORT);
    ESP_LOGI(TAG, "Alexa: say 'Alexa, discover devices'");
}