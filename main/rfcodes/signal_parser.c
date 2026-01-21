/**
 * @file signal_parser.c
 * @brief Signal parser implementation
 * 
 * Ported from RFCodes by Matthias Hertel (BSD 3-Clause)
 */

#include "signal_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "SignalParser"

// ===== Private function declarations =====

static signal_protocol_t* find_protocol(signal_parser_t* parser, char* name);
static signal_code_t* find_code(signal_protocol_t* protocol, char code_name);
static void reset_codes(signal_protocol_t* protocol);
static void reset_protocol(signal_parser_t* parser, signal_protocol_t* protocol);
static void use_callback(signal_parser_t* parser, signal_protocol_t* protocol);
static void parse_protocol(signal_parser_t* parser, signal_protocol_t* protocol, code_time_t duration);
static void recalc_protocol(signal_protocol_t* protocol, code_time_t base_time);

// ===== Private functions =====

/** Find protocol by name */
static signal_protocol_t* find_protocol(signal_parser_t* parser, char* name) {
  signal_protocol_t* p = NULL;

  for (int n = 0; n < parser->protocol_count; n++) {
    p = parser->protocols[n];
    if (strcmp(name, p->name) == 0) {
      break;
    }
  }
  return p;
}

/** Find code by name */
static signal_code_t* find_code(signal_protocol_t* protocol, char code_name) {
  signal_code_t* c = protocol->codes;
  int cnt = protocol->code_length;

  while (c && cnt) {
    if (c->name == code_name) {
      break;
    }
    c++;
    cnt--;
  }
  return (cnt ? c : NULL);
}

/** Reset all codes in a protocol */
static void reset_codes(signal_protocol_t* protocol) {
  signal_code_t* c = protocol->codes;
  int c_cnt = protocol->code_length;

  while (c && c_cnt) {
    c->valid = true;
    c->cnt = 0;
    c->total = 0;
    c++;
    c_cnt--;
  }
}

/** Reset the whole protocol to start capturing from scratch */
static void reset_protocol(signal_parser_t* parser, signal_protocol_t* protocol) {
  TRACE_MSG(TAG, "  reset prot: %s", protocol->name);
  protocol->seq_len = 0;
  protocol->seq[0] = NUL;
  reset_codes(protocol);
  recalc_protocol(protocol, protocol->base_time);
}

/** Use the callback function when registered using format <protocolname> <sequence> */
static void use_callback(signal_parser_t* parser, signal_protocol_t* protocol) {
  if (protocol && parser->callback_func) {
    char code[PROTNAME_LEN + MAX_SEQUENCE_LENGTH + 2];
    snprintf(code, sizeof(code), "%s %s", protocol->name, protocol->seq);
    parser->callback_func(code);
  }
}

/** Check if the duration fits for the protocol */
static void parse_protocol(signal_parser_t* parser, signal_protocol_t* protocol, code_time_t duration) {
  signal_code_t* c = protocol->codes;
  int c_cnt = protocol->code_length;
  bool any_valid = false;
  bool retry_candidate = false;

  while (c && c_cnt) {
    if (c->valid) {
      // Check if timing fits into this code
      int8_t i = c->cnt;
      code_type_t type = c->type;
      bool matched = false; // until found that the new duration fits

      TRACE_MSG(TAG, "check: %c", c->name);

      if ((protocol->seq_len == 0) && !(type & CODE_TYPE_START)) {
        // Codes other than start codes are not acceptable as a first code in the sequence
        // TRACE_MSG(TAG, "  not start");

      } else if ((protocol->seq_len > 0) && !(type & CODE_TYPE_ANY)) {
        // Codes other than data and end codes are not acceptable during receiving
        // TRACE_MSG(TAG, "  not data");

      } else if ((duration < c->min_time[i]) || (duration > c->max_time[i])) {
        // This timing is not matching
        // TRACE_MSG(TAG, "  no fitting timing");

        if ((i == 1) && (protocol->seq_len == 0)) {
          // Reanalyze this duration as a first duration for starting
          retry_candidate = true;
          // TRACE_MSG(TAG, "  --retry");
        }

      } else {
        matched = true; // this code matches
        c->total += duration;
      }

      // Write back to code
      c->valid = matched;
      any_valid = any_valid || matched;

      if (retry_candidate) {
        // Reset this code only and try again
        TRACE_MSG(TAG, "  start retry...");
        reset_protocol(parser, protocol);

      } else if (matched) {
        // This timing is matching
        TRACE_MSG(TAG, "  matched.");
        c->cnt = i = i + 1;

        if (i == c->time_length) {
          // All timings received so add code-character
          if (protocol->seq_len == 0) {
            TRACE_MSG(TAG, "start: %s %d", protocol->name, c->total);
            int all_times = 0;
            for (int tl = 0; tl < c->time_length; tl++) {
              all_times += c->time[tl];
            }
            recalc_protocol(protocol, c->total / all_times);
          }

          protocol->seq[protocol->seq_len++] = c->name;
          protocol->seq[protocol->seq_len] = NUL;
          TRACE_MSG(TAG, "  add '%s'", protocol->seq);

          reset_codes(protocol); // reset all codes but not the protocol

          if ((type == CODE_TYPE_END) && (protocol->seq_len < protocol->min_code_len)) {
            // End packet found but sequence was not started early enough
            TRACE_MSG(TAG, "  end fragment: %s", protocol->seq);
            reset_protocol(parser, protocol);

          } else if ((type & CODE_TYPE_END) && (protocol->seq_len >= protocol->min_code_len)) {
            TRACE_MSG(TAG, "  found-1: %s", protocol->seq);
            use_callback(parser, protocol);
            reset_protocol(parser, protocol);

          } else if ((protocol->seq_len == protocol->max_code_len)) {
            TRACE_MSG(TAG, "  found-2: %s", protocol->seq);
            use_callback(parser, protocol);
            reset_protocol(parser, protocol);
          }
          break; // no more code checking in this protocol
        }
      }
    }

    if (retry_candidate) {
      // Only loop once
      retry_candidate = false;
    } else {
      // Next code
      c++;
      c_cnt--;
    }
  }

  if (!any_valid) {
    TRACE_MSG(TAG, "  no codes.");
    reset_protocol(parser, protocol);
  }
}

