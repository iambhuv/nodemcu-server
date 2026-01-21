/**
 * @file signal_parser.h
 * @brief Signal parser for RF/IR protocol decoding and encoding
 * 
 * Ported from RFCodes by Matthias Hertel (BSD 3-Clause)
 * https://github.com/mathertel/RFCodes
 */

#ifndef SIGNAL_PARSER_H_
#define SIGNAL_PARSER_H_

#include "debugout.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define NUL '\0'

#define MAX_TIMELENGTH 8 // maximal length of a code definition
#define MAX_CODELENGTH 8 // maximal number of code definitions per protocol

#define MAX_SEQUENCE_LENGTH 120                                  // maximal length of a code sequence
#define MAX_TIMING_LENGTH (MAX_TIMELENGTH * MAX_SEQUENCE_LENGTH) // maximal number of timings in a sequence

#define PROTNAME_LEN 12 // maximal protocol name len including ending '\0'

// ===== Type definitions =====

// Use-cases of a defined code (start, data, end)
typedef enum {
  CODE_TYPE_START = 0x01,   // A valid start code type
  CODE_TYPE_DATA = 0x02,    // A code containing some information
  CODE_TYPE_END = 0x04,     // This code ends a sequence
  CODE_TYPE_ANYDATA = 0x03, // (START | DATA) A code with data can be used to start a sequence
  CODE_TYPE_ANY = 0x06      // (DATA | END) A code with data that can end the sequence
} code_type_t;

// Timings are using code_time_t datatypes meaning µsecs
typedef unsigned int code_time_t;

// The Code structure is used to hold a specific timing sequence used in the protocol
typedef struct {
  code_type_t type; // type of usage of code
  char name;        // single character name for this code used for the message string

  code_time_t time[MAX_TIMELENGTH]; // ideal time of the code part

  // These members will be calculated:
  int time_length;   // number of timings for this code
  code_time_t total; // total time in this code

  code_time_t min_time[MAX_TIMELENGTH]; // minimum time of the code part
  code_time_t max_time[MAX_TIMELENGTH]; // maximum time of the code part

  // These fields reflect the current status of the code
  int cnt;    // number of discovered timings
  bool valid; // is true while discovering and the code is still possible
} signal_code_t;

// The Protocol structure is used to hold the basic settings for a protocol
typedef struct {
  // These members must be initialized for load():
  char name[PROTNAME_LEN];   // name of the protocol
  unsigned int min_code_len; // minimal number of codes in a row required by the protocol
  unsigned int max_code_len; // maximum number of codes in a row defining a complete sequence
  unsigned int tolerance;    // tolerance of the timings in percent
  unsigned int send_repeat;  // Number of repeats when sending
  code_time_t base_time;     // base time for protocol
  code_time_t real_base;     // calculated real base time

  signal_code_t codes[MAX_CODELENGTH];

  // ===== These members are used while parsing:
  int code_length;               // Number of defined codes in this table
  char seq[MAX_SEQUENCE_LENGTH]; // current sequence being parsed
  int seq_len;                   // length of current sequence
} signal_protocol_t;

// Callback when a code sequence was detected
typedef void (*signal_callback_t)(const char* code);

// SignalParser structure (replaces C++ class)
typedef struct {
  signal_protocol_t** protocols;
  int protocol_alloc;
  int protocol_count;
  signal_callback_t callback_func;
} signal_parser_t;

// ===== Public Functions =====

/**
 * @brief Initialize a signal parser
 * @param parser Pointer to parser structure
 */
void signal_parser_init(signal_parser_t* parser);

/**
 * @brief Attach a callback function that will get passed any new code
 * @param parser Pointer to parser structure
 * @param callback Callback function
 */
void signal_parser_attach_callback(signal_parser_t* parser, signal_callback_t callback);

/**
 * @brief Return the number of send repeats that should occur
 * @param parser Pointer to parser structure
 * @param name Protocol name
 * @return Number of repeats
 */
int signal_parser_get_send_repeat(signal_parser_t* parser, char* name);

/**
 * @brief Parse a single duration
 * @param parser Pointer to parser structure
 * @param duration Check if this duration fits to any definitions
 */
void signal_parser_parse(signal_parser_t* parser, code_time_t duration);

/**
 * @brief Compose the timings of a sequence by using the code table
 * @param parser Pointer to parser structure
 * @param sequence Textual representation using "<protocolname> <codes>"
 * @param timings Output buffer for timings
 * @param len Length of timings buffer
 */
void signal_parser_compose(signal_parser_t* parser, const char* sequence, code_time_t* timings, int len);

/**
 * @brief Load a protocol to be used
 * @param parser Pointer to parser structure
 * @param protocol Protocol to load
 * @param other_base_time Alternative base time (0 = use protocol default)
 */
void signal_parser_load(signal_parser_t* parser, signal_protocol_t* protocol, code_time_t other_base_time);

/**
 * @brief Dump protocol information for debugging
 * @param protocol Protocol to dump
 */
void signal_parser_dump_protocol(signal_protocol_t* protocol);

/**
 * @brief Dump all loaded protocols
 * @param parser Pointer to parser structure
 */
void signal_parser_dump_table(signal_parser_t* parser);

#endif // SIGNAL_PARSER_H_
