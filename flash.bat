@echo off

python "C:\esp\ESP8266_RTOS_SDK\components\esptool_py\esptool\esptool.py" ^
  -p COM3 ^
  -b 460800 ^
  --after hard_reset write_flash ^
  --flash_mode dio ^
  --flash_size 2MB ^
  --flash_freq 40m ^
  0x0 build\bootloader\bootloader.bin ^
  0x8000 build\partition_table\partition-table.bin ^
  0x10000 build\relay_controller.bin