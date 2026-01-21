/**
 * @file http_server.h
 * @brief HTTP REST API server for relay control and configuration
 *
 * Provides:
 * - GET /api/status - Get all relay states and names
 * - POST /api/relay/{id}/on - Turn relay on
 * - POST /api/relay/{id}/off - Turn relay off
 * - POST /api/relay/{id}/toggle - Toggle relay
 * - PUT /api/relay/{id}/name - Rename relay (body: new name)
 * - PUT /api/relay/{id}/room - Set room (body: room name)
 * - GET / - Serve web interface
 */

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "lwip/sockets.h"
#include "esp_log.h"
#include "wifi.h"
#include "relays.h"
#include "relay_config.h"

#define HTTP_PORT 80
#define HTTP_TAG "HTTP"
#define HTTP_RECV_BUF_SIZE 512
#define HTTP_SEND_BUF_SIZE 1024

// Simple HTTP response helpers
static const char* HTTP_200 = "HTTP/1.1 200 OK\r\n";
static const char* HTTP_204 = "HTTP/1.1 204 No Content\r\n";
static const char* HTTP_400 = "HTTP/1.1 400 Bad Request\r\n";
static const char* HTTP_404 = "HTTP/1.1 404 Not Found\r\n";
static const char* CONTENT_JSON = "Content-Type: application/json\r\n";
static const char* CONTENT_HTML = "Content-Type: text/html\r\n";
static const char* CORS_HEADERS = "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, PUT, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\n";
static const char* CONN_CLOSE = "Connection: close\r\n\r\n";

/**
 * @brief Build JSON status response
 */
static int http_build_status_json(char* buf, size_t buf_size) {
    int offset = 0;

    offset += snprintf(buf + offset, buf_size - offset,
        "{\"device\":{\"name\":\"%s\",\"model\":\"SR-4\",\"fw\":\"2.0.0\"},\"relays\":[",
        MDNS_HOSTNAME);

    for (int i = 0; i < NUM_RELAYS; i++) {
        const relay_config_entry_t* cfg = relay_config_get(i);
        offset += snprintf(buf + offset, buf_size - offset,
            "%s{\"id\":%d,\"name\":\"%s\",\"room\":\"%s\",\"state\":%d,\"icon\":%d,\"alexa\":%s}",
            i > 0 ? "," : "",
            i,
            cfg->name,
            cfg->room,
            relay_get(i),
            cfg->icon,
            cfg->alexa_enabled ? "true" : "false");
    }

    offset += snprintf(buf + offset, buf_size - offset, "]}");
    return offset;
}

/**
 * @brief Build single relay JSON response
 */
static int http_build_relay_json(char* buf, size_t buf_size, uint8_t relay_id) {
    const relay_config_entry_t* cfg = relay_config_get(relay_id);
    return snprintf(buf, buf_size,
        "{\"id\":%d,\"name\":\"%s\",\"room\":\"%s\",\"state\":%d,\"icon\":%d,\"alexa\":%s}",
        relay_id,
        cfg->name,
        cfg->room,
        relay_get(relay_id),
        cfg->icon,
        cfg->alexa_enabled ? "true" : "false");
}

/**
 * @brief Embedded web interface HTML
 */
