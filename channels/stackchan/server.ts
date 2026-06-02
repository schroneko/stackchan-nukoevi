#!/usr/bin/env bun

import { Server } from '@modelcontextprotocol/sdk/server/index.js'
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js'
import {
  CallToolRequestSchema,
  ListToolsRequestSchema,
} from '@modelcontextprotocol/sdk/types.js'
import { spawnSync } from 'child_process'
import { randomUUID } from 'crypto'
import { mkdirSync, readFileSync, writeFileSync } from 'fs'
import { createServer } from 'net'
import { homedir } from 'os'
import { Aedes } from 'aedes'
import mqtt from 'mqtt'

const host = process.env.STACKCHAN_CHANNEL_HOST ?? '0.0.0.0'
const publicHost = process.env.STACKCHAN_PUBLIC_HOST ?? '192.168.1.10'
const port = Number(process.env.STACKCHAN_CHANNEL_PORT ?? '18080')
const assistantTimeoutMs = Number(process.env.STACKCHAN_REPLY_TIMEOUT_MS ?? '120000')
const upstreamOtaUrl = process.env.STACKCHAN_UPSTREAM_OTA_URL ?? 'https://api.tenclass.net/xiaozhi/ota/'
const directMcpChannel = process.env.STACKCHAN_DIRECT_MCP_CHANNEL === '1'
const statePath = process.env.STACKCHAN_RELAY_STATE_PATH ?? `${homedir()}/.local/state/stackchan-xiaozhi-relay/upstream.json`
const mqttEnabled = process.env.NUKOEVI_MQTT_ENABLED !== '0'
const mqttEmbedded = process.env.NUKOEVI_MQTT_EMBEDDED !== '0'
const mqttBrokerHost = process.env.NUKOEVI_MQTT_BROKER_HOST ?? '0.0.0.0'
const mqttHost = process.env.NUKOEVI_MQTT_HOST ?? '127.0.0.1'
const mqttPort = Number(process.env.NUKOEVI_MQTT_PORT ?? '18883')
const mqttUrl = process.env.NUKOEVI_MQTT_URL ?? `mqtt://${mqttHost}:${mqttPort}`
const mqttTopicPrefix = process.env.NUKOEVI_MQTT_TOPIC_PREFIX ?? 'nukoevi'
const mqttInputTopic = `${mqttTopicPrefix}/input/text`
const mqttOutputTopic = `${mqttTopicPrefix}/output/text`
const mqttOutputAudioTopic = `${mqttTopicPrefix}/output/audio/opus`
const mqttStateTopic = `${mqttTopicPrefix}/device/stackchan/state`
const irodoriTtsUrl = process.env.STACKCHAN_IRODORI_TTS_URL ?? 'https://schroneko-irodori-tts-stackchan-api.hf.space/synthesis'
const irodoriTtsKey = process.env.STACKCHAN_IRODORI_TTS_KEY ?? ''
const irodoriTtsSpeaker = process.env.STACKCHAN_IRODORI_TTS_SPEAKER ?? '3'
const irodoriTtsSteps = process.env.STACKCHAN_IRODORI_TTS_STEPS ?? '24'
const irodoriTtsSeconds = process.env.STACKCHAN_IRODORI_TTS_SECONDS ?? ''
const irodoriTtsEnabled = process.env.STACKCHAN_IRODORI_TTS_ENABLED !== '0'
const irodoriTtsFrameDelayMs = Number(process.env.STACKCHAN_IRODORI_TTS_FRAME_DELAY_MS ?? '55')
const assistantSpeechQueueMs = Number(process.env.STACKCHAN_ASSISTANT_SPEECH_QUEUE_MS ?? '30000')
const evictlBin = process.env.STACKCHAN_EVICTL_BIN ?? `${homedir()}/ghq/github.com/schroneko/evictl/bin/evictl`
const evictlIdentity = process.env.STACKCHAN_EVICTL_IDENTITY ?? 'nukoevi'

type StackChanRequest = {
  id: string
  sessionId: string
  deviceId: string
  createdAt: number
  resolve: (text: string) => void
  socket?: ServerWebSocket<StackChanConnection>
}

type StackChanConnection = {
  sessionId: string
  deviceId: string
  clientId: string
  protocolVersion: string
  upstream?: UpstreamConnection
  transcript?: string
  claudeAsked?: boolean
  turnClosing?: boolean
}

type UpstreamConfig = {
  url: string
  token: string
  version: number
}

type OpenAIMessage = {
  role?: string
  content?: unknown
}

type NukoeviMqttEvent = {
  id?: string
  correlation_id?: string
  type?: string
  source?: string
  target?: string
  channel?: string
  text?: string
  session_id?: string
  device_id?: string
  sample_rate?: number
  frame_duration?: number
  sequence?: number
  total?: number
  payload?: string
  ts?: string
}

type PendingAssistantSpeech = {
  text: string
  createdAt: number
}

type PendingMqttAssistantAudio = {
  text: string
  createdAt: number
}

