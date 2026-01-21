# RFCodes - ESP-IDF Port

This is a C port of the RFCodes library for ESP-IDF, enabling RF433 and IR protocol encoding/decoding on ESP8266/ESP32.

## Original Library

- **Author**: Matthias Hertel
- **Source**: https://github.com/mathertel/RFCodes
- **License**: BSD 3-Clause

## Port Details

- Ported from C++/Arduino to C/ESP-IDF
- Adapted for FreeRTOS and ESP-IDF GPIO/timer APIs
- Maintains full protocol compatibility with original library

## Supported Protocols

- **EV1527**: Common 433MHz remote control chip (20-bit address + 4-bit data)
- **SC5272**: 3-state protocol chip
- **Intertechno IT1**: Older Intertechno protocol (12-bit)
- **Intertechno IT2**: Newer Intertechno protocol (32-46 bit)
- **Cresta**: Weather sensor protocol

## Usage

See `../rf.h` for a complete example of using the library for RF433 reception.

## Files

- `signal_parser.c/h` - Core protocol parsing engine
- `signal_collector.c/h` - GPIO interrupt handling and signal collection
- `protocols.h` - Protocol definitions
- `rfcodes.h` - Main header file
- `debugout.h` - ESP-IDF logging macros
- `LICENSE` - BSD 3-Clause license text
