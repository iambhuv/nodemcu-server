/**
 * @file signal_collector.c
 * @brief Signal collector implementation
 * 
 * Ported from RFCodes by Matthias Hertel (BSD 3-Clause)
 */

#include "signal_collector.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include <stdlib.h>
#include <string.h>

#define TAG "SignalCollector"

// ===== Static variables for ring buffer (accessible to ISR) =====

static code_time_t* buf88 = NULL;               // allocated memory
static volatile code_time_t* ring_write = NULL; // write pointer
static volatile code_time_t* buf88_read = NULL; // read pointer
static code_time_t* buf88_end = NULL;           // end of buffer+1 pointer for wrapping
static volatile unsigned int buf88_cnt = 0;     // number of bytes in buffer

static uint64_t last_time = 0; // last time the interrupt was called

static int recv_pin_global = -1; // Global recv pin for ISR

// ===== ISR Handler =====

static void IRAM_ATTR signal_change_handler(void* arg) {
  uint64_t now = esp_timer_get_time();
  code_time_t t = (code_time_t)(now - last_time);

  // Write to ring buffer
  if (buf88_cnt < SC_BUFFERSIZE) {
    *ring_write++ = t;
    buf88_cnt++;

    // Reset pointer to the start when reaching end
    if (ring_write == buf88_end) {
      ring_write = buf88;
    }
  }

  last_time = now;
}

// ===== Helper functions =====

static void strcpy_protname(char* target, const char* signal) {
  char* p = target;
  const char* s = signal;
  int len = PROTNAME_LEN - 1;

  while (len && *s && (*s != ' ')) {
    *p++ = *s++;
    len--;
  }
  *p = NUL;
}

// ===== Public functions =====

void signal_collector_init(signal_collector_t* collector, signal_parser_t* parser, int recv_pin, int send_pin, int trim) {
  TRACE_MSG(TAG, "Initializing signal collector hardware");

  collector->parser = parser;
  collector->recv_pin = recv_pin;
  collector->send_pin = send_pin;
  collector->trim = trim;

  // Allocate ring buffer if not already allocated
  if (buf88 == NULL) {
    buf88 = (code_time_t*)malloc(SC_BUFFERSIZE * sizeof(code_time_t));
    ring_write = buf88;
    buf88_read = buf88;
    buf88_end = buf88 + SC_BUFFERSIZE;
    buf88_cnt = 0;
  }

  // Receiving mode
  if (recv_pin >= 0) {
    recv_pin_global = recv_pin;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << recv_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&io_conf);

    // Install ISR service if not already installed
    gpio_install_isr_service(0);
    gpio_isr_handler_add(recv_pin, signal_change_handler, NULL);

    INFO_MSG(TAG, "Receiver initialized on GPIO %d", recv_pin);
  }

  // Sending mode
  if (send_pin >= 0) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << send_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(send_pin, 0);

    INFO_MSG(TAG, "Transmitter initialized on GPIO %d", send_pin);
  }
}

void signal_collector_send(signal_collector_t* collector, const char* signal) {
  code_time_t timings[256];
  int level = 0; // LOW level before starting

  char protname[PROTNAME_LEN];
  strcpy_protname(protname, signal);

  int repeat = signal_parser_get_send_repeat(collector->parser, protname);

  if ((repeat) && (collector->send_pin >= 0)) {
    // Get timings of the code
    signal_parser_compose(collector->parser, signal, timings, sizeof(timings) / sizeof(code_time_t));

    while (repeat) {
      code_time_t* t = timings;

      if (collector->recv_pin >= 0) {
        // Disable interrupts during transmission
        portDISABLE_INTERRUPTS();
      }

      while (*t) {
        level = !level;
        gpio_set_level(collector->send_pin, level);
        ets_delay_us(*t++);
      }

      if (collector->recv_pin >= 0) {
        portENABLE_INTERRUPTS();
      }

      repeat--;
    }

    // Never leave active after sending
    gpio_set_level(collector->send_pin, 0);
  }
}

void signal_collector_loop(signal_collector_t* collector) {
  while (buf88_cnt > 0) {
    code_time_t t = *buf88_read++;
    buf88_cnt--;

    signal_parser_parse(collector->parser, t);

    // Reset pointer to the start when reaching end
    if (buf88_read == buf88_end) {
      buf88_read = buf88;
    }

    vTaskDelay(0); // Yield to other tasks
  }
}

uint32_t signal_collector_get_buffer_count(signal_collector_t* collector) {
  return buf88_cnt;
}

void signal_collector_get_buffer_data(signal_collector_t* collector, code_time_t* buffer, int len) {
  len--; // keep space for final '0'
  if (len > SC_BUFFERSIZE) {
    len = SC_BUFFERSIZE;
  }

  code_time_t* p = (code_time_t*)buf88_read - len;
  if (p < buf88) {
    p += SC_BUFFERSIZE;
  }

  // Copy timings to buffer
  while (len) {
    *buffer++ = *p++;

    // Reset pointer to the start when reaching end
    if (p == buf88_end) {
      p = buf88;
    }
    len--;
  }
  *buffer = 0;
}

void signal_collector_dump_timings(code_time_t* raw) {
  // Dump probes
  code_time_t* p = raw;
  int len = 0;

  while (p && *p) {
    if (len % 8 == 0) {
      RAW_MSG("%3d: %5u,", len, *p);
    } else if (len % 8 == 7) {
      RAW_MSG(" %5u,\n", *p);
    } else {
      RAW_MSG(" %5u,", *p);
    }
    p++;
    len++;
  }
  RAW_MSG("\n");
}

void signal_collector_inject_timing(signal_collector_t* collector, code_time_t t) {
  // Write to ring buffer
  if (buf88_cnt < SC_BUFFERSIZE) {
    *ring_write++ = t;
    buf88_cnt++;

    // Reset pointer to the start when reaching end
    if (ring_write == buf88_end) {
      ring_write = buf88;
    }
  }

  last_time = esp_timer_get_time();
}

// End.