static const char HTTP_INDEX_HTML[] =
"<!DOCTYPE html>"
"<html lang=\"en\">"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>Smart Switch</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:-apple-system,system-ui,sans-serif;background:#1a1a2e;color:#eee;min-height:100vh;padding:20px}"
"h1{text-align:center;margin-bottom:20px;font-weight:300;color:#fff}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px;max-width:800px;margin:0 auto}"
".card{background:#16213e;border-radius:12px;padding:20px;transition:transform .2s}"
".card:hover{transform:translateY(-2px)}"
".card-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:16px}"
".card-name{font-size:1.2em;font-weight:500;cursor:pointer;border:none;background:transparent;color:#fff;padding:4px 8px;border-radius:4px}"
".card-name:hover{background:#0f3460}"
".card-name:focus{outline:2px solid #e94560;background:#0f3460}"
".card-room{font-size:0.85em;color:#888;cursor:pointer}"
".toggle{width:64px;height:34px;background:#333;border-radius:17px;position:relative;cursor:pointer;transition:background .3s}"
".toggle.on{background:#e94560}"
".toggle::after{content:'';position:absolute;width:28px;height:28px;background:#fff;border-radius:50%;top:3px;left:3px;transition:left .3s}"
".toggle.on::after{left:33px}"
".status{display:flex;gap:12px;font-size:.9em;color:#888}"
".alexa{display:flex;align-items:center;gap:8px;margin-top:12px;font-size:.85em}"
".alexa input{width:18px;height:18px}"
".footer{text-align:center;margin-top:30px;color:#666;font-size:.85em}"
"</style>"
"</head>"
"<body>"
"<h1>Smart Switch Control</h1>"
"<div class=\"grid\" id=\"relays\"></div>"
"<div class=\"footer\">SR-4 | Firmware 2.0.0</div>"
"<script>"
"const api='/api';"
"async function load(){"
"const r=await fetch(api+'/status');"
"const d=await r.json();"
"const c=document.getElementById('relays');"
"c.innerHTML=d.relays.map(relay=>`"
"<div class=\"card\" data-id=\"${relay.id}\">"
"<div class=\"card-header\">"
"<input class=\"card-name\" value=\"${relay.name}\" onchange=\"rename(${relay.id},this.value)\">"
"<div class=\"toggle ${relay.state?'on':''}\" onclick=\"toggle(${relay.id})\"></div>"
"</div>"
"<div class=\"status\">"
"<span class=\"card-room\" onclick=\"setRoom(${relay.id})\">${relay.room}</span>"
"</div>"
"<label class=\"alexa\"><input type=\"checkbox\" ${relay.alexa?'checked':''} onchange=\"setAlexa(${relay.id},this.checked)\">Alexa enabled</label>"
"</div>"
"`).join('')}"
"async function toggle(id){"
"await fetch(`${api}/relay/${id}/toggle`,{method:'POST'});"
"const t=document.querySelector(`[data-id=\"${id}\"] .toggle`);"
"t.classList.toggle('on')}"
"async function rename(id,name){"
"await fetch(`${api}/relay/${id}/name`,{method:'PUT',body:name})}"
"async function setRoom(id){"
"const room=prompt('Enter room name:');"
"if(room){await fetch(`${api}/relay/${id}/room`,{method:'PUT',body:room});load()}}"
"async function setAlexa(id,enabled){"
"await fetch(`${api}/relay/${id}/alexa`,{method:'PUT',body:enabled?'1':'0'})}"
"load();setInterval(load,5000)"
"</script>"
"</body>"
"</html>";

/**
 * @brief Parse HTTP request method and path
 */
static bool http_parse_request(const char* buf, char* method, size_t method_size,
                                char* path, size_t path_size, char* body, size_t body_size) {
    // Parse first line: "METHOD /path HTTP/1.x"
    const char* space1 = strchr(buf, ' ');
    if (!space1) return false;

    size_t method_len = space1 - buf;
    if (method_len >= method_size) method_len = method_size - 1;
    strncpy(method, buf, method_len);
    method[method_len] = '\0';

    const char* path_start = space1 + 1;
    const char* space2 = strchr(path_start, ' ');
    if (!space2) return false;

    size_t path_len = space2 - path_start;
    if (path_len >= path_size) path_len = path_size - 1;
    strncpy(path, path_start, path_len);
    path[path_len] = '\0';

    // Find body (after \r\n\r\n)
    const char* body_start = strstr(buf, "\r\n\r\n");
    if (body_start && body_size > 0) {
        body_start += 4;
        strncpy(body, body_start, body_size - 1);
        body[body_size - 1] = '\0';
    } else if (body_size > 0) {
        body[0] = '\0';
    }

    return true;
}

/**
 * @brief Extract relay ID from path like /api/relay/2/toggle
 */