const pending = new Map<string, StackChanRequest>()
const upstreamConfigs = new Map<string, UpstreamConfig>()
const stackChanSockets = new Set<ServerWebSocket<StackChanConnection>>()
const pendingAssistantSpeech: PendingAssistantSpeech[] = []
const pendingMqttAssistantAudio: PendingMqttAssistantAudio[] = []
let mqttClient: ReturnType<typeof mqtt.connect> | undefined
let mqttReady: Promise<void> | undefined
let mqttInputCount = 0
let mqttOutputCount = 0
let mqttOutputAudioCount = 0
let mqttStateCount = 0
let assistantSpeechSending = false
let mqttAssistantAudioSending = false
let lastMqttInput: NukoeviMqttEvent | undefined
let lastMqttOutput: NukoeviMqttEvent | undefined
let lastMqttOutputAudio: NukoeviMqttEvent | undefined
let lastMqttState: NukoeviMqttEvent | undefined
let lastAssistantSpeech: { text: string; queued: boolean; ts: string } | undefined
const recentMqttStates: NukoeviMqttEvent[] = []
let lastChannelEmit: { text: string; meta: Record<string, string>; ts: string } | undefined

function loadUpstreamConfigs() {
  try {
    const data = JSON.parse(readFileSync(statePath, 'utf8')) as Record<string, UpstreamConfig>
    for (const [key, config] of Object.entries(data)) {
      if (config?.url) upstreamConfigs.set(key, config)
    }
    log(`loaded upstream config cache: ${upstreamConfigs.size}`)
  } catch {
  }
}

function saveUpstreamConfigs() {
  const data: Record<string, UpstreamConfig> = {}
  for (const [key, config] of upstreamConfigs) {
    data[key] = config
  }
  mkdirSync(statePath.replace(/\/[^/]+$/, ''), { recursive: true })
  writeFileSync(statePath, JSON.stringify(data, null, 2))
}

function nowIso() {
  return new Date().toISOString()
}

function log(message: string) {
  process.stderr.write(`stackchan channel: ${message}\n`)
}

function startEmbeddedMqttBroker() {
  if (!mqttEnabled || !mqttEmbedded) return
  void Aedes.createBroker().then(broker => {
    const server = createServer(broker.handle)
    server.listen(mqttPort, mqttBrokerHost, () => {
      log(`MQTT broker listening: mqtt://${mqttBrokerHost}:${mqttPort}`)
    })
    server.on('error', err => {
      log(`MQTT broker skipped: ${err}`)
    })
  }).catch(err => {
    log(`MQTT broker skipped: ${err}`)
  })
}

async function ensureMqtt() {
  if (!mqttEnabled) return undefined
  if (mqttClient) return mqttClient
  if (mqttReady) {
    await mqttReady
    return mqttClient
  }

  mqttClient = mqtt.connect(mqttUrl, {
    clientId: `stackchan-relay-${randomUUID()}`,
    reconnectPeriod: 1000,
  })

  mqttClient.on('message', (topic, payload) => {
    if (topic === mqttOutputTopic) void handleMqttOutput(payload.toString())
    if (topic === mqttInputTopic) void handleMqttInput(payload.toString())
    if (topic === mqttStateTopic) void handleMqttState(payload.toString())
  })
  mqttClient.on('error', err => {
    log(`MQTT error: ${err}`)
  })
  mqttClient.on('reconnect', () => {
    log('MQTT reconnecting')
  })

  mqttReady = new Promise(resolve => {
    mqttClient?.once('connect', () => {
      log(`MQTT connected: ${mqttUrl}`)
      const topics = directMcpChannel ? [mqttOutputTopic, mqttInputTopic, mqttStateTopic] : [mqttOutputTopic, mqttStateTopic]
      mqttClient?.subscribe(topics, err => {
        if (err) log(`MQTT subscribe failed: ${err}`)
        else log(`MQTT subscribed: ${topics.join(', ')}`)
        resolve()
      })
    })
  })

  await mqttReady
  return mqttClient
}

function mqttEvent(type: string, text: string, values: Partial<NukoeviMqttEvent> = {}): NukoeviMqttEvent {
  return {
    id: values.id ?? randomUUID(),
    type,
    source: values.source ?? 'stackchan',
    text,
    ts: nowIso(),
    ...values,
  }
}

async function publishMqtt(topic: string, event: NukoeviMqttEvent) {
  const client = await ensureMqtt()
  if (!client) return
  client.publish(topic, JSON.stringify(event), { qos: 0 }, err => {
    if (err) log(`MQTT publish failed: ${topic} ${err}`)
  })
}

async function publishMqttInput(event: NukoeviMqttEvent) {
  await publishMqtt(mqttInputTopic, event)
}

async function publishMqttOutput(event: NukoeviMqttEvent) {
  await publishMqtt(mqttOutputTopic, event)
}

async function publishMqttOutputAudio(event: NukoeviMqttEvent) {
  mqttOutputAudioCount += 1
  lastMqttOutputAudio = event
  await publishMqtt(mqttOutputAudioTopic, event)
}

async function publishStackChanState(state: string, values: Partial<NukoeviMqttEvent> = {}) {
  await publishMqtt(mqttStateTopic, mqttEvent('device.state', state, values))
}

