/**
 * @file alexa.h
 * @brief Alexa support via Belkin WeMo emulation
 *
 * Implements UPnP/SSDP discovery and SOAP control endpoints to make
 * each relay appear as a separate WeMo smart plug to Alexa.
 *
 * How it works:
 * 1. SSDP server listens for M-SEARCH broadcasts from Alexa
 * 2. Responds with device info for each Alexa-enabled relay
 * 3. HTTP endpoints serve setup.xml and handle SOAP on/off commands
 *
 * Discovery: "Alexa, discover devices"
 * Control: "Alexa, turn on [relay name]"
 */

#ifndef ALEXA_H
#define ALEXA_H

#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "lwip/sockets.h"
#include "lwip/igmp.h"
#include "esp_log.h"
#include "wifi.h"
#include "relays.h"
#include "relay_config.h"

#define ALEXA_TAG "ALEXA"

// SSDP multicast address and port
#define SSDP_MULTICAST_ADDR "239.255.255.250"
#define SSDP_PORT 1900

// Base port for WeMo HTTP servers (one per relay: 49152, 49153, ...)
#define WEMO_BASE_PORT 49152

// Device serial number prefix (combined with relay ID for uniqueness)
#define DEVICE_SERIAL_PREFIX "SR4"

// Generate unique device ID for each relay
static void alexa_get_device_id(uint8_t relay_id, char* buf, size_t buf_size) {
    // Create a unique ID based on MAC address + relay ID
    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    snprintf(buf, buf_size, "%02x%02x%02x%02x%02x%02x-%d",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], relay_id);
}

// Generate UUID for device
static void alexa_get_uuid(uint8_t relay_id, char* buf, size_t buf_size) {
    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    snprintf(buf, buf_size, "Socket-1_0-%02X%02X%02X%02X%02X%02XR%d",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], relay_id);
}

// WeMo setup.xml template
static const char WEMO_SETUP_XML[] =
"<?xml version=\"1.0\"?>"
"<root xmlns=\"urn:Belkin:device-1-0\">"
"<specVersion><major>1</major><minor>0</minor></specVersion>"
"<device>"
"<deviceType>urn:Belkin:device:controllee:1</deviceType>"
"<friendlyName>%s</friendlyName>"
"<manufacturer>Belkin International Inc.</manufacturer>"
"<manufacturerURL>http://www.belkin.com</manufacturerURL>"
"<modelDescription>Belkin Plugin Socket 1.0</modelDescription>"
"<modelName>Socket</modelName>"
"<modelNumber>1.0</modelNumber>"
"<modelURL>http://www.belkin.com/plugin/</modelURL>"
"<serialNumber>%s%d</serialNumber>"
"<UDN>uuid:%s</UDN>"
"<UPC>123456789</UPC>"
"<serviceList>"
"<service>"
"<serviceType>urn:Belkin:service:basicevent:1</serviceType>"
"<serviceId>urn:Belkin:serviceId:basicevent1</serviceId>"
"<controlURL>/upnp/control/basicevent1</controlURL>"
"<eventSubURL>/upnp/event/basicevent1</eventSubURL>"
"<SCPDURL>/eventservice.xml</SCPDURL>"
"</service>"
"</serviceList>"
"</device>"
"</root>";

// SSDP response template
static const char SSDP_RESPONSE[] =
"HTTP/1.1 200 OK\r\n"
"CACHE-CONTROL: max-age=86400\r\n"
"DATE: Sat, 01 Jan 2000 00:00:00 GMT\r\n"
"EXT:\r\n"
"LOCATION: http://%s:%d/setup.xml\r\n"
"OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
"01-NLS: %s\r\n"
"SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
"ST: urn:Belkin:device:**\r\n"
"USN: uuid:%s::urn:Belkin:device:**\r\n"
"\r\n";

// SOAP response for GetBinaryState
static const char SOAP_GET_STATE_RESPONSE[] =
"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
"<s:Body><u:GetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">"
"<BinaryState>%d</BinaryState></u:GetBinaryStateResponse></s:Body></s:Envelope>";

// SOAP response for SetBinaryState
static const char SOAP_SET_STATE_RESPONSE[] =
"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
"<s:Body><u:SetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">"
"<BinaryState>%d</BinaryState></u:SetBinaryStateResponse></s:Body></s:Envelope>";

// State for each WeMo virtual device
typedef struct {
    int listen_sock;
    char uuid[48];
    uint16_t port;
    uint8_t relay_id;
    uint8_t _pad; // Ensure size is multiple of 4 (56 bytes) for array alignment
} wemo_device_t;

