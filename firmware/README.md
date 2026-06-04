# Nukoevi StackChan firmware

This directory contains the ESP-IDF firmware build for Nukoevi StackChan.

The build uses local project sources in `firmware/main` and fetched upstream
sources listed in `firmware/repos.json`. The Xiaozhi upstream checkout is
patched with `firmware/patches/xiaozhi-esp32.patch` during dependency setup.

## Fetch dependencies

From the repository root:

```bash
uv run python firmware/fetch_repos.py
```

If `uv` is not available, install it first or run the script with the Python
tooling used in your local development environment.

## Toolchain

The firmware has been built with ESP-IDF v5.5.4. Use the repository wrapper
from the repository root. It sources ESP-IDF from `IDF_PATH`,
`${GHQ_ROOT:-$HOME/ghq}/github.com/espressif/esp-idf`, or `$HOME/esp/esp-idf`.

```bash
tools/idf.sh --version
```

## Build

```bash
tools/idf.sh build
```

## Flash

Connect StackChan over USB-C and use the serial port shown by your machine.

```bash
tools/idf.sh -p /dev/tty.usbmodemXXXX flash
```

## Monitor

```bash
tools/idf.sh -p /dev/tty.usbmodemXXXX monitor
```

Useful startup log lines:

- `[NUKOEVI] on open`
- `WifiBoard: Connected to WiFi`
- `NUKOEVI MQTT output receiver connected`
- `blink assets loaded: ok`
- `sleep assets loaded: ok`

## Current firmware behavior

- Boots into the Nukoevi app.
- Shows StackChan Wi-Fi and battery status.
- Keeps camera, microphone, Wi-Fi setup, home, and controls UI in the Nukoevi
  screen.
- Receives response text and device state over MQTT.
- Plays Irodori TTS audio through the StackChan audio websocket. MQTT audio is
  kept only for debugging because it causes severe audible gaps on the physical
  StackChan.
- Uses the standard StackChan/Xiaozhi microphone path as the basis for voice
  input.
- Switches to a sleepy Nukoevi image between 22:00 and 07:00 JST.
- Retries saved Wi-Fi credentials when connection startup times out instead of
  entering Wi-Fi configuration mode immediately.

## Nukoevi image assets

Nukoevi full-screen motion frames are loaded from the firmware `assets`
partition as raw RGB565 `.bin` image descriptors. The normal blink frames and
sleepy frames both use this path.

Each raw frame is `320x240` RGB565 with a 12 byte LVGL image header followed by
`153600` bytes of pixel data. The header uses `LV_COLOR_FORMAT_RGB565`, no
compression flag, and `stride=0` so LVGL computes the same stride as the
working C image descriptor path.

The project avoids LZ4-compressed full-screen avatar assets for the Nukoevi
avatar path. LZ4 kept flash usage smaller, but this rendering path produced a
white screen with `Avatar is invalid` during testing. Keep these avatar frames
raw unless the display path is changed and verified on hardware.

The `assets` partition is `5M` in `partitions.csv`. This gives room for the
normal blink frames, sleepy frames, talk frames, icons, fonts, and other runtime
assets while keeping the app partition below its OTA size limit. Use full
`flash`, not `app-flash`, after changing runtime assets or the partition table.

Regenerate normal blink C previews and runtime `.bin` assets after changing the
normal source frames:

```bash
uv run --with pillow python main/apps/app_nukoevi/source-assets/generate_nukoevi_blink_assets.py
```

Regenerate talk and sleepy runtime `.bin` assets after changing talk or sleepy
source frames:

```bash
uv run --with pillow --with lz4 python main/apps/app_nukoevi/source-assets/generate_nukoevi_motion_assets.py
```

## Upstream patch

The upstream Xiaozhi source is not edited directly in this repository history.
Customizations are recorded in `patches/xiaozhi-esp32.patch`.

When changing files under `xiaozhi-esp32`, mirror the intended change into the
patch file and verify it applies cleanly to a clean Xiaozhi checkout.
