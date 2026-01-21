/**
 * @file debugout.h
 * @brief Debug output helper for ESP-IDF
 *
 * Ported from RFCodes library (https://github.com/mathertel/RFCodes)
 * Original work Copyright (c) by Matthias Hertel, https://www.mathertel.de
 *
 * This work is licensed under a BSD 3-Clause style license,
 * https://www.mathertel.de/License.aspx
 */

#ifndef DEBUGOUT_H_
#define DEBUGOUT_H_

#include "esp_log.h"

// Uncomment to disable debug output
// #define NODEBUG

#if defined(NODEBUG)
#  define TRACE_MSG(...)
#  define ERROR_MSG(...)
#  define INFO_MSG(...)
#  define RAW_MSG(...)
#else
#  define ERROR_MSG(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#  define INFO_MSG(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#  define TRACE_MSG(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)
#  define RAW_MSG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

// Disable trace messages by default (too verbose)
#undef TRACE_MSG
#define TRACE_MSG(...)

#endif // DEBUGOUT_H_