static wemo_device_t wemo_devices[NUM_RELAYS] __attribute__((aligned(4)));
static char device_ip[16] = {0};

/**
 * @brief Get local IP address as string
 */
static void alexa_get_local_ip(void) {
    tcpip_adapter_ip_info_t ip_info;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
    snprintf(device_ip, sizeof(device_ip), IPSTR, IP2STR(&ip_info.ip));
}

/**
 * @brief Handle WeMo HTTP request for a specific relay
 */
static void alexa_handle_wemo_request(int client_sock, uint8_t relay_id, const char* request) {
    char response[1024];
    char body[1024];
    int body_len = 0;
    const char* content_type = "text/xml";

    ESP_LOGD(ALEXA_TAG, "WeMo request for relay %d", relay_id);

    // GET /setup.xml - Device description
    if (strstr(request, "GET /setup.xml") || strstr(request, "GET / ")) {
        char uuid[48];
        alexa_get_uuid(relay_id, uuid, sizeof(uuid));
        const char* name = relay_config_get_name(relay_id);

        body_len = snprintf(body, sizeof(body), WEMO_SETUP_XML,
                           name, DEVICE_SERIAL_PREFIX, relay_id, uuid);

        ESP_LOGI(ALEXA_TAG, "Serving setup.xml for '%s' (relay %d)", name, relay_id);
    }
    // POST /upnp/control/basicevent1 - SOAP control
    else if (strstr(request, "POST /upnp/control/basicevent1")) {
        // Check for SetBinaryState
        if (strstr(request, "SetBinaryState")) {
            // Extract state from request
            const char* state_tag = strstr(request, "<BinaryState>");
            int new_state = 0;
            if (state_tag) {
                new_state = atoi(state_tag + 13);  // Skip "<BinaryState>"
                // WeMo uses 0/1 but also accepts other values
                new_state = (new_state != 0) ? 1 : 0;
            }

            ESP_LOGI(ALEXA_TAG, "SetBinaryState: relay %d -> %s",
                     relay_id, new_state ? "ON" : "OFF");

            relay_set(relay_id, new_state);
            body_len = snprintf(body, sizeof(body), SOAP_SET_STATE_RESPONSE, new_state);
        }
        // GetBinaryState
        else if (strstr(request, "GetBinaryState")) {
            int state = relay_get(relay_id);
            ESP_LOGI(ALEXA_TAG, "GetBinaryState: relay %d = %d", relay_id, state);
            body_len = snprintf(body, sizeof(body), SOAP_GET_STATE_RESPONSE, state);
        }
    }
    // GET /eventservice.xml - Service description (minimal)
    else if (strstr(request, "GET /eventservice.xml")) {
        body_len = snprintf(body, sizeof(body),
            "<?xml version=\"1.0\"?>"
            "<scpd xmlns=\"urn:Belkin:service-1-0\">"
            "<specVersion><major>1</major><minor>0</minor></specVersion>"
            "<actionList></actionList><serviceStateTable></serviceStateTable>"
            "</scpd>");
    }
    else {
        // Unknown request
        ESP_LOGW(ALEXA_TAG, "Unknown WeMo request");
        const char* not_found = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(client_sock, not_found, strlen(not_found), 0);
        return;
    }

    // Build HTTP response
    int resp_len = snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n%s",
        content_type, body_len, body);

    send(client_sock, response, resp_len, 0);
}

/**
 * @brief WeMo HTTP server task for a single relay
 */
