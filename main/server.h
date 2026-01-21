#ifndef SERVER_H
#define SERVER_H

#include "config.h"
#include "lwip/sockets.h"
#include "protocol.h"
#include "wifi.h"
#include "relays.h"
#include "relay_config.h"

void relay_server_task(void* pvParameters) {
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  int listen_sock, client_sock;
  uint8_t recv_buf[64];
  uint8_t send_buf[64];

  xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
  ESP_LOGI(TAG, "Starting relay server on port %d", RELAY_PORT);

  listen_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sock < 0) {
    ESP_LOGE(TAG, "Failed to create socket");
    vTaskDelete(NULL);
    return;
  }

  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(RELAY_PORT);

  if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    ESP_LOGE(TAG, "Failed to bind");
    close(listen_sock);
    vTaskDelete(NULL);
    return;
  }

  if (listen(listen_sock, 5) < 0) {
    ESP_LOGE(TAG, "Failed to listen");
    close(listen_sock);
    vTaskDelete(NULL);
    return;
  }

  ESP_LOGI(TAG, "Server listening...");

  while (1) {
    client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_addr_len);

    if (client_sock < 0) {
      ESP_LOGE(TAG, "Accept failed");
      continue;
    }

    ESP_LOGI(TAG, "Client: %s", inet_ntoa(client_addr.sin_addr));

    int len = recv(client_sock, recv_buf, sizeof(recv_buf), 0);
    if (len >= sizeof(relay_request_t)) {
      relay_request_t req;

      if (proto_parse_request(recv_buf, len, &req)) {
        size_t resp_len = 0;

        switch (req.cmd) {
        case CMD_PING:
          ESP_LOGI(TAG, "PING");
          resp_len = proto_pong_response(send_buf);
          break;

        case CMD_GET_STATUS: {
          uint8_t states = 0;
          for (int i = 0; i < NUM_RELAYS; i++) {
            if (relay_get(i)) {
              states |= (1 << i);
            }
          }
          ESP_LOGI(TAG, "GET_STATUS: 0x%02X", states);
          resp_len = proto_status_response(send_buf, states);
          break;
        }

        case CMD_SET_RELAY:
          if (req.relay_id < NUM_RELAYS) {
            ESP_LOGI(TAG, "SET relay %d -> %d", req.relay_id, req.value);
            relay_set(req.relay_id, req.value != 0);
            resp_len = proto_ok_response(send_buf);
          } else {
            resp_len = proto_error_response(send_buf, 0x01); // Invalid relay
          }
          break;

        case CMD_TOGGLE_RELAY:
          if (req.relay_id < NUM_RELAYS) {
            bool new_state = !relay_get(req.relay_id);
            ESP_LOGI(TAG, "TOGGLE relay %d -> %d", req.relay_id, new_state);
            relay_set(req.relay_id, new_state);
            resp_len = proto_ok_response(send_buf);
          } else {
            resp_len = proto_error_response(send_buf, 0x01);
          }
          break;

        case CMD_SET_ALL:
          ESP_LOGI(TAG, "SET_ALL: 0x%02X", req.relay_id);
          for (int i = 0; i < NUM_RELAYS; i++) {
            relay_set(i, (req.relay_id >> i) & 1);
          }
          resp_len = proto_ok_response(send_buf);
          break;

        case CMD_DESCRIBE: {
          ESP_LOGI(TAG, "DESCRIBE");

          // Build TLV descriptor response
          uint8_t desc_data[64];
          uint8_t idx = 0;

          // Device type: "switch"
          desc_data[idx++] = DESC_DEVICE_TYPE;
          desc_data[idx++] = 6; // length
          memcpy(&desc_data[idx], "switch", 6);
          idx += 6;

          // Model: "SR-4"
          desc_data[idx++] = DESC_MODEL;
          desc_data[idx++] = 4;
          memcpy(&desc_data[idx], "SR-4", 4);
          idx += 4;

          // Relay count
          desc_data[idx++] = DESC_RELAY_COUNT;
          desc_data[idx++] = 1;
          desc_data[idx++] = (uint8_t)NUM_RELAYS; // Explicit cast

          // Capabilities (bitmask: bit 0 = relay control, bit 1 = alexa)
          desc_data[idx++] = DESC_CAPABILITIES;
          desc_data[idx++] = 1;
          desc_data[idx++] = 0x03;  // relay + alexa

          // Firmware version: "2.0.0"
          desc_data[idx++] = DESC_FW_VERSION;
          desc_data[idx++] = 5;
          memcpy(&desc_data[idx], "2.0.0", 5);
          idx += 5;

          resp_len = proto_build_response(send_buf, RESP_DESCRIBE, desc_data, idx);
          break;
        }

        case CMD_GET_RELAY_CONFIG: {
          if (req.relay_id >= NUM_RELAYS) {
            resp_len = proto_error_response(send_buf, ERR_INVALID_RELAY);
            break;
          }

          ESP_LOGI(TAG, "GET_RELAY_CONFIG: relay %d", req.relay_id);

          const relay_config_entry_t* cfg = relay_config_get(req.relay_id);
          uint8_t cfg_data[128];
          uint8_t idx = 0;

          // Relay ID
          cfg_data[idx++] = CFG_RELAY_ID;
          cfg_data[idx++] = 1;
          cfg_data[idx++] = req.relay_id;

          // Name
          uint8_t name_len = strlen(cfg->name);
          cfg_data[idx++] = CFG_RELAY_NAME;
          cfg_data[idx++] = name_len;
          memcpy(&cfg_data[idx], cfg->name, name_len);
          idx += name_len;

          // Room
          uint8_t room_len = strlen(cfg->room);
          cfg_data[idx++] = CFG_RELAY_ROOM;
          cfg_data[idx++] = room_len;
          memcpy(&cfg_data[idx], cfg->room, room_len);
          idx += room_len;

          // Icon
          cfg_data[idx++] = CFG_RELAY_ICON;
          cfg_data[idx++] = 1;
          cfg_data[idx++] = cfg->icon;

          // Alexa enabled
          cfg_data[idx++] = CFG_RELAY_ALEXA;
          cfg_data[idx++] = 1;
          cfg_data[idx++] = cfg->alexa_enabled;

          // Current state
          cfg_data[idx++] = CFG_RELAY_STATE;
          cfg_data[idx++] = 1;
          cfg_data[idx++] = relay_get(req.relay_id);

          resp_len = proto_build_response(send_buf, RESP_CONFIG, cfg_data, idx);
          break;
        }

        case CMD_SET_RELAY_NAME: {
          if (req.relay_id >= NUM_RELAYS) {
            resp_len = proto_error_response(send_buf, ERR_INVALID_RELAY);
            break;
          }

          // Name follows the 4-byte header
          if (len > sizeof(relay_request_t)) {
            uint8_t name_len = len - sizeof(relay_request_t);
            if (name_len >= RELAY_NAME_MAX_LEN) {
              resp_len = proto_error_response(send_buf, ERR_NAME_TOO_LONG);
              break;
            }

            char name[RELAY_NAME_MAX_LEN] = {0};
            memcpy(name, &recv_buf[sizeof(relay_request_t)], name_len);
            name[name_len] = '\0';

            ESP_LOGI(TAG, "SET_RELAY_NAME: relay %d -> '%s'", req.relay_id, name);
            relay_config_set_name(req.relay_id, name);
            resp_len = proto_ok_response(send_buf);
          } else {
            resp_len = proto_error_response(send_buf, ERR_INVALID_VALUE);
          }
          break;
        }

        case CMD_SET_RELAY_ROOM: {
          if (req.relay_id >= NUM_RELAYS) {
            resp_len = proto_error_response(send_buf, ERR_INVALID_RELAY);
            break;
          }

          if (len > sizeof(relay_request_t)) {
            uint8_t room_len = len - sizeof(relay_request_t);
            if (room_len >= RELAY_ROOM_MAX_LEN) {
              resp_len = proto_error_response(send_buf, ERR_NAME_TOO_LONG);
              break;
            }

            char room[RELAY_ROOM_MAX_LEN] = {0};
            memcpy(room, &recv_buf[sizeof(relay_request_t)], room_len);
            room[room_len] = '\0';

            ESP_LOGI(TAG, "SET_RELAY_ROOM: relay %d -> '%s'", req.relay_id, room);
            relay_config_set_room(req.relay_id, room);
            resp_len = proto_ok_response(send_buf);
          } else {
            resp_len = proto_error_response(send_buf, ERR_INVALID_VALUE);
          }
          break;
        }

        case CMD_SET_RELAY_ICON: {
          if (req.relay_id >= NUM_RELAYS) {
            resp_len = proto_error_response(send_buf, ERR_INVALID_RELAY);
            break;
          }

          ESP_LOGI(TAG, "SET_RELAY_ICON: relay %d -> %d", req.relay_id, req.value);
          relay_config_set_icon(req.relay_id, req.value);
          resp_len = proto_ok_response(send_buf);
          break;
        }

        case CMD_SET_RELAY_ALEXA: {
          if (req.relay_id >= NUM_RELAYS) {
            resp_len = proto_error_response(send_buf, ERR_INVALID_RELAY);
            break;
          }

          ESP_LOGI(TAG, "SET_RELAY_ALEXA: relay %d -> %d", req.relay_id, req.value);
          relay_config_set_alexa(req.relay_id, req.value != 0);
          resp_len = proto_ok_response(send_buf);
          break;
        }

        case CMD_GET_ALL_CONFIG: {
          ESP_LOGI(TAG, "GET_ALL_CONFIG");

          // Build response with all relay configs
          // Format: [count:1] then for each: [id:1][name_len:1][name:N][state:1]
          uint8_t cfg_data[200];
          uint8_t idx = 0;

          cfg_data[idx++] = NUM_RELAYS;

          for (int i = 0; i < NUM_RELAYS; i++) {
            const relay_config_entry_t* cfg = relay_config_get(i);

            cfg_data[idx++] = i;  // relay id

            uint8_t name_len = strlen(cfg->name);
            cfg_data[idx++] = name_len;
            memcpy(&cfg_data[idx], cfg->name, name_len);
            idx += name_len;

            cfg_data[idx++] = relay_get(i);  // state
            cfg_data[idx++] = cfg->alexa_enabled;
          }

          resp_len = proto_build_response(send_buf, RESP_CONFIG, cfg_data, idx);
          break;
        }

        default:
          ESP_LOGW(TAG, "Unknown command: 0x%02X", req.cmd);
          resp_len = proto_error_response(send_buf, ERR_UNKNOWN_CMD); // Unknown command
        }

        if (resp_len > 0) {
          send(client_sock, send_buf, resp_len, 0);
        }
      } else {
        ESP_LOGW(TAG, "Invalid magic byte");
        size_t resp_len = proto_error_response(send_buf, 0xFF);
        send(client_sock, send_buf, resp_len, 0);
      }
    }

    close(client_sock);
  }
}

#endif // SERVER_H