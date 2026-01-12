## Relay Controller (FreeRTOS, ESP8266)

A tiny, no-nonsense relay controller written in **pure C** for **FreeRTOS-based ESP8266** systems.  
Built for deterministic GPIO control without PlatformIO, Arduino layers, or C++ overhead.

### Why this exists
You want predictable relay control on ESP8266, you’re already using FreeRTOS, and you don’t want framework magic getting in the way.

### What it does
- Controls multiple relays via GPIO
- Clean, minimal C API
- Easy to drop into existing FreeRTOS projects

### Requirements
- ESP8266_RTOS_SDK
- xtensa-lx106-elf toolchain in your `PATH`
- Basic FreeRTOS familiarity

### Typical use cases
- Home automation
- Hardware bring-up & test rigs

### License
MIT - do whatever you want, just don’t be evil.