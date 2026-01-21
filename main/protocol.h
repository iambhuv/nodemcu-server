#ifndef RELAY_PROTOCOL_H
#define RELAY_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Protocol: Simple binary packets
// All multi-byte values are little-endian

#define PROTO_MAGIC 0xA5 // Start byte for packet validation

#define MAX_RESP_DATA 255

// Command types
typedef enum {
  CMD_PING = 0x01,         // Ping device
  CMD_GET_STATUS = 0x02,   // Get all relay states
  CMD_SET_RELAY = 0x03,    // Set specific relay state
  CMD_TOGGLE_RELAY = 0x04, // Toggle specific relay
  CMD_SET_ALL = 0x05,      // Set all relays at once (bitmask)
  CMD_DESCRIBE = 0x10      // Get all information about device
} cmd_type_t;

// Response types
typedef enum { RESP_OK = 0x00, RESP_ERROR = 0x01, RESP_STATUS = 0x02, RESP_PONG = 0x03, RESP_DESCRIBE = 0x04 } resp_type_t;

// A5 04 1B 01 06 73 77 69 74 63 68 02 04 53 52 2D 34 03 01 A5 A5 A5 A5 A5 A5 A5
// A5 A5 A5 A5

// Device description
typedef enum {
  DESC_DEVICE_TYPE = 0x01,  // "switch"
  DESC_MODEL = 0x02,        // "SR-4"
  DESC_RELAY_COUNT = 0x03,  // u8
  DESC_CAPABILITIES = 0x04, // bitmask
  DESC_FW_VERSION = 0x05,   // "1.2.0"
} desc_type_t;

// Request packet structure
// [MAGIC:1][CMD:1][RELAY_ID:1][VALUE:1]
typedef struct __attribute__((packed)) {
  uint8_t magic;    // Must be PROTO_MAGIC
  uint8_t cmd;      // Command type
  uint8_t relay_id; // Relay index (0-7) or bitmask for SET_ALL
  uint8_t value;    // 0=off, 1=on, or bitmask
} relay_request_t;

// Response packet structure
// [MAGIC:1][RESP_TYPE:1][DATA_LEN:1][DATA:N]
typedef struct __attribute__((packed)) {
  uint8_t magic;     // PROTO_MAGIC
  uint8_t resp_type; // Response type
  uint8_t data_len;  // Length of following data
  uint8_t data[16];  // Response data (status bits, error codes, etc)
} relay_response_t;

// Parse and validate request
static inline bool proto_parse_request(const uint8_t* buf, size_t len, relay_request_t* req) {
  if (len < sizeof(relay_request_t))
    return false;

  req->magic = buf[0];
  req->cmd = buf[1];
  req->relay_id = buf[2];
  req->value = buf[3];

  return req->magic == PROTO_MAGIC;
}

// Build response
static inline size_t proto_build_response(uint8_t* buf, uint8_t resp_type, const uint8_t* data, uint8_t data_len) {

  buf[0] = PROTO_MAGIC;
  buf[1] = resp_type;
  buf[2] = data_len;

  if (data_len > 0 && data != NULL) {
    for (int i = 0; i < data_len; i++) {
      buf[3 + i] = data[i];
    }
  }

  return 3 + data_len;
}

// Quick helpers for common responses
static inline size_t proto_ok_response(uint8_t* buf) {
  return proto_build_response(buf, RESP_OK, NULL, 0);
}

static inline size_t proto_error_response(uint8_t* buf, uint8_t error_code) {
  return proto_build_response(buf, RESP_ERROR, &error_code, 1);
}

static inline size_t proto_pong_response(uint8_t* buf) {
  return proto_build_response(buf, RESP_PONG, NULL, 0);
}

static inline size_t proto_status_response(uint8_t* buf, uint8_t relay_states) {
  return proto_build_response(buf, RESP_STATUS, &relay_states, 1);
}

#endif // RELAY_PROTOCOL_H