static int http_extract_relay_id(const char* path) {
    // Find /relay/X pattern
    const char* relay_pos = strstr(path, "/relay/");
    if (!relay_pos) return -1;

    relay_pos += 7;  // Skip "/relay/"
    int id = atoi(relay_pos);

    if (id < 0 || id >= NUM_RELAYS) return -1;
    return id;
}

/**
 * @brief Handle HTTP request
 */
static void http_handle_request(int client_sock, const char* recv_buf) {
    char method[8] = {0};
    char path[64] = {0};
    char body[128] = {0};
    char send_buf[HTTP_SEND_BUF_SIZE];
    int send_len = 0;

    if (!http_parse_request(recv_buf, method, sizeof(method),
                            path, sizeof(path), body, sizeof(body))) {
        send_len = snprintf(send_buf, sizeof(send_buf), "%s%s%s", HTTP_400, CONN_CLOSE, "Bad Request");
        send(client_sock, send_buf, send_len, 0);
        return;
    }

    ESP_LOGI(HTTP_TAG, "%s %s", method, path);

    // Handle CORS preflight
    if (strcmp(method, "OPTIONS") == 0) {
        send_len = snprintf(send_buf, sizeof(send_buf), "%s%s%s", HTTP_204, CORS_HEADERS, CONN_CLOSE);
        send(client_sock, send_buf, send_len, 0);
        return;
    }

    // GET / - Serve web interface
    if (strcmp(method, "GET") == 0 && (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)) {
        send_len = snprintf(send_buf, sizeof(send_buf),
            "%s%sContent-Length: %d\r\n%s",
            HTTP_200, CONTENT_HTML, (int)sizeof(HTTP_INDEX_HTML) - 1, CONN_CLOSE);
        send(client_sock, send_buf, send_len, 0);
        send(client_sock, HTTP_INDEX_HTML, sizeof(HTTP_INDEX_HTML) - 1, 0);
        return;
    }

    // GET /api/status - Get all relay states
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/status") == 0) {
        char json_buf[512];
        int json_len = http_build_status_json(json_buf, sizeof(json_buf));

        send_len = snprintf(send_buf, sizeof(send_buf),
            "%s%s%sContent-Length: %d\r\n%s%s",
            HTTP_200, CONTENT_JSON, CORS_HEADERS, json_len, CONN_CLOSE, json_buf);
        send(client_sock, send_buf, send_len, 0);
        return;
    }

    // POST /api/relay/{id}/on
    if (strcmp(method, "POST") == 0 && strstr(path, "/on")) {
        int id = http_extract_relay_id(path);
        if (id >= 0) {
            relay_set(id, 1);
            char json_buf[128];
            int json_len = http_build_relay_json(json_buf, sizeof(json_buf), id);
            send_len = snprintf(send_buf, sizeof(send_buf),
                "%s%s%sContent-Length: %d\r\n%s%s",
                HTTP_200, CONTENT_JSON, CORS_HEADERS, json_len, CONN_CLOSE, json_buf);
            send(client_sock, send_buf, send_len, 0);
            return;
        }
    }

    // POST /api/relay/{id}/off
    if (strcmp(method, "POST") == 0 && strstr(path, "/off")) {
        int id = http_extract_relay_id(path);
        if (id >= 0) {
            relay_set(id, 0);
            char json_buf[128];
            int json_len = http_build_relay_json(json_buf, sizeof(json_buf), id);
            send_len = snprintf(send_buf, sizeof(send_buf),
                "%s%s%sContent-Length: %d\r\n%s%s",
                HTTP_200, CONTENT_JSON, CORS_HEADERS, json_len, CONN_CLOSE, json_buf);
            send(client_sock, send_buf, send_len, 0);
            return;
        }
    }

    // POST /api/relay/{id}/toggle
    if (strcmp(method, "POST") == 0 && strstr(path, "/toggle")) {
        int id = http_extract_relay_id(path);
        if (id >= 0) {
            relay_set(id, !relay_get(id));
            char json_buf[128];
            int json_len = http_build_relay_json(json_buf, sizeof(json_buf), id);
            send_len = snprintf(send_buf, sizeof(send_buf),
                "%s%s%sContent-Length: %d\r\n%s%s",
                HTTP_200, CONTENT_JSON, CORS_HEADERS, json_len, CONN_CLOSE, json_buf);
            send(client_sock, send_buf, send_len, 0);
            return;
        }
    }

    // PUT /api/relay/{id}/name
    if (strcmp(method, "PUT") == 0 && strstr(path, "/name")) {
        int id = http_extract_relay_id(path);
        if (id >= 0 && strlen(body) > 0) {
            relay_config_set_name(id, body);
            char json_buf[128];
            int json_len = http_build_relay_json(json_buf, sizeof(json_buf), id);
            send_len = snprintf(send_buf, sizeof(send_buf),
                "%s%s%sContent-Length: %d\r\n%s%s",
                HTTP_200, CONTENT_JSON, CORS_HEADERS, json_len, CONN_CLOSE, json_buf);
            send(client_sock, send_buf, send_len, 0);
            return;
        }
    }

    // PUT /api/relay/{id}/room
    if (strcmp(method, "PUT") == 0 && strstr(path, "/room")) {
        int id = http_extract_relay_id(path);
        if (id >= 0 && strlen(body) > 0) {
            relay_config_set_room(id, body);
            char json_buf[128];
            int json_len = http_build_relay_json(json_buf, sizeof(json_buf), id);
            send_len = snprintf(send_buf, sizeof(send_buf),
                "%s%s%sContent-Length: %d\r\n%s%s",
                HTTP_200, CONTENT_JSON, CORS_HEADERS, json_len, CONN_CLOSE, json_buf);
            send(client_sock, send_buf, send_len, 0);
            return;
        }
    }

    // PUT /api/relay/{id}/alexa
    if (strcmp(method, "PUT") == 0 && strstr(path, "/alexa")) {
        int id = http_extract_relay_id(path);
        if (id >= 0) {
            bool enabled = (body[0] == '1' || body[0] == 't');
            relay_config_set_alexa(id, enabled);
            char json_buf[128];
            int json_len = http_build_relay_json(json_buf, sizeof(json_buf), id);
            send_len = snprintf(send_buf, sizeof(send_buf),
                "%s%s%sContent-Length: %d\r\n%s%s",
                HTTP_200, CONTENT_JSON, CORS_HEADERS, json_len, CONN_CLOSE, json_buf);
            send(client_sock, send_buf, send_len, 0);
            return;
        }
    }

    // 404 Not Found
    const char* not_found = "{\"error\":\"Not Found\"}";
    send_len = snprintf(send_buf, sizeof(send_buf),
        "%s%s%sContent-Length: %d\r\n%s%s",
        HTTP_404, CONTENT_JSON, CORS_HEADERS, (int)strlen(not_found), CONN_CLOSE, not_found);
    send(client_sock, send_buf, send_len, 0);
}

/**
 * @brief HTTP server task
 */
void http_server_task(void* pvParameters) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int listen_sock, client_sock;
    char recv_buf[HTTP_RECV_BUF_SIZE];

    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(HTTP_TAG, "Starting HTTP server on port %d", HTTP_PORT);

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        ESP_LOGE(HTTP_TAG, "Failed to create socket");
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(HTTP_PORT);

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(HTTP_TAG, "Failed to bind");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_sock, 3) < 0) {
        ESP_LOGE(HTTP_TAG, "Failed to listen");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(HTTP_TAG, "HTTP server listening...");

    while (1) {
        client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_addr_len);

        if (client_sock < 0) {
            ESP_LOGE(HTTP_TAG, "Accept failed");
            continue;
        }

        // Set receive timeout
        struct timeval timeout = {.tv_sec = 2, .tv_usec = 0};
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        memset(recv_buf, 0, sizeof(recv_buf));
        int len = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);

        if (len > 0) {
            http_handle_request(client_sock, recv_buf);
        }

        close(client_sock);
    }
}

#endif // HTTP_SERVER_H