async function handleMqttOutput(raw: string) {
  let event: NukoeviMqttEvent
  try {
    event = JSON.parse(raw) as NukoeviMqttEvent
  } catch {
    log(`MQTT output ignored: ${raw}`)
    return
  }

  const text = normalizeText(String(event.text ?? ''))
  if (!text) return
  mqttOutputCount += 1
  lastMqttOutput = event

  const requestId = event.correlation_id ?? event.id ?? ''
  const entry = requestId ? pending.get(requestId) : undefined
  if (entry) {
    entry.resolve(text)
    return
  }

  if (event.target && event.target !== 'stackchan' && event.target !== 'all') return
  void speakStackChanAssistant(text, true)
}

async function handleMqttState(raw: string) {
  let event: NukoeviMqttEvent
  try {
    event = JSON.parse(raw) as NukoeviMqttEvent
  } catch {
    log(`MQTT state ignored: ${raw}`)
    return
  }

  mqttStateCount += 1
  lastMqttState = event
  recentMqttStates.push(event)
  while (recentMqttStates.length > 20) {
    recentMqttStates.shift()
  }
}

async function handleMqttInput(raw: string) {
  if (!directMcpChannel) return

  let event: NukoeviMqttEvent
  try {
    event = JSON.parse(raw) as NukoeviMqttEvent
  } catch {
    log(`MQTT input ignored: ${raw}`)
    return
  }

  const text = normalizeText(String(event.text ?? ''))
  if (!text) return
  mqttInputCount += 1
  lastMqttInput = event

  const requestId = event.id ?? randomUUID()
  if (pending.has(requestId)) return

  const sessionId = event.session_id ?? 'mqtt'
  const deviceId = event.device_id ?? event.source ?? 'mqtt'
  const started = Date.now()
  const timer = setTimeout(() => {
    pending.delete(requestId)
    log(`MQTT input timed out: ${requestId}`)
  }, assistantTimeoutMs)

  pending.set(requestId, {
    id: requestId,
    sessionId,
    deviceId,
    createdAt: started,
    resolve: value => {
      clearTimeout(timer)
      pending.delete(requestId)
      log(`MQTT input resolved: ${requestId} ${value}`)
    },
  })

  await emitChannel(text, {
    source: event.source ?? 'mqtt',
    request_id: requestId,
    session_id: sessionId,
    device_id: deviceId,
    user: event.source ?? 'mqtt',
  }).catch(err => {
    clearTimeout(timer)
    pending.delete(requestId)
    log(`failed to deliver MQTT inbound to Claude: ${err}`)
  })
}

function textFromContent(content: unknown): string {
  if (typeof content === 'string') return content
  if (Array.isArray(content)) {
    return content
      .map(item => {
        if (typeof item === 'string') return item
        if (item && typeof item === 'object' && 'text' in item) {
          const text = (item as { text?: unknown }).text
          return typeof text === 'string' ? text : ''
        }
        return ''
      })
      .filter(Boolean)
      .join('\n')
  }
  return ''
}

function latestUserText(messages: OpenAIMessage[]): string {
  for (const message of [...messages].reverse()) {
    if (message.role === 'user') {
      const text = textFromContent(message.content).trim()
      if (text) return text
    }
  }
  return ''
}

function sendJson(ws: ServerWebSocket<StackChanConnection>, value: unknown) {
  ws.send(JSON.stringify(value))
}

function normalizeText(text: string) {
  return text.replace(/\s+/g, ' ').trim()
}

function sendStackChanText(ws: ServerWebSocket<StackChanConnection>, sessionId: string, text: string, role: 'user' | 'assistant') {
  const normalized = normalizeText(text)
  if (!normalized) return
  if (role === 'user') {
    sendJson(ws, { session_id: sessionId, type: 'stt', text: normalized })
  } else {
    sendJson(ws, { session_id: sessionId, type: 'llm', emotion: 'happy' })
    sendJson(ws, { session_id: sessionId, type: 'tts', state: 'start' })
    sendJson(ws, { session_id: sessionId, type: 'tts', state: 'sentence_start', text: normalized })
    sendJson(ws, { session_id: sessionId, type: 'tts', state: 'stop' })
  }
}

function sleep(ms: number) {
  return new Promise(resolve => setTimeout(resolve, ms))
}

function concatUint8Arrays(chunks: Uint8Array[]) {
  const total = chunks.reduce((sum, chunk) => sum + chunk.byteLength, 0)
  const output = new Uint8Array(total)
  let offset = 0
  for (const chunk of chunks) {
    output.set(chunk, offset)
    offset += chunk.byteLength
  }
  return output
}

function hasAsciiPrefix(value: Uint8Array, prefix: string) {
  if (value.byteLength < prefix.length) return false
  for (let index = 0; index < prefix.length; index++) {
    if (value[index] !== prefix.charCodeAt(index)) return false
  }
  return true
}

