# Voice input runtime notes

This note records the StackChan voice input path and the operational lessons
learned while debugging repeated `マイク起動中` failures.

## Current path

The microphone button uses the standard StackChan/Xiaozhi voice input path, but
the response is routed through the local StackChan channel server.

```text
StackChan microphone button
-> StackChan/Xiaozhi listen start
-> StackChan channel server /xiaozhi websocket
-> upstream Xiaozhi AI Agent websocket
-> STT text
-> Claude Code Channels through StackChan MCP
-> StackChan text display
-> Irodori TTS audio websocket
```

MQTT is used for text and device state observability. It is not the production
audio transport for Irodori TTS.

## Runtime expectations

- The physical StackChan must be connected to the local MQTT broker.
- The physical StackChan may open a `/xiaozhi` websocket while voice input is
  active.
- The local StackChan channel server must keep the physical StackChan websocket
  open across turns.
- After an STT result, the server may close only the upstream Xiaozhi websocket.
  Closing the physical StackChan websocket after every turn causes repeated
  reconnects and can lead to `マイク起動中` or `準備できなかったの` loops.
- The firmware should not start a microphone turn when the local relay is not
  connected. It should show a connection message instead of entering
  `マイク起動中`.

## Health checklist

Check the local StackChan server:

```bash
curl -s http://127.0.0.1:18080/health
```

Useful fields:

- `mqtt.brokerClients`: should include `nukoevi-stackchan-...` and
  `stackchan-relay-...`.
- `stackChanClients`: should become `1` when the physical StackChan has an
  active `/xiaozhi` websocket.
- `xiaozhiAudio.recentEvents`: should show `listen start` and `listen stop`
  around a microphone press and release.
- `xiaozhiAudio.recentUpstreamTextMessages`: should show `stt` with text after
  a successful utterance.
- `lastMqttState`: shows the latest firmware state event, such as
  `mic.pressed`, `xiaozhi.status`, `mic.released`, or `xiaozhi.stop.requested`.

## Debug interpretation

- If `mqtt.brokerClients` does not include the physical StackChan, the device is
  not connected to the relay. The microphone should not enter voice input.
- If `mic.pressed` appears and `xiaozhi.status` reaches `Listening...`, the
  relay and the start path worked.
- If `Listening...` appears but no `stt` text appears after release, the failure
  is after listen start and before STT completion.
- If the first turn works but later turns loop on `マイク起動中`, check whether
  the server closed the physical StackChan `/xiaozhi` websocket after the prior
  turn.
- If `audioTransport` is `websocket` and `mqttOutputAudioCount` is `0`, Irodori
  TTS audio is not being sent over MQTT.

## Known failure modes

- Closing the physical StackChan websocket after STT is a bug. It forces the
  firmware to reconnect for the next turn and can make the microphone UI look
  stuck.
- Keeping `suppressResponse` state on a reused upstream connection is a bug. A
  new microphone turn needs a fresh upstream Xiaozhi websocket.
- Restarting the device or the channel server can recover the system, but that
  is not a root fix. The stable fix is to keep the physical websocket alive and
  recreate only stale upstream or MQTT clients.
- MQTT audio should not be used to mask websocket audio issues. It caused severe
  audible gaps on the physical StackChan.