/** Recalculate adjusted timings */
static void recalc_protocol(signal_protocol_t* protocol, code_time_t base_time) {
  // Calc min and max and codes_length
  if (protocol->base_time != base_time) {
    TRACE_MSG(TAG, "recalc %d", base_time);
  }

  for (int cl = 0; cl < protocol->code_length; cl++) {
    signal_code_t* c = &(protocol->codes[cl]);

    for (int tl = 0; tl < c->time_length; tl++) {
      code_time_t t = base_time * c->time[tl];
      int radius = (t * protocol->tolerance) / 100;
      c->min_time[tl] = t - radius;
      c->max_time[tl] = t + radius;
    }
  }
}

// ===== Public functions =====

void signal_parser_init(signal_parser_t* parser) {
  parser->protocols = NULL;
  parser->protocol_alloc = 0;
  parser->protocol_count = 0;
  parser->callback_func = NULL;
}

void signal_parser_attach_callback(signal_parser_t* parser, signal_callback_t callback) {
  parser->callback_func = callback;
}

int signal_parser_get_send_repeat(signal_parser_t* parser, char* name) {
  signal_protocol_t* p = find_protocol(parser, name);
  return (p ? p->send_repeat : 0);
}

void signal_parser_parse(signal_parser_t* parser, code_time_t duration) {
  TRACE_MSG(TAG, "(%d)", duration);

  for (int n = 0; n < parser->protocol_count; n++) {
    parse_protocol(parser, parser->protocols[n], duration);
  }
}

void signal_parser_compose(signal_parser_t* parser, const char* sequence, code_time_t* timings, int len) {
  char protname[PROTNAME_LEN];

  char* s = strchr(sequence, ' ');

  if (s) {
    // Extract protname
    {
      char* tar = protname;
      const char* src = sequence;
      while (*src && (*src != ' ')) {
        *tar++ = *src++;
      }
      *tar = NUL;
    }
    signal_protocol_t* p = find_protocol(parser, protname);

    s++; // to start of code characters

    if (p && timings) {
      while (*s && len) {
        signal_code_t* c = find_code(p, *s);
        if (c) {
          for (int i = 0; i < c->time_length; i++) {
            *timings++ = (c->min_time[i] + c->max_time[i]) / 2;
          }
        }
        s++;
        len--;
      }
      *timings = 0;
    }
  }
}

void signal_parser_load(signal_parser_t* parser, signal_protocol_t* protocol, code_time_t other_base_time) {
  if (protocol) {
    TRACE_MSG(TAG, "loading protocol %s", protocol->name);

    // Get space for protocol definition
    if (parser->protocol_count >= parser->protocol_alloc) {
      parser->protocol_alloc += 8;
      TRACE_MSG(TAG, "alloc %d", parser->protocol_alloc);
      parser->protocols = (signal_protocol_t**)realloc(parser->protocols, parser->protocol_alloc * sizeof(signal_protocol_t*));
    }

    // Fill last one
    parser->protocols[parser->protocol_count] = protocol;
    TRACE_MSG(TAG, "_p[%d]=%p", parser->protocol_count, (void*)protocol);
    parser->protocol_count += 1;

    code_time_t base_time = protocol->base_time;

    // Calc c->time_length and p->code_length
    int cl = 0;
    while ((cl < MAX_CODELENGTH) && (protocol->codes[cl].name)) {
      signal_code_t* c = &(protocol->codes[cl]);

      int tl = 0;
      while ((tl < MAX_TIMELENGTH) && (c->time[tl])) {
        tl++;
      }
      c->time_length = tl;
      cl++;
    }
    protocol->code_length = cl; // no need to specify code_length

    recalc_protocol(protocol, base_time);
    reset_protocol(parser, protocol);

    for (int n = 0; n < parser->protocol_count; n++) {
      TRACE_MSG(TAG, " reg[%d] = %p", n, (void*)parser->protocols[n]);
    }
  }
}

void signal_parser_dump_protocol(signal_protocol_t* protocol) {
  TRACE_MSG(TAG, "dump %p", (void*)protocol);

  if (protocol) {
    // Dump the Protocol characteristics
    RAW_MSG("Protocol '%s', min:%d max:%d tol:%02u rep:%d\n", protocol->name, protocol->min_code_len, protocol->max_code_len,
            protocol->tolerance, protocol->send_repeat);

    signal_code_t* c = protocol->codes;
    int cnt = protocol->code_length;

    while (c && cnt) {
      RAW_MSG("  '%c' |", c->name);

      for (int n = 0; n < c->time_length; n++) {
        RAW_MSG("%5d -%5d |", c->min_time[n], c->max_time[n]);
      }
      RAW_MSG("\n");

      c++;
      cnt--;
    }
    RAW_MSG("\n");
  }
}

void signal_parser_dump_table(signal_parser_t* parser) {
  for (int n = 0; n < parser->protocol_count; n++) {
    signal_protocol_t* p = parser->protocols[n];
    signal_parser_dump_protocol(p);
  }
}

// End.
