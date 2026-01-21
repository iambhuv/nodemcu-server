#include "mdns.h"
#include "config.h"
#include "esp_log.h"
#include "event_groups.h"
#include "freertos/task.h"

extern EventGroupHandle_t s_wifi_event_group;

void mdns_init_service(void) {
  esp_err_t err;

  // Initialize mDNS
  err = mdns_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "mDNS init failed: %d", err);
    return;
  }

  // Set hostname
  err = mdns_hostname_set(MDNS_HOSTNAME);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set hostname: %d", err);
    return;
  }

  // Set instance name
  err = mdns_instance_name_set(MDNS_INSTANCE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set instance: %d", err);
    return;
  }

  // Add HTTP service
  err = mdns_service_add(MDNS_INSTANCE, MDNS_SERVICE, MDNS_PROTO, RELAY_PORT, mdns_txt, sizeof(mdns_txt) / sizeof(mdns_txt[0]));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add service: %d", err);
    return;
  }

  ESP_LOGI(TAG, "mDNS started: %s.local", MDNS_HOSTNAME);
}

// Main mDNS task
void mdns_task(void* pvParameters) {
  // Wait for WiFi connection
  ESP_LOGI(TAG, "Waiting for WiFi connection...");
  xEventGroupWaitBits(s_wifi_event_group, BIT0, false, true, portMAX_DELAY);

  // Wait a bit for IP to stabilize
  vTaskDelay(2000 / portTICK_PERIOD_MS);

  // Initialize mDNS once
  mdns_init_service();

  ESP_LOGI(TAG, "Device accessible at: http://%s.local", MDNS_HOSTNAME);

  // Keep task alive
  while (1) {
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}