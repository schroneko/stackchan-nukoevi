@preconcurrency import CoreBluetooth
import Foundation
import UIKit

@MainActor
final class StackChanBridge: NSObject, ObservableObject {
    @Published var connectionState = "Starting Bluetooth"
    @Published var lastPrompt = ""
    @Published var lastResponse = ""
    @Published var lastCameraImage: UIImage?
    @Published var lastSavedCameraImagePath = ""
    @Published var logFilePath = ""
    @Published var isScanning = false

    private let serviceUUID = CBUUID(string: "E2E5E5E0-1234-5678-1234-56789ABCDEF0")
    private let configUUID = CBUUID(string: "E2E5E5E3-1234-5678-1234-56789ABCDEF0")
    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var configCharacteristic: CBCharacteristic?
    private let responder = AppleIntelligenceResponder()
    private var cameraTransfers: [Int: CameraTransfer] = [:]
    private var cameraImages: [Int: Data] = [:]
    private var latestCameraImageData: Data?
    private var hasRequestedInitialCapture = false
    private var requestedConfigRead = false

    private struct CameraTransfer {
        var chunks: [Data?]

        var isComplete: Bool {
            chunks.allSatisfy { $0 != nil }
        }

        var data: Data {
            chunks.reduce(into: Data()) { result, chunk in
                if let chunk {
                    result.append(chunk)
                }
            }
        }
    }

    override init() {
        super.init()
        resetLog()
        appendLog("bridge init")
        central = CBCentralManager(delegate: self, queue: nil)
    }

    func start() {
        appendLog("start requested, central state: \(central.state.rawValue)")
        guard central.state == .poweredOn else {
            connectionState = "Bluetooth is not ready"
            appendLog("bluetooth is not ready")
            return
        }

        isScanning = true
        connectionState = "Scanning for StackChan"
        central.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
        appendLog("scan started")
    }

    private func resetLog() {
        guard let url = logURL else {
            return
        }

        try? "".write(to: url, atomically: true, encoding: .utf8)
        logFilePath = url.path
    }

    private var logURL: URL? {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first?.appendingPathComponent("bridge-log.txt")
    }

    private func appendLog(_ message: String) {
        guard let url = logURL else {
            return
        }

        let line = "\(ISO8601DateFormatter().string(from: Date())) \(message)\n"
        if FileManager.default.fileExists(atPath: url.path),
           let handle = try? FileHandle(forWritingTo: url) {
            try? handle.seekToEnd()
            try? handle.write(contentsOf: Data(line.utf8))
            try? handle.close()
        } else {
            try? line.write(to: url, atomically: true, encoding: .utf8)
        }
        logFilePath = url.path
    }

    private func handleConfigData(_ data: Data) -> Bool {
        guard
            let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
            let command = object["cmd"] as? String,
            let payload = object["data"] as? [String: Any]
        else {
            appendLog("ignored invalid config data, bytes: \(data.count)")
            return false
        }

        appendLog("received command: \(command)")
        switch command {
        case "cameraStart":
            handleCameraStart(payload)
        case "cameraChunk":
            handleCameraChunk(payload)
        case "chatPrompt":
            handleChatPrompt(payload)
        default:
            break
        }
        return true
    }

    private func handleCameraStart(_ payload: [String: Any]) {
        guard
            let id = payload["id"] as? Int,
            let chunks = payload["chunks"] as? Int,
            chunks > 0
        else {
            return
        }

        cameraTransfers[id] = CameraTransfer(chunks: Array(repeating: nil, count: chunks))
        connectionState = "Receiving StackChan camera"
        appendLog("camera start id: \(id), chunks: \(chunks)")
    }

    private func handleCameraChunk(_ payload: [String: Any]) {
        guard
            let id = payload["id"] as? Int,
            let index = payload["index"] as? Int,
            let encoded = payload["data"] as? String,
            let chunk = Data(base64Encoded: encoded),
            var transfer = cameraTransfers[id],
            transfer.chunks.indices.contains(index)
        else {
            return
        }

        transfer.chunks[index] = chunk
        if transfer.isComplete {
            let imageData = transfer.data
            cameraTransfers[id] = nil
            cameraImages[id] = imageData
            latestCameraImageData = imageData
            lastCameraImage = UIImage(data: imageData)
            lastSavedCameraImagePath = saveCameraImage(imageData, requestID: id) ?? ""
            connectionState = lastSavedCameraImagePath.isEmpty ? "StackChan camera received" : "StackChan camera saved"
            appendLog("camera complete id: \(id), bytes: \(imageData.count), path: \(lastSavedCameraImagePath)")
        } else {
            cameraTransfers[id] = transfer
            let received = transfer.chunks.filter { $0 != nil }.count
            connectionState = "Receiving camera \(received)/\(transfer.chunks.count)"
        }
    }

    private func saveCameraImage(_ data: Data, requestID: Int) -> String? {
        guard let directory = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first else {
            return nil
        }

        let formatter = ISO8601DateFormatter()
        formatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        let timestamp = formatter.string(from: Date()).replacingOccurrences(of: ":", with: "-")
        let url = directory.appendingPathComponent("stackchan-camera-\(requestID)-\(timestamp).jpg")

        do {
            try data.write(to: url, options: .atomic)
            return url.path
        } catch {
            appendLog("failed to save camera image: \(error.localizedDescription)")
            return nil
        }
    }