function oggOpusPackets(ogg: Uint8Array) {
  const packets: Uint8Array[] = []
  let offset = 0
  let current: Uint8Array[] = []

  while (offset + 27 <= ogg.byteLength) {
    if (String.fromCharCode(...ogg.slice(offset, offset + 4)) !== 'OggS') {
      throw new Error(`invalid ogg capture pattern at ${offset}`)
    }

    const segmentCount = ogg[offset + 26]
    const segmentTableOffset = offset + 27
    const payloadOffset = segmentTableOffset + segmentCount
    if (payloadOffset > ogg.byteLength) {
      throw new Error('truncated ogg segment table')
    }

    const segmentSizes = ogg.slice(segmentTableOffset, payloadOffset)
    const payloadSize = segmentSizes.reduce((sum, value) => sum + value, 0)
    const pageEnd = payloadOffset + payloadSize
    if (pageEnd > ogg.byteLength) {
      throw new Error('truncated ogg payload')
    }

    let cursor = payloadOffset
    for (const size of segmentSizes) {
      current.push(ogg.slice(cursor, cursor + size))
      cursor += size
      if (size < 255) {
        const packet = concatUint8Arrays(current)
        current = []
        if (!hasAsciiPrefix(packet, 'OpusHead') && !hasAsciiPrefix(packet, 'OpusTags')) {
          packets.push(packet)
        }
      }
    }

    offset = pageEnd
  }

  return packets
}

function encodeMp3ToOpusPackets(mp3: Uint8Array) {
  const result = spawnSync('ffmpeg', [
    '-hide_banner',
    '-loglevel',
    'error',
    '-i',
    'pipe:0',
    '-ar',
    '16000',
    '-ac',
    '1',
    '-c:a',
    'libopus',
    '-application',
    'voip',
    '-b:a',
    '24k',
    '-frame_duration',
    '60',
    '-f',
    'opus',
    'pipe:1',
  ], {
    input: Buffer.from(mp3),
    maxBuffer: 16 * 1024 * 1024,
  })

  if (result.status !== 0) {
    throw new Error(`ffmpeg failed: ${result.stderr.toString().trim()}`)
  }

  return oggOpusPackets(new Uint8Array(result.stdout))
}

async function synthesizeIrodoriMp3(text: string) {
  const url = new URL(irodoriTtsUrl)
  url.searchParams.set('text', text)
  url.searchParams.set('speaker', irodoriTtsSpeaker)
  url.searchParams.set('steps', irodoriTtsSteps)
  if (irodoriTtsSeconds) url.searchParams.set('seconds', irodoriTtsSeconds)
  if (irodoriTtsKey) url.searchParams.set('key', irodoriTtsKey)

  const response = await fetch(url)
  const body = await response.text()
  if (!response.ok) {
    throw new Error(`Irodori synthesis failed: ${response.status} ${body}`)
  }

  const json = JSON.parse(body) as { success?: boolean; mp3StreamingUrl?: string; mp3DownloadUrl?: string; error?: string }
  if (json.success === false) {
    throw new Error(`Irodori synthesis failed: ${json.error ?? body}`)
  }

  const audioUrl = json.mp3StreamingUrl ?? json.mp3DownloadUrl
  if (!audioUrl) {
    throw new Error(`Irodori response has no mp3 URL: ${body}`)
  }

  const audioResponse = await fetch(audioUrl)
  if (!audioResponse.ok) {
    throw new Error(`Irodori audio download failed: ${audioResponse.status}`)
  }
  return new Uint8Array(await audioResponse.arrayBuffer())
}

async function sendStackChanAssistant(ws: ServerWebSocket<StackChanConnection>, sessionId: string, text: string) {
  const normalized = normalizeText(text)
  if (!normalized) return

  sendJson(ws, { session_id: sessionId, type: 'llm', emotion: 'happy' })
  sendJson(ws, { session_id: sessionId, type: 'tts', state: 'start' })
  sendJson(ws, { session_id: sessionId, type: 'tts', state: 'sentence_start', text: normalized })

  if (irodoriTtsEnabled) {
    try {
      log(`Irodori TTS request: ${normalized}`)
      const mp3 = await synthesizeIrodoriMp3(normalized)
      const packets = encodeMp3ToOpusPackets(mp3)
      log(`Irodori TTS packets: ${packets.length}`)
      for (const packet of packets) {
        ws.send(packet)
        if (irodoriTtsFrameDelayMs > 0) {
          await sleep(irodoriTtsFrameDelayMs)
        }
      }
    } catch (err) {
      log(`Irodori TTS skipped: ${err}`)
    }
  }

  sendJson(ws, { session_id: sessionId, type: 'tts', state: 'stop' })
}

async function sendStackChanAssistantSafe(ws: ServerWebSocket<StackChanConnection>, text: string) {
  try {
    await sendStackChanAssistant(ws, ws.data.sessionId, text)
  } catch (err) {
    log(`StackChan assistant speech failed: ${err}`)
  }
}

