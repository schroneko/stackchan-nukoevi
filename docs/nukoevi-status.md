# Nukoevi status

This page is a public status summary for the Nukoevi StackChan firmware.

## Implemented

- Nukoevi app is registered as the startup app.
- Nukoevi character frames are generated as full-screen image assets.
- Blink, sleepy, and talk display frames are packed into the firmware assets
  partition and loaded as LVGL image descriptors at runtime.
- The Nukoevi screen keeps the standard StackChan status surface, including
  Wi-Fi, battery, camera, microphone, home, and controls UI.
- Telegram and StackChan response fanout is routed through Claude Code Channels
  and the StackChan MCP path.
- StackChan output receives text over MQTT and displays it on the bottom comment
  area.
- Irodori TTS output is sent to StackChan as Opus audio frames over MQTT.
- Irodori TTS uses the StackChan API adapter and ZeroGPU backend with
  `duration_scale=0.95`, model `bf16`, codec `fp32`, and a 30 second ZeroGPU
  task budget.
- The microphone icon starts the StackChan/Xiaozhi voice input path.
- Sleepy mode switches through sleepy Nukoevi frames from 22:00 to 07:00 JST.
- Saved Wi-Fi credentials retry on startup timeout instead of automatically
  entering Wi-Fi configuration mode.

## Asset workflow

- Source and preview images live under
  `firmware/main/apps/app_nukoevi/source-assets/`.
- Runtime `.bin` assets live under `firmware/main/assets/assets_bin/`.
- Normal blink and sleepy frames are raw RGB565 `.bin` assets in the assets
  partition. Each frame is `320x240` with a 12 byte LVGL image header and
  `153600` bytes of RGB565 pixel data.
- Talk frames remain LZ4-compressed `.bin` assets because they are not used as
  the primary full-screen avatar image path.
- The assets partition is `5M` so runtime images can be stored outside the app
  partition while keeping OTA app size below the app partition limit.
- Use full `flash` after changing runtime assets or `partitions.csv`.
- Full-frame assets are used for character animation. The project does not
  generate isolated mouth, eye, or facial parts and paste them onto a base
  image.

## Integration notes

- `firmware/repos.json` lists fetched upstream repositories.
- `firmware/patches/xiaozhi-esp32.patch` records the Xiaozhi customizations.
- The StackChan output path expects an MQTT broker reachable from the device.
- The local Claude Code Channels setup may use the Telegram and StackChan
  plugins, including `mcp__stackchan__reply`.
- `docs/irodori-tts-runtime-notes.md` records the speech output path, runtime
  tuning, and debug checklist.

## Open items

- Continue improving speech input stability on the physical StackChan.
- Connect the camera icon to the current image input flow.
- Keep generated image assets reproducible and documented.