    private func handleChatPrompt(_ payload: [String: Any]) {
        guard let prompt = payload["text"] as? String else {
            return
        }

        let requestID = payload["id"] as? Int
        let imageData: Data?
        if let requestID, let requestImage = cameraImages.removeValue(forKey: requestID) {
            imageData = requestImage
        } else {
            imageData = latestCameraImageData
        }

        lastPrompt = prompt
        connectionState = "Thinking with Apple Intelligence"
        appendLog("chat prompt id: \(requestID ?? 0), has image: \(imageData != nil), prompt: \(prompt)")

        Task {
            let response = await responder.response(for: prompt, imageData: imageData)
            await MainActor.run {
                self.lastResponse = response
                self.sendResponse(response, requestID: requestID)
            }
        }
    }

    private func sendResponse(_ text: String, requestID: Int? = nil) {
        guard let peripheral, let configCharacteristic else {
            connectionState = "StackChan is not connected"
            return
        }

        var dataPayload: [String: Any] = [
            "text": String(text.prefix(80))
        ]
        if let requestID {
            dataPayload["id"] = requestID
        }

        let payload: [String: Any] = [
            "cmd": "chatResponse",
            "data": dataPayload
        ]

        guard let data = try? JSONSerialization.data(withJSONObject: payload) else {
            connectionState = "Failed to encode response"
            return
        }

        peripheral.writeValue(data, for: configCharacteristic, type: .withResponse)
        connectionState = "Response sent to StackChan"
        appendLog("chat response write requested, bytes: \(data.count)")
    }

    func requestCapture() {
        guard let peripheral, let configCharacteristic else {
            connectionState = "StackChan is not connected"
            appendLog("capture request skipped, not connected")
            return
        }

        let payload: [String: Any] = [
            "cmd": "captureRequest",
            "data": [:]
        ]

        guard let data = try? JSONSerialization.data(withJSONObject: payload) else {
            connectionState = "Failed to encode capture request"
            return
        }

        peripheral.writeValue(data, for: configCharacteristic, type: .withResponse)
        connectionState = "Capture requested"
        appendLog("capture request write requested, bytes: \(data.count)")
    }

    private func prepareResponder(sendStatus: Bool = false) {
        Task {
            let status = await responder.prepare()
            await MainActor.run {
                self.lastResponse = status
                if sendStatus {
                    self.sendResponse(status)
                }
            }
        }
    }
}

extension StackChanBridge: @MainActor CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        appendLog("central state changed: \(central.state.rawValue)")
        switch central.state {
        case .poweredOn:
            start()
        case .poweredOff:
            connectionState = "Bluetooth is off"
        case .unauthorized:
            connectionState = "Bluetooth permission is required"
        case .unsupported:
            connectionState = "Bluetooth is unsupported"
        default:
            connectionState = "Bluetooth is not ready"
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let services = advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID] ?? []
        let localName = advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? peripheral.name ?? ""
        appendLog("discovered name: \(localName), services: \(services.map(\.uuidString)), rssi: \(RSSI)")
        guard services.contains(serviceUUID), localName == "StackChan" else {
            return
        }

        isScanning = false
        connectionState = "Connecting to StackChan"
        self.peripheral = peripheral
        peripheral.delegate = self
        central.stopScan()
        central.connect(peripheral)
        appendLog("connecting to \(localName)")
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        connectionState = "Discovering StackChan service"
        appendLog("connected peripheral: \(peripheral.name ?? "(unknown)")")
        peripheral.discoverServices([serviceUUID])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        appendLog("failed to connect: \(error?.localizedDescription ?? "(none)")")
        connectionState = "Failed to connect"
        start()
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        configCharacteristic = nil
        connectionState = "Disconnected"
        appendLog("disconnected: \(error?.localizedDescription ?? "(none)")")
        start()
    }
}

extension StackChanBridge: @MainActor CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        appendLog("services discovered error: \(error?.localizedDescription ?? "(none)")")
        guard let service = peripheral.services?.first(where: { $0.uuid == serviceUUID }) else {
            connectionState = "StackChan service not found"
            appendLog("stackchan service not found")
            return
        }

        peripheral.discoverCharacteristics([configUUID], for: service)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        appendLog("characteristics discovered error: \(error?.localizedDescription ?? "(none)")")
        guard let characteristic = service.characteristics?.first(where: { $0.uuid == configUUID }) else {
            connectionState = "StackChan config channel not found"
            appendLog("config characteristic not found")
            return
        }

        configCharacteristic = characteristic
        peripheral.setNotifyValue(true, for: characteristic)
        connectionState = "Connected to StackChan"
        appendLog("config characteristic ready")
        prepareResponder(sendStatus: true)
        if !hasRequestedInitialCapture {
            hasRequestedInitialCapture = true
            Task {
                try? await Task.sleep(for: .seconds(1))
                await MainActor.run {
                    self.requestCapture()
                }
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        if let error {
            appendLog("value update error: \(error.localizedDescription)")
            return
        }

        guard let data = characteristic.value else {
            appendLog("value update without data")
            return
        }

        let handled = handleConfigData(data)
        if handled {
            requestedConfigRead = false
        } else if requestedConfigRead {
            requestedConfigRead = false
            appendLog("config read still invalid, bytes: \(data.count)")
        } else if data.count >= 500 {
            requestedConfigRead = true
            peripheral.readValue(for: characteristic)
            appendLog("requested config read after truncated update")
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        appendLog("notification state updated uuid: \(characteristic.uuid.uuidString), notifying: \(characteristic.isNotifying), error: \(error?.localizedDescription ?? "(none)")")
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        appendLog("write completed uuid: \(characteristic.uuid.uuidString), error: \(error?.localizedDescription ?? "(none)")")
    }
}
