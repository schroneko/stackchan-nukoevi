# StackChan-RemoteControl-ESPNow

This repository is designed to remotely control StackChan servo movements via the `ESPNow` protocol.

## Applicable Devices

* StickC-Plus + Hat Mini JoyC

## Compilation Environment

* IDF：ESP-IDF v5.4.2
* Device Type：esp32

## Compilation and Flashing

1. Before compilation, globally search the project for `__has_include(<driver/i2c_master.h>)` in `M5GFX` and replace all occurrences with `0`.
2. When flashing, specify the baud rate as `1500000`.

## Package Firmware

```
esptool.py --chip esp32 merge_bin -o StackChan-RemoteControl-ESPNow-jyy-20251231_0x0.bin 0x1000 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0x10000 build\StackChan-RemoteControl-ESPNow.bin
```
