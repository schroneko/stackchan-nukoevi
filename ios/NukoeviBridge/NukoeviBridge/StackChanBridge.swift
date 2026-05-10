@preconcurrency import CoreBluetooth
import Foundation

@MainActor
final class StackChanBridge: NSObject, ObservableObject {
    @Published var connectionState = "Starting Bluetooth"
    @Published var lastPrompt = ""
    @Published var lastResponse = ""
    @Published var isScanning = false

    private let serviceUUID = CBUUID(string: "E2E5E5E0-1234-5678-1234-56789ABCDEF0")
    private let configUUID = CBUUID(string: "E2E5E5E3-1234-5678-1234-56789ABCDEF0")
    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var configCharacteristic: CBCharacteristic?
    private let responder = AppleIntelligenceResponder()

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: nil)
    }

    func start() {
        guard central.state == .poweredOn else {
            connectionState = "Bluetooth is not ready"
            return
        }

        isScanning = true
        connectionState = "Scanning for StackChan"
        central.scanForPeripherals(withServices: [serviceUUID], options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }

    private func handleConfigData(_ data: Data) {
        guard
            let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
            let command = object["cmd"] as? String,
            command == "chatPrompt",
            let payload = object["data"] as? [String: Any],
            let prompt = payload["text"] as? String
        else {
            return
        }

        let requestID = payload["id"] as? Int
        lastPrompt = prompt
        connectionState = "Thinking with Apple Intelligence"

        Task {
            let response = await responder.response(for: prompt)
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
        isScanning = false
        connectionState = "Connecting to StackChan"
        self.peripheral = peripheral
        peripheral.delegate = self
        central.stopScan()
        central.connect(peripheral)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        connectionState = "Discovering StackChan service"
        peripheral.discoverServices([serviceUUID])
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        configCharacteristic = nil
        connectionState = "Disconnected"
        start()
    }
}

extension StackChanBridge: @MainActor CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let service = peripheral.services?.first(where: { $0.uuid == serviceUUID }) else {
            connectionState = "StackChan service not found"
            return
        }

        peripheral.discoverCharacteristics([configUUID], for: service)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristic = service.characteristics?.first(where: { $0.uuid == configUUID }) else {
            connectionState = "StackChan config channel not found"
            return
        }

        configCharacteristic = characteristic
        peripheral.setNotifyValue(true, for: characteristic)
        connectionState = "Connected to StackChan"
        prepareResponder(sendStatus: true)
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard let data = characteristic.value else {
            return
        }

        handleConfigData(data)
    }
}