async function publishMqttAssistantAudio(text: string) {
  if (!mqttEnabled || !irodoriTtsEnabled) return

  try {
    log(`Irodori MQTT TTS request: ${text}`)
    const mp3 = await synthesizeIrodoriMp3(text)
    const packets = encodeMp3ToOpusPackets(mp3)
    log(`Irodori MQTT TTS packets: ${packets.length}`)
    for (let i = 0; i < packets.length; i++) {
      await publishMqttOutputAudio(mqttEvent('output.audio.opus', text, {
        source: 'stackchan-relay',
        target: 'stackchan',
        sample_rate: 16000,
        frame_duration: 60,
        sequence: i,
        total: packets.length,
        payload: Buffer.from(packets[i]).toString('base64'),
      }))
      if (irodoriTtsFrameDelayMs > 0) {
        await sleep(irodoriTtsFrameDelayMs)
      }
    }
  } catch (err) {
    log(`Irodori MQTT TTS skipped: ${err}`)
  }
}

function trimPendingMqttAssistantAudio() {
  const expiresAt = Date.now() - assistantSpeechQueueMs
  while (pendingMqttAssistantAudio.length > 0 && pendingMqttAssistantAudio[0].createdAt < expiresAt) {
    pendingMqttAssistantAudio.shift()
  }
  while (pendingMqttAssistantAudio.length > 4) {
    pendingMqttAssistantAudio.shift()
  }
}

async function flushPendingMqttAssistantAudio() {
  if (mqttAssistantAudioSending) return
  trimPendingMqttAssistantAudio()
  if (pendingMqttAssistantAudio.length === 0) return

  mqttAssistantAudioSending = true
  try {
    while (pendingMqttAssistantAudio.length > 0) {
      const item = pendingMqttAssistantAudio.shift()
      if (!item) continue
      await publishMqttAssistantAudio(item.text)
    }
  } finally {
    mqttAssistantAudioSending = false
  }
}

function queueMqttAssistantAudio(text: string) {
  const normalized = normalizeText(text)
  if (!normalized || !mqttEnabled || !irodoriTtsEnabled) return
  pendingMqttAssistantAudio.push({ text: normalized, createdAt: Date.now() })
  trimPendingMqttAssistantAudio()
  void flushPendingMqttAssistantAudio()
}

function trimPendingAssistantSpeech() {
  const expiresAt = Date.now() - assistantSpeechQueueMs
  while (pendingAssistantSpeech.length > 0 && pendingAssistantSpeech[0].createdAt < expiresAt) {
    pendingAssistantSpeech.shift()
  }
  while (pendingAssistantSpeech.length > 4) {
    pendingAssistantSpeech.shift()
  }
}

async function flushPendingAssistantSpeech() {
  if (assistantSpeechSending) return
  trimPendingAssistantSpeech()
  if (pendingAssistantSpeech.length === 0 || stackChanSockets.size === 0) return

  assistantSpeechSending = true
  try {
    while (pendingAssistantSpeech.length > 0 && stackChanSockets.size > 0) {
      const item = pendingAssistantSpeech.shift()
      if (!item) continue
      for (const ws of Array.from(stackChanSockets)) {
        await sendStackChanAssistantSafe(ws, item.text)
      }
    }
  } finally {
    assistantSpeechSending = false
  }
}

async function speakStackChanAssistant(text: string, queueWhenDisconnected: boolean) {
  const normalized = normalizeText(text)
  if (!normalized) return

  trimPendingAssistantSpeech()
  if (stackChanSockets.size === 0) {
    if (mqttEnabled) {
      lastAssistantSpeech = { text: normalized, queued: false, ts: nowIso() }
      queueMqttAssistantAudio(normalized)
    } else if (queueWhenDisconnected) {
      pendingAssistantSpeech.push({ text: normalized, createdAt: Date.now() })
      lastAssistantSpeech = { text: normalized, queued: true, ts: nowIso() }
      log(`StackChan assistant speech queued: ${normalized}`)
    }
    return
  }

  lastAssistantSpeech = { text: normalized, queued: false, ts: nowIso() }
  for (const ws of Array.from(stackChanSockets)) {
    void sendStackChanAssistantSafe(ws, normalized)
  }
}

function finishStackChanTurn(ws: ServerWebSocket<StackChanConnection>) {
  if (ws.data.turnClosing) return
  ws.data.turnClosing = true
  setTimeout(() => {
    ws.data.upstream?.close()
    ws.close(1000, 'turn complete')
  }, 300)
}

function headerObject(headers: Headers) {
  const output: Record<string, string> = {}
  for (const [key, value] of headers) {
    const lower = key.toLowerCase()
    if (lower === 'host' || lower === 'content-length' || lower === 'accept-encoding' || lower === 'connection') continue
    output[key] = value
  }
  return output
}

function authorizationValue(token: string) {
  if (!token) return ''
  return token.includes(' ') ? token : `Bearer ${token}`
}

function deviceKey(req: Request) {
  return req.headers.get('device-id') ?? req.headers.get('Device-Id') ?? 'stackchan'
}

