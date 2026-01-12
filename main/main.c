#include "esp_log.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "relays.h"
#include "server.c"
#include "mdns.c"
#include <stdio.h>

void app_main(void) {
  ESP_LOGI(TAG, "Starting relay controller");

  esp_err_t ret = nvs_flash_init();

  relays_init();

  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(TAG, "ESP8266 WiFi + Web Server starting...");

  // Initialize WiFi
  wifi_init_sta();

  // Start web server task
  xTaskCreate(relay_server_task, "http_server", 4096, NULL, 5, NULL);
  xTaskCreate(mdns_task, "mdns_task", 2048, NULL, 5, NULL);
}