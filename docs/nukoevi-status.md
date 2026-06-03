# Nukoevi status

This page is a public status summary for the Nukoevi StackChan firmware.

## Implemented

- Nukoevi app is registered as the startup app.
- Nukoevi character frames are generated as full-screen image assets.
- Blink and sleepy display frames are compiled into the app as C image
  descriptors. Talk frames are packed into the firmware assets partition.
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
- Sleepy mode switches to the embedded sleepy Nukoevi frame from 22:00 to
  07:00 JST.
- Saved Wi-Fi credentials retry on startup timeout instead of automatically
  entering Wi-Fi configuration mode.

## Asset workflow

- Source and preview images live under
  `firmware/main/apps/app_nukoevi/source-assets/`.
- Runtime `.bin` assets live under `firmware/main/assets/assets_bin/`.
- Sleepy mode uses
  `firmware/main/apps/app_nukoevi/assets/nukoevi_sleep.c`, generated from
  `firmware/main/apps/app_nukoevi/source-assets/nukoevi-sleep-frame-0.png`.
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
- Add more animation frames only after confirming firmware partition budget.
- Keep generated image assets reproducible and documented.
