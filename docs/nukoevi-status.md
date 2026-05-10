# Nukoevi display status

Date: 2026-05-10

## Goal

Show `/Users/username/Downloads/nukoevi.png` on the StackChan screen as a Nukoevi character app. The custom AI agent integration is intentionally out of scope for this step.

## Implemented

- Added `firmware/main/apps/app_nukoevi/`.
- Embedded a 64x64 launcher icon from `nukoevi.png`.
- Embedded a 320x240 screen image from `nukoevi.png`.
- Registered `AppNukoevi` from `firmware/main/apps/apps.h`.
- Installed `AppNukoevi` from `firmware/main/main.cpp`.
- Opened `AppNukoevi` at boot with `GetMooncake().openApp(nukoevi_app_id)`, so the Nukoevi image is shown immediately after flashing and booting.

## Verified

- ESP-IDF: v5.5.4 under `/Users/username/ghq/github.com/espressif/esp-idf`.
- Build command: `idf.py build`.
- Build result: success.
- Flash port: `/dev/cu.usbmodem2101`.
- Flash command: `idf.py -p /dev/cu.usbmodem2101 flash`.
- Flash result: success.
- Monitor result: boot completed and logged `[NUKOEVI] on open`.

## Not Included Yet

- Custom AI agent connection.
- Speech, audio, or conversation handling.
- Sprite animation from `spritesheet.webp`.

## Repository Handling

- Work is in the private ghq repository at `/Users/username/ghq/github.com/schroneko/stackchan-nukoevi`.
- No issue, PR, commit, or push was made to the official `m5stack/StackChan` repository.