async function fetchUpstreamOta(req: Request) {
  const body = await req.text()
  const response = await fetch(upstreamOtaUrl, {
    method: req.method,
    headers: headerObject(req.headers),
    body: req.method === 'GET' || req.method === 'HEAD' ? undefined : body,
  })
  const text = await response.text()
  let json: Record<string, unknown>
  try {
    json = JSON.parse(text)
  } catch {
    throw new Error(`upstream OTA returned non JSON: ${response.status}`)
  }
  if (!response.ok) {
    throw new Error(`upstream OTA failed: ${response.status} ${text}`)
  }
  return json
}

function parseUpstreamConfig(json: Record<string, unknown>): UpstreamConfig | undefined {
  const websocket = json.websocket
  if (!websocket || typeof websocket !== 'object') return undefined
  const value = websocket as Record<string, unknown>
  const url = typeof value.url === 'string' ? value.url : ''
  if (!url) return undefined
  const token = typeof value.token === 'string' ? value.token : ''
  const version = typeof value.version === 'number' ? value.version : 1
  return { url, token, version }
}

function localOtaResponse(upstream: Record<string, unknown>, version: number) {
  const response: Record<string, unknown> = {
    ...upstream,
    websocket: {
      url: `ws://${publicHost}:${port}/xiaozhi`,
      token: '',
      version,
    },
  }
  delete response.firmware
  delete response.mqtt
  return response
}

async function emitChannel(text: string, meta: Record<string, string>) {
  lastChannelEmit = { text, meta, ts: nowIso() }
  const requestId = meta.request_id ?? randomUUID()
  const source = meta.source ?? 'stackchan'
  const sessionId = meta.session_id ?? 'stackchan'
  const deviceId = meta.device_id ?? 'stackchan'
  const prompt = [
    `StackChan voice input arrived from ${source}.`,
    `request_id: ${requestId}`,
    `session_id: ${sessionId}`,
    `device_id: ${deviceId}`,
    `text: ${text}`,
    '',
    'Reply by calling mcp__stackchan__reply with this exact request_id and a concise Japanese text.',
    'Do not call Telegram tools for this StackChan voice input.',
  ].join('\n')
  const result = spawnSync(evictlBin, ['send', evictlIdentity, '--text', prompt, '--source', 'stackchan-relay'], {
    encoding: 'utf8',
  })
  if (result.status !== 0) {
    throw new Error(`evictl send failed: ${result.stderr || result.stdout}`)
  }
}

async function askClaude(text: string, socket: ServerWebSocket<StackChanConnection> | undefined, sessionId: string, deviceId: string): Promise<string> {
  const id = randomUUID()
  const started = Date.now()
  return await new Promise<string>((resolve) => {
    const timer = setTimeout(() => {
      pending.delete(id)
      resolve('時間がかかりすぎちゃったの。もう一回話しかけてね。')
    }, assistantTimeoutMs)

    pending.set(id, {
      id,
      sessionId,
      deviceId,
      createdAt: started,
      socket,
      resolve: value => {
        clearTimeout(timer)
        pending.delete(id)
        resolve(value)
      },
    })

    const event = mqttEvent('input.text', text, {
      id,
      source: socket ? 'stackchan' : 'http',
      channel: socket ? 'stackchan' : 'openai-compatible',
      session_id: sessionId,
      device_id: deviceId,
      target: 'claude',
    })

    void publishMqttInput(event).catch(err => {
      log(`failed to publish MQTT input: ${err}`)
    })

    if (!directMcpChannel) {
      log(`waiting MQTT output: ${id}`)
      return
    }

    void emitChannel(text, {
      source: 'stackchan',
      request_id: id,
      session_id: sessionId,
      device_id: deviceId,
      user: 'stackchan',
    }).catch(err => {
      clearTimeout(timer)
      pending.delete(id)
      log(`failed to deliver inbound to Claude: ${err}`)
      resolve('Claude Code Channels に送れなかったの。Mac 側を確認してね。')
    })
  })
}

class UpstreamConnection {
  private ws?: WebSocket
  private opened?: Promise<void>
  private suppressResponse = false

  constructor(
    private readonly local: ServerWebSocket<StackChanConnection>,
    private readonly config: UpstreamConfig,
  ) {}

  async send(value: string | Uint8Array | ArrayBuffer | Buffer) {
    await this.ensureOpen()
    this.ws?.send(value)
  }

  close() {
    this.ws?.close()
  }

  private async ensureOpen() {
    if (this.ws?.readyState === WebSocket.OPEN) return
    if (this.opened) return await this.opened

    this.opened = new Promise((resolve, reject) => {
      const headers: Record<string, string> = {
        'Protocol-Version': String(this.config.version || this.local.data.protocolVersion || '1'),
        'Device-Id': this.local.data.deviceId,
        'Client-Id': this.local.data.clientId,
      }
      const authorization = authorizationValue(this.config.token)
      if (authorization) headers.Authorization = authorization

      const ws = new WebSocket(this.config.url, { headers })
      const timer = setTimeout(() => {
        ws.close()
        reject(new Error(`upstream connect timeout: ${this.config.url}`))
      }, 10000)

      ws.addEventListener('open', () => {
        clearTimeout(timer)
        this.ws = ws
        log(`upstream connected: ${this.config.url}`)
        resolve()
      })
      ws.addEventListener('message', event => this.handleMessage(event.data))
      ws.addEventListener('close', event => {
        log(`upstream disconnected code=${event.code} reason=${event.reason}`)
        if (this.ws === ws) this.ws = undefined
      })
      ws.addEventListener('error', () => {
        reject(new Error(`upstream connect failed: ${this.config.url}`))
      })
    }).finally(() => {
      this.opened = undefined
    })

    return await this.opened
  }

