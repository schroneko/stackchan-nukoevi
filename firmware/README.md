# Nukoevi StackChan firmware

This directory contains the ESP-IDF firmware build for Nukoevi StackChan.

The build uses local project sources in `firmware/main` and fetched upstream
sources listed in `firmware/repos.json`. The Xiaozhi upstream checkout is
patched with `firmware/patches/xiaozhi-esp32.patch` during dependency setup.

## Fetch dependencies

```bash
uv run python ./fetch_repos.py
```

If `uv` is not available, install it first or run the script with the Python
tooling used in your local development environment.

## Toolchain

The firmware has been built with ESP-IDF v5.5.4.

```bash
. /path/to/esp-idf/export.sh
```

## Build

```bash
idf.py build
```

## Flash

Connect StackChan over USB-C and use the serial port shown by your machine.

```bash
idf.py -p /dev/tty.usbmodemXXXX flash
```

## Monitor

```bash
idf.py -p /dev/tty.usbmodemXXXX monitor
```

Useful startup log lines:

- `[NUKOEVI] on open`
- `WifiBoard: Connected to WiFi`
- `NUKOEVI MQTT output receiver connected`

## Current firmware behavior

- Boots into the Nukoevi app.
- Shows StackChan Wi-Fi and battery status.
- Keeps camera, microphone, Wi-Fi setup, home, and controls UI in the Nukoevi
  screen.
- Receives text and Opus audio frames over MQTT for response display and TTS
  playback.
- Uses the standard StackChan/Xiaozhi microphone path as the basis for voice
  input.
- Retries saved Wi-Fi credentials when connection startup times out instead of
  entering Wi-Fi configuration mode immediately.

## Upstream patch

The upstream Xiaozhi source is not edited directly in this repository history.
Customizations are recorded in `patches/xiaozhi-esp32.patch`.

When changing files under `xiaozhi-esp32`, mirror the intended change into the
patch file and verify it applies cleanly to a clean Xiaozhi checkout.
