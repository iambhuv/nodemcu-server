/**
 * @file signal_collector.h
 * @brief Signal collector for RF/IR reception and transmission
 * 
 * Ported from RFCodes by Matthias Hertel (BSD 3-Clause)
 */

#ifndef SIGNAL_COLLECTOR_H_
#define SIGNAL_COLLECTOR_H_

#include "driver/gpio.h"
#include "signal_parser.h"

#define NO_PIN (-1)

#define SC_BUFFERSIZE 512

// SignalCollector structure (replaces C++ class)
typedef struct {
  signal_parser_t* parser;
  int recv_pin; // IO Pin number for receiving signals
  int send_pin; // IO Pin number for sending signals
  int trim;     // timing factor
} signal_collector_t;

// ===== Public Functions =====

/**
 * @brief Initialize receiving and sending pins and register interrupt service routine
 * @param collector Pointer to collector structure
 * @param parser Pointer to signal parser
 * @param recv_pin GPIO pin for receiving (-1 to disable)
 * @param send_pin GPIO pin for sending (-1 to disable)
 * @param trim Timing adjustment factor
 */
void signal_collector_init(signal_collector_t* collector, signal_parser_t* parser, int recv_pin, int send_pin, int trim);

/**
 * @brief Send out a new code
 * @param collector Pointer to collector structure
 * @param code Code string in format "<protocol> <sequence>"
 */
void signal_collector_send(signal_collector_t* collector, const char* code);

/**
 * @brief Process buffered data - must be called regularly from main loop
 * @param collector Pointer to collector structure
 */
void signal_collector_loop(signal_collector_t* collector);

/**
 * @brief Return the number of buffered data in the ring buffer
 * @param collector Pointer to collector structure
 * @return Number of buffered timings
 */
uint32_t signal_collector_get_buffer_count(signal_collector_t* collector);

/**
 * @brief Return the last received timings from the ring-buffer
 * @param collector Pointer to collector structure
 * @param buffer Target timing buffer
 * @param len Length of buffer
 */
void signal_collector_get_buffer_data(signal_collector_t* collector, code_time_t* buffer, int len);

/**
 * @brief Dump the data from a table of timings that end with a 0 time
 * @param raw Pointer to raw timings data
 */
void signal_collector_dump_timings(code_time_t* raw);

/**
 * @brief Inject a test timing into the ring buffer
 * @param collector Pointer to collector structure
 * @param t Timing value to inject
 */
void signal_collector_inject_timing(signal_collector_t* collector, code_time_t t);

#endif // SIGNAL_COLLECTOR_H_