static void wemo_device_task(void* pvParameters) {
    wemo_device_t* device = (wemo_device_t*)pvParameters;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char recv_buf[512];

    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

    ESP_LOGI(ALEXA_TAG, "Task starting for relay %d, device ptr: %p", device->relay_id, device);

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        ESP_LOGE(ALEXA_TAG, "Failed to create socket for relay %d", device->relay_id);
        vTaskDelete(NULL);
        return;
    }
    // Use memcpy to avoid unaligned access panic if device ptr is unchecked
    memcpy(&device->listen_sock, &sock, sizeof(int));

    int opt = 1;
    setsockopt(device->listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(device->port);

    if (bind(device->listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(ALEXA_TAG, "Failed to bind WeMo port %d", device->port);
        close(device->listen_sock);
        vTaskDelete(NULL);
        return;
    }

    listen(device->listen_sock, 2);
    ESP_LOGI(ALEXA_TAG, "WeMo device '%s' on port %d",
             relay_config_get_name(device->relay_id), device->port);

    while (1) {
        int client_sock = accept(device->listen_sock,
                                 (struct sockaddr*)&client_addr, &client_addr_len);

        if (client_sock < 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        struct timeval timeout = {.tv_sec = 2, .tv_usec = 0};
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        memset(recv_buf, 0, sizeof(recv_buf));
        int len = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);

        if (len > 0) {
            alexa_handle_wemo_request(client_sock, device->relay_id, recv_buf);
        }

        close(client_sock);
    }
}

/**
 * @brief SSDP server task - responds to Alexa discovery requests
 */
void alexa_ssdp_task(void* pvParameters) {
    struct sockaddr_in addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char recv_buf[256];
    char send_buf[512];
    int sock;

    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(1000));  // Wait for network to settle

    alexa_get_local_ip();
    ESP_LOGI(ALEXA_TAG, "Starting SSDP server, device IP: %s", device_ip);

    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        ESP_LOGE(ALEXA_TAG, "Failed to create SSDP socket");
        vTaskDelete(NULL);
        return;
    }

    // Allow multiple sockets to use the same port
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // Bind to SSDP port
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SSDP_PORT);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(ALEXA_TAG, "Failed to bind SSDP socket");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    // Join multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(SSDP_MULTICAST_ADDR);
    mreq.imr_interface.s_addr = INADDR_ANY;

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        ESP_LOGW(ALEXA_TAG, "Failed to join multicast group");
    }

    ESP_LOGI(ALEXA_TAG, "SSDP listening on %s:%d", SSDP_MULTICAST_ADDR, SSDP_PORT);

    while (1) {
        memset(recv_buf, 0, sizeof(recv_buf));
        int len = recvfrom(sock, recv_buf, sizeof(recv_buf) - 1, 0,
                          (struct sockaddr*)&client_addr, &client_len);

        if (len <= 0) {
            continue;
        }

        // Check if this is an M-SEARCH request for our device type
        if (strstr(recv_buf, "M-SEARCH") &&
            (strstr(recv_buf, "urn:Belkin:device:**") ||
             strstr(recv_buf, "upnp:rootdevice") ||
             strstr(recv_buf, "ssdp:all"))) {

            ESP_LOGI(ALEXA_TAG, "Discovery request from %s",
                     inet_ntoa(client_addr.sin_addr));

            // Respond for each Alexa-enabled relay
            for (int i = 0; i < NUM_RELAYS; i++) {
                if (!relay_config_alexa_enabled(i)) {
                    continue;
                }

                vTaskDelay(pdMS_TO_TICKS(50 + (i * 100)));  // Stagger responses

                char uuid[48];
                alexa_get_uuid(i, uuid, sizeof(uuid));

                int resp_len = snprintf(send_buf, sizeof(send_buf),
                    SSDP_RESPONSE, device_ip, wemo_devices[i].port, uuid, uuid);

                sendto(sock, send_buf, resp_len, 0,
                       (struct sockaddr*)&client_addr, client_len);

                ESP_LOGI(ALEXA_TAG, "Sent discovery response for '%s'",
                         relay_config_get_name(i));
            }
        }
    }
}

/**
 * @brief Initialize Alexa WeMo emulation
 * Call this after WiFi is connected and relay config is loaded
 */
void alexa_init(void) {
    ESP_LOGI(ALEXA_TAG, "Initializing Alexa WeMo emulation");

    // Initialize device structures
    for (int i = 0; i < NUM_RELAYS; i++) {
        wemo_devices[i].relay_id = i;
        wemo_devices[i].port = WEMO_BASE_PORT + i;
        wemo_devices[i].listen_sock = -1;
        alexa_get_uuid(i, wemo_devices[i].uuid, sizeof(wemo_devices[i].uuid));
    }

    // Start SSDP server task
    xTaskCreate(alexa_ssdp_task, "ssdp_task", 3072, NULL, 4, NULL);

    // Start WeMo HTTP server for each Alexa-enabled relay
    for (int i = 0; i < NUM_RELAYS; i++) {
        char task_name[16];
        snprintf(task_name, sizeof(task_name), "wemo_%d", i);
        // Increase stack to 4096 to prevent overflow (likely cause of panic)
        xTaskCreate(wemo_device_task, task_name, 4096, &wemo_devices[i], 4, NULL);
    }

    ESP_LOGI(ALEXA_TAG, "Alexa support initialized - say 'Alexa, discover devices'");
}

#endif // ALEXA_H
