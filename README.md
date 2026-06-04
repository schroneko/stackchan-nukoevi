# Nukoevi StackChan

![Nukoevi on StackChan](firmware/main/apps/app_nukoevi/source-assets/nukoevi-talk-frame-0.png)

Nukoevi StackChan is a personal custom firmware and companion workspace for
M5Stack StackChan. It keeps the StackChan hardware and much of the upstream
application structure, then adds the Nukoevi home screen, character animation,
microphone flow, MQTT text output, and websocket-based Irodori TTS playback used
by the local Claude Code Channels setup.

This repository is not an official M5Stack repository and is not a GitHub fork
of `m5stack/StackChan`. It is an independently published repository that
preserves upstream attribution through SPDX headers, dependency metadata, and
patch files.

## What is included

- Firmware for M5Stack StackChan with the Nukoevi app as the startup screen.
- StackChan UI integration for Wi-Fi, battery, camera, microphone, and controls.
- MQTT output handling for text responses and device status.
- Irodori TTS output path for StackChan speech playback.
- Patch-based integration with `78/xiaozhi-esp32` so upstream source can be
  fetched and customized reproducibly.
- The original app, remote, and server directories from the StackChan open
  source workspace, kept for reference and compatibility.

## Upstream projects

This project builds on work from these upstream projects:

- M5Stack StackChan: https://github.com/m5stack/StackChan
- StackChan BSP: https://github.com/m5stack/StackChan-BSP
- Xiaozhi ESP32: https://github.com/78/xiaozhi-esp32

The firmware fetches external source repositories from `firmware/repos.json`.
Custom changes to Xiaozhi are stored in
`firmware/patches/xiaozhi-esp32.patch`.

## Build

See `firmware/README.md` for the firmware build and flash flow.

Short version:

```bash
uv run python firmware/fetch_repos.py
tools/idf.sh build
tools/idf.sh -p /dev/tty.usbmodemXXXX flash
```

Use the serial port shown by your local machine. The checked-in paths and docs
do not assume a fixed device port.

## Current status

See `docs/nukoevi-status.md` for the current public status summary.
See `docs/irodori-tts-runtime-notes.md` for the Irodori TTS runtime notes and
debug checklist.
See `docs/voice-input-runtime-notes.md` for the microphone input path,
reconnect behavior, and `マイク起動中` debug checklist.

At a high level, the current firmware boots into Nukoevi, shows StackChan
status indicators, accepts text output over MQTT, displays responses, and can
play Irodori TTS audio through StackChan over websocket. MQTT audio is kept only
as a debug path because it produces severe audible gaps and is not usable for
speech playback. The Wi-Fi timeout behavior has been changed so saved Wi-Fi
credentials retry instead of dropping into Wi-Fi configuration mode.

## Repository notes

- This repository is intended for personal experimentation and public reference.
- It is not a replacement for official M5Stack documentation or firmware.
- Hardware safety warnings from the official StackChan documentation still
  apply. Do not force movable motor parts by hand while motors may be powered.
- Generated Nukoevi image assets are kept under
  `firmware/main/apps/app_nukoevi/source-assets/`.

## License

See `LICENSE` and `NOTICE.md`.

Source files keep their own SPDX headers. Third-party dependencies and vendored
drivers keep their own licenses.
