# Irodori TTS runtime notes

This note records the StackChan speech output path and the runtime tuning
decisions learned while integrating Irodori TTS.

## Current path

StackChan voice or Telegram input is handled by Claude Code Channels. When the
assistant calls `mcp__stackchan__reply`, the StackChan channel server displays
the reply text and starts Irodori TTS audio generation.

The speech path is:

```text
Claude Code Channels
-> StackChan MCP reply
-> channels/stackchan/server.ts
-> Irodori TTS ZeroGPU Space
-> MP3 fetch
-> Opus frames over StackChan audio websocket
-> StackChan speaker
```

The StackChan API is a thin CPU Basic adapter for public HTTP access. The local
StackChan server can also call the ZeroGPU Space directly with a Hugging Face
token. The actual model inference runs in the ZeroGPU Space.

MQTT audio is not part of the production speech path. It is kept only as a
debug path because Opus frames over MQTT caused severe audible gaps and cut
points on the physical StackChan. Production Irodori TTS audio should use the
StackChan audio websocket, while MQTT remains for text output, device state,
and debugging.

## Runtime settings

- StackChan server default TTS steps: `18`
- Local runtime override file: `~/.config/stackchan/irodori.env`
- A fast local override can set `STACKCHAN_IRODORI_TTS_STEPS=8`
- Duration scale: `0.95`
- Audio transport: `STACKCHAN_IRODORI_TTS_AUDIO_TRANSPORT=websocket`
- ZeroGPU function budget: `@spaces.GPU(duration=30)`
- ZeroGPU model precision: `bf16`
- ZeroGPU codec precision: `fp32`

`duration=30` is the GPU task time budget, not the generated voice length. It
is kept below `60` to improve ZeroGPU scheduling while leaving enough room for
calendar or scheduling replies that are longer than a greeting.

## Tuning notes

- `steps` is the largest controllable generation-speed knob. Lower values are
  faster. `18` is the repository default, while `8` is useful for fast
  interactive testing.
- `duration_scale=0.95` trims the duration predictor output slightly and helps
  reduce tail noise on short prompts.
- Leave `seconds` empty for normal use. Fixed `seconds` can cut off longer
  replies.
- Keep the codec in `fp32` while running the model in `bf16`. This keeps audio
  decode behavior easier to reason about.
- BF16 requires tensors that enter model layers to match the model dtype.
  Reference audio latents are cast to the model dtype before speaker encoding.
- ZeroGPU `xlarge` may be worth testing for latency under Hugging Face Pro
  quota, but it consumes twice the ZeroGPU quota. Paid dedicated GPU hardware
  is not part of the default setup.

## Debug checklist

Check the local StackChan server first:

```bash
curl -s http://127.0.0.1:18080/health
```

Useful fields:

- `lastMqttOutput`: Claude reply text reached the StackChan server.
- `recentMqttAudioPublishes`: Irodori audio was generated and published. Check
  `transport`; production audio should be `ws`, not `mqtt`.
- `irodoriTts.audioTransport`: should be `websocket` for the local Nukoevi
  runtime.
- `irodoriWarmup`: warmup state. Warmup can be canceled when synthesis starts.
- `xiaozhiAudio.recentUpstreamTextMessages`: STT and local device events.

Check the public adapter:

```bash
curl -s https://schroneko-irodori-tts-stackchan-api.hf.space/health
```

Check the ZeroGPU backend:

```bash
hf spaces logs schroneko/irodori-tts-zerogpu -n 200
hf spaces logs schroneko/irodori-tts-stackchan-api -n 200
```

If the StackChan API returns `502`, inspect the ZeroGPU logs first. A successful
StackChan API response should include `metadata.success=true`,
`generation_seconds`, `total_to_decode`, and `stage_timings`.

## Known failure modes

- If text appears but no Irodori audio is published, check
  `irodoriTts.audioTransport`, `stackChanAudioClients`, and the latest
  `recentMqttAudioPublishes` error.
- Do not switch production audio back to MQTT to hide websocket connection
  failures. MQTT audio is known to be too choppy for speech playback.
- After changing ZeroGPU code, wait until the Hugging Face Space runtime `sha`
  matches the repo `sha`; the repo can update before the running app restarts.
- Startup log messages like `Invalid file descriptor: -1` and missing
  `SilentCipher` have been observed as non-blocking for synthesis.
- If BF16 is enabled and logs show dtype mismatch, verify that reference latents
  are cast to the model dtype before speaker encoding.
