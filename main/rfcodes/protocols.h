/**
 * @file protocols.h
 * @brief Common 433 MHz RF protocol definitions
 * 
 * Ported from RFCodes by Matthias Hertel (BSD 3-Clause)
 */

#ifndef SIGNAL_PARSER_PROTOCOLS_H_
#  define SIGNAL_PARSER_PROTOCOLS_H_

#  include "signal_parser.h"

// ===== Protocol Definitions =====

/**
 * Definition of the "older" intertechno protocol with fixed 12 bits of data
 */
static signal_protocol_t protocol_it1 = {.name = "it1",
                                         .min_code_len = 1 + 12,
                                         .max_code_len = 1 + 12,
                                         .tolerance = 25,
                                         .send_repeat = 4,
                                         .base_time = 400,
                                         .codes = {
                                             {.type = CODE_TYPE_START, .name = 'B', .time = {1, 31, 0}},
                                             {.type = CODE_TYPE_DATA, .name = '0', .time = {1, 3, 3, 1, 0}},
                                             {.type = CODE_TYPE_DATA, .name = '1', .time = {1, 3, 1, 3, 0}},
                                             {.name = 0} // Terminator
                                         }};

/**
 * Definition of the "newer" intertechno protocol with 32 - 46 data bits
 */
static signal_protocol_t protocol_it2 = {.name = "it2",
                                         .min_code_len = 34,
                                         .max_code_len = 48,
                                         .tolerance = 25,
                                         .send_repeat = 10,
                                         .base_time = 280, // base time in µsecs
                                         .codes = {
                                             {.type = CODE_TYPE_START, .name = 's', .time = {1, 10, 0}},
                                             {.type = CODE_TYPE_DATA, .name = '_', .time = {1, 1, 1, 5, 0}},
                                             {.type = CODE_TYPE_DATA, .name = '#', .time = {1, 5, 1, 1, 0}},
                                             {.type = CODE_TYPE_DATA, .name = 'D', .time = {1, 1, 1, 1, 0}},
                                             {.type = CODE_TYPE_END, .name = 'x', .time = {1, 38, 0}},
                                             {.name = 0} // Terminator
                                         }};

/**
 * Definition of the protocol from SC5272 and similar chips with 12 data bits
 */
static signal_protocol_t protocol_sc5 = {.name = "sc5",
                                         .min_code_len = 1 + 12,
                                         .max_code_len = 1 + 12,
                                         .tolerance = 25,
                                         .send_repeat = 3,
                                         .base_time = 100,
                                         .codes = {
                                             {.type = CODE_TYPE_ANYDATA, .name = '0', .time = {4, 12, 4, 12, 0}},
                                             {.type = CODE_TYPE_ANYDATA, .name = '1', .time = {12, 4, 12, 4, 0}},
                                             {.type = CODE_TYPE_ANYDATA, .name = 'f', .time = {4, 12, 12, 4, 0}},
                                             {.type = CODE_TYPE_END, .name = 'S', .time = {4, 124, 0}},
                                             {.name = 0} // Terminator
                                         }};

/**
 * Definition of the protocol from EV1527 and similar chips with 20 address and 4 data bits
 */
static signal_protocol_t protocol_ev1527 = {.name = "ev1527",
                                            .min_code_len = 1 + 20 + 4,
                                            .max_code_len = 1 + 20 + 4,
                                            .tolerance = 25,
                                            .send_repeat = 3,
                                            .base_time = 320,
                                            .codes = {
                                                {.type = CODE_TYPE_START, .name = 's', .time = {1, 31, 0}},
                                                {.type = CODE_TYPE_DATA, .name = '0', .time = {1, 3, 0}},
                                                {.type = CODE_TYPE_DATA, .name = '1', .time = {3, 1, 0}},
                                                {.name = 0} // Terminator
                                            }};

/**
 * Register the cresta protocol with a length of 59 codes
 * Used for sensor data transmissions
 * See /docs/cresta_protocol.md
 */
__attribute__((unused))
static signal_protocol_t protocol_cw = {.name = "cw",
                                        .min_code_len = 59,
                                        .max_code_len = 59,
                                        .tolerance = 16,
                                        .send_repeat = 3,
                                        .base_time = 500,
                                        .codes = {
                                            {.type = CODE_TYPE_START, .name = 'H', .time = {2, 2, 2, 2, 2, 0}},
                                            {.type = CODE_TYPE_DATA, .name = 's', .time = {1, 1, 0}},
                                            {.type = CODE_TYPE_DATA, .name = 'l', .time = {2, 0}},
                                            {.name = 0} // Terminator
                                        }};

#endif // SIGNAL_PARSER_PROTOCOLS_H_

// End.