  private handleMessage(data: unknown) {
    if (typeof data !== 'string') {
      if (!this.suppressResponse) this.local.send(data as ArrayBuffer)
      return
    }

    let message: { type?: string; state?: string; text?: string; session_id?: string }
    try {
      message = JSON.parse(data)
    } catch {
      if (!this.suppressResponse) this.local.send(data)
      return
    }

    if (message.type === 'hello') {
      if (message.session_id) this.local.data.sessionId = message.session_id
      this.local.send(data)
      return
    }

    if (message.type === 'stt' && message.text) {
      const transcript = normalizeText(message.text)
      if (!transcript) return
      this.local.data.transcript = transcript
      sendStackChanText(this.local, this.local.data.sessionId, transcript, 'user')
      if (!this.local.data.claudeAsked) {
        this.local.data.claudeAsked = true
        this.suppressResponse = true
        this.abortUpstream()
        void this.replyWithClaude(transcript)
      }
      return
    }

    if (this.suppressResponse && (message.type === 'llm' || message.type === 'tts')) return
    this.local.send(data)
  }

  private abortUpstream() {
    const sessionId = this.local.data.sessionId
    if (!sessionId) return
    this.ws?.send(JSON.stringify({ session_id: sessionId, type: 'abort' }))
  }

  private async replyWithClaude(transcript: string) {
    const reply = await askClaude(transcript, this.local, this.local.data.sessionId, this.local.data.deviceId)
    await sendStackChanAssistant(this.local, this.local.data.sessionId, reply)
    finishStackChanTurn(this.local)
  }
}

const mcp = new Server(
  { name: 'stackchan', version: '0.0.1' },
  {
    capabilities: {
      tools: {},
      experimental: {
        'claude/channel': {},
      },
    },
    instructions: [
      'Messages from StackChan arrive as <channel source="stackchan" request_id="..." session_id="..." device_id="..." user="stackchan" ts="...">.',
      'Anything that should appear on the StackChan display must be sent with the reply tool. Pass request_id back unchanged when replying to StackChan input. For output mirrored from another channel, reuse the exact same response text already sent to that channel, omit request_id, and do not compose a second variant.',
      'Do not use Telegram tools for StackChan replies.',
    ].join('\n'),
  },
)

mcp.setRequestHandler(ListToolsRequestSchema, async () => ({
  tools: [
    {
      name: 'reply',
      description: 'Reply to StackChan. Pass request_id from an inbound StackChan channel message when available. Without request_id, the text is broadcast to the StackChan display and voice output.',
      inputSchema: {
        type: 'object',
        properties: {
          request_id: { type: 'string' },
          text: { type: 'string' },
        },
        required: ['text'],
      },
    },
  ],
}))

mcp.setRequestHandler(CallToolRequestSchema, async req => {
  const args = (req.params.arguments ?? {}) as Record<string, unknown>
  if (req.params.name !== 'reply') {
    throw new Error(`unknown tool: ${req.params.name}`)
  }

  const requestId = String(args.request_id ?? '')
  const text = String(args.text ?? '').trim()
  const entry = pending.get(requestId)
  if (!entry) {
    await publishMqttOutput(mqttEvent('output.text', text, {
      correlation_id: requestId || randomUUID(),
      source: 'claude',
      target: 'all',
      session_id: 'broadcast',
      device_id: 'stackchan',
    }))
    return { content: [{ type: 'text', text: 'sent' }] }
  }

  await publishMqttOutput(mqttEvent('output.text', text, {
    correlation_id: requestId,
    source: 'claude',
    target: 'all',
    session_id: entry.sessionId,
    device_id: entry.deviceId,
  }))
  entry.resolve(text)
  return { content: [{ type: 'text', text: 'sent' }] }
})

async function handleChatCompletions(req: Request): Promise<Response> {
  const body = await req.json().catch(() => ({})) as { messages?: OpenAIMessage[]; model?: string }
  const text = latestUserText(body.messages ?? [])
  if (!text) {
    return Response.json({ error: { message: 'no user message' } }, { status: 400 })
  }

  const reply = await askClaude(text, undefined, 'openai-compatible', 'http')
  return Response.json({
    id: `chatcmpl-${randomUUID()}`,
    object: 'chat.completion',
    created: Math.floor(Date.now() / 1000),
    model: body.model ?? 'stackchan-claude-code-channels',
    choices: [
      {
        index: 0,
        message: {
          role: 'assistant',
          content: reply,
        },
        finish_reason: 'stop',
      },
    ],
  })
}

