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

## Current firmware behavior

- Boots into the Nukoevi app.
- Shows StackChan Wi-Fi and battery status.
- Keeps camera, microphone, Wi-Fi setup, home, and controls UI in the Nukoevi
  screen.
- Receives text and Opus audio frames over MQTT for response display and TTS
  playback.
- Uses the standard StackChan/Xiaozhi microphone path as the basis for voice
  input.
- Switches to a sleepy Nukoevi image between 22:00 and 07:00 JST.
- Retries saved Wi-Fi credentials when connection startup times out instead of
  entering Wi-Fi configuration mode immediately.

## Nukoevi image assets

The normal face and sleepy face are compiled as C image descriptors and passed
to LVGL with the same `setSrc()` path. This keeps the 22:00 to 07:00 sleepy
mode on the same rendering path as the normal Nukoevi screen.

Talk frames still use runtime `.bin` assets from the assets partition because
there are six mouth frames and keeping every motion frame as uncompressed C
data would use too much app partition space. The sleepy mode currently uses one
static `320x240` frame generated from
`main/apps/app_nukoevi/source-assets/nukoevi-sleep-frame-0.png`.

Regenerate the sleepy C asset after changing the source frame:

```bash
uv run --with pillow python main/apps/app_nukoevi/source-assets/generate_nukoevi_sleep_asset.py
```

## Upstream patch

The upstream Xiaozhi source is not edited directly in this repository history.
Customizations are recorded in `patches/xiaozhi-esp32.patch`.

When changing files under `xiaozhi-esp32`, mirror the intended change into the
patch file and verify it applies cleanly to a clean Xiaozhi checkout.
