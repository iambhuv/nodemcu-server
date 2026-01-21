/**
 * @file rfcodes.h
 * @brief Main header for RFCodes library - RF/IR protocol encoding and decoding
 *
 * Ported from RFCodes library (https://github.com/mathertel/RFCodes)
 * Original work Copyright (c) by Matthias Hertel, https://www.mathertel.de
 *
 * This work is licensed under a BSD 3-Clause style license,
 * https://www.mathertel.de/License.aspx
 *
 * This library can encode and decode signal patterns that are used in the
 * 433 MHz and IR technology.
 *
 * Ported to C/ESP-IDF from original C++/Arduino implementation
 *
 * @example Basic usage:
 * @code
 * signal_parser_t parser;
 * signal_collector_t collector;
 *
 * // Initialize parser
 * signal_parser_init(&parser);
 * signal_parser_load(&parser, &protocol_ev1527, 0);
 * signal_parser_attach_callback(&parser, my_callback);
 *
 * // Initialize collector
 * signal_collector_init(&collector, &parser, RX_PIN, TX_PIN, 0);
 *
 * // In main loop
 * signal_collector_loop(&collector);
 *
 * // To send a code
 * signal_collector_send(&collector, "ev1527 s000000000000000000001111");
 * @endcode
 */

#ifndef RFCODES_H_
#define RFCODES_H_

#include "protocols.h"
#include "signal_collector.h"
#include "signal_parser.h"

/**
 * @brief Library version information
 */
#define RFCODES_VERSION "2.0.0-espidf"
#define RFCODES_ORIGINAL_AUTHOR "Matthias Hertel"
#define RFCODES_ORIGINAL_URL "https://github.com/mathertel/RFCodes"
#define RFCODES_LICENSE "BSD 3-Clause"

#endif // RFCODES_H_