Bun.serve<StackChanConnection>({
  hostname: host,
  port,
  async fetch(req, server) {
    const url = new URL(req.url)
    if (url.pathname === '/health') {
      return Response.json({
        status: 'ok',
        upstreamOtaUrl,
        mqtt: {
          enabled: mqttEnabled,
          embedded: mqttEmbedded,
          brokerHost: mqttBrokerHost,
          url: mqttUrl,
          inputTopic: mqttInputTopic,
          outputTopic: mqttOutputTopic,
          outputAudioTopic: mqttOutputAudioTopic,
          stateTopic: mqttStateTopic,
        },
        stackChanClients: stackChanSockets.size,
        pendingRequests: pending.size,
        pendingAssistantSpeech: pendingAssistantSpeech.length,
        pendingMqttAssistantAudio: pendingMqttAssistantAudio.length,
        mqttAssistantAudioSending,
        mqttInputCount,
        mqttOutputCount,
        mqttOutputAudioCount,
        mqttStateCount,
        lastMqttInput,
        lastMqttOutput,
        lastMqttOutputAudio,
        lastMqttState,
        lastAssistantSpeech,
        recentMqttStates,
        lastChannelEmit,
      })
    }
    if (url.pathname.startsWith('/ota')) {
      try {
        const upstream = await fetchUpstreamOta(req)
        const config = parseUpstreamConfig(upstream)
        if (!config) {
          return Response.json(upstream)
        }
        const key = deviceKey(req)
        upstreamConfigs.set(key, config)
        upstreamConfigs.set('stackchan', config)
        saveUpstreamConfigs()
        log(`ota proxy: ${key} -> ${config.url}`)
        return Response.json(localOtaResponse(upstream, config.version))
      } catch (err) {
        log(`ota proxy failed: ${err}`)
        return Response.json({ error: 'upstream OTA failed' }, { status: 502 })
      }
    }
    if (url.pathname === '/v1/chat/completions' && req.method === 'POST') {
      return await handleChatCompletions(req)
    }
    if (url.pathname === '/xiaozhi' && server.upgrade(req, {
      data: {
        sessionId: randomUUID(),
        deviceId: req.headers.get('device-id') ?? req.headers.get('Device-Id') ?? 'stackchan',
        clientId: req.headers.get('client-id') ?? req.headers.get('Client-Id') ?? randomUUID(),
        protocolVersion: req.headers.get('protocol-version') ?? req.headers.get('Protocol-Version') ?? '1',
      },
    })) {
      return undefined
    }
    return new Response('not found', { status: 404 })
  },
  websocket: {
    open(ws) {
      stackChanSockets.add(ws)
      log(`connected ${ws.data.deviceId} session=${ws.data.sessionId}`)
      void publishStackChanState('connected', {
        source: 'stackchan',
        session_id: ws.data.sessionId,
        device_id: ws.data.deviceId,
      })
      setTimeout(() => {
        void flushPendingAssistantSpeech()
      }, 500)
    },
    async message(ws, message) {
      const config = upstreamConfigs.get(ws.data.deviceId) ?? upstreamConfigs.get('stackchan')
      if (!config) {
        sendStackChanText(ws, ws.data.sessionId, '標準 AI Agent の接続情報がまだ取れてないの。もう一回起動してね。', 'assistant')
        return
      }
      if (!ws.data.upstream) {
        ws.data.upstream = new UpstreamConnection(ws, config)
      }
      try {
        if (typeof message === 'string') {
          let parsed: { type?: string; state?: string }
          try {
            parsed = JSON.parse(message)
          } catch {
            parsed = {}
          }
          if (parsed.type === 'listen' && parsed.state === 'start') {
            ws.data.transcript = undefined
            ws.data.claudeAsked = false
            void publishStackChanState('listening', {
              source: 'stackchan',
              session_id: ws.data.sessionId,
              device_id: ws.data.deviceId,
            })
          }
          if (parsed.type === 'listen' && parsed.state === 'stop') {
            void publishStackChanState('idle', {
              source: 'stackchan',
              session_id: ws.data.sessionId,
              device_id: ws.data.deviceId,
            })
          }
          log(`xiaozhi -> upstream: ${message}`)
          await ws.data.upstream.send(message)
          return
        }
        const payload = message instanceof ArrayBuffer ? message : message.slice().buffer
        await ws.data.upstream.send(payload)
      } catch (err) {
        log(`upstream send failed: ${err}`)
        sendStackChanText(ws, ws.data.sessionId, '標準 AI Agent への接続で失敗しちゃったの。', 'assistant')
      }
    },
    close(ws, code, reason) {
      stackChanSockets.delete(ws)
      ws.data.upstream?.close()
      log(`disconnected ${ws.data.deviceId} code=${code} reason=${reason}`)
      void publishStackChanState('disconnected', {
        source: 'stackchan',
        session_id: ws.data.sessionId,
        device_id: ws.data.deviceId,
      })
    },
  },
})

const transport = new StdioServerTransport()
loadUpstreamConfigs()
startEmbeddedMqttBroker()
void ensureMqtt()
if (directMcpChannel) {
  await mcp.connect(transport)
}
