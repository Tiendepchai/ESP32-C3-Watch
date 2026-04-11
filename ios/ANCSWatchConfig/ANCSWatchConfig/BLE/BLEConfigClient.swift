import CoreBluetooth
import Foundation

struct ConfigEntry: Identifiable, Hashable {
    let id: String
    var allowed: Bool

    var displayName: String {
        switch id {
        case "__calls__":
            return "Calls"
        default:
            let lowercased = id.lowercased()
            if lowercased.contains("zalo") { return "Zalo" }
            if lowercased.contains("telegram") || lowercased.contains("telegraph") { return "Telegram" }
            if lowercased.contains("messenger") { return "Messenger" }
            if lowercased.contains("instagram") { return "Instagram" }
            if lowercased.contains("momo") || lowercased.contains("mservice") { return "MoMo" }
            if lowercased.contains("tpbank") || lowercased.contains("tpb") { return "TPBank" }
            if lowercased.contains("mbbank") || lowercased.contains("mb mobile") || lowercased.contains("mbmobile") { return "MB Bank" }
            return id
        }
    }
}

@MainActor
final class BLEConfigClient: NSObject, ObservableObject {
    @Published var bluetoothState = "Starting Bluetooth..."
    @Published var discoveredNames: [String] = []
    @Published var isConnected = false
    @Published var entries: [ConfigEntry] = []
    @Published var currentPage = 0
    @Published var totalPages = 1
    @Published var totalEntries = 0
    @Published var revision = 0
    @Published var callsEnabled = true

    private let targetName = "C3-ANCS"
    private let serviceUUID = CBUUID(string: "605E4D3C-2B9A-019C-5B4A-3E4F00104A9F")
    private let summaryUUID = CBUUID(string: "605E4D3C-2B9A-019C-5B4A-3E4F01104A9F")
    private let pageUUID = CBUUID(string: "605E4D3C-2B9A-019C-5B4A-3E4F02104A9F")
    private let catalogUUID = CBUUID(string: "605E4D3C-2B9A-019C-5B4A-3E4F03104A9F")
    private let toggleUUID = CBUUID(string: "605E4D3C-2B9A-019C-5B4A-3E4F04104A9F")

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var summaryCharacteristic: CBCharacteristic?
    private var pageCharacteristic: CBCharacteristic?
    private var catalogCharacteristic: CBCharacteristic?
    private var toggleCharacteristic: CBCharacteristic?

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: nil)
    }

    func startScanning() {
        guard central.state == .poweredOn else { return }
        bluetoothState = "Scanning..."
        discoveredNames = []
        central.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }

    func disconnect() {
        guard let peripheral else { return }
        central.cancelPeripheralConnection(peripheral)
    }

    func nextPage() {
        setPage(min(currentPage + 1, max(totalPages - 1, 0)))
    }

    func previousPage() {
        setPage(max(currentPage - 1, 0))
    }

    func toggle(_ entry: ConfigEntry, allowed: Bool) {
        guard let toggleCharacteristic, let peripheral else { return }
        let payload = "\(entry.id)|\(allowed ? 1 : 0)"
        peripheral.writeValue(Data(payload.utf8), for: toggleCharacteristic, type: .withResponse)
    }

    private func connect(to peripheral: CBPeripheral) {
        self.peripheral = peripheral
        peripheral.delegate = self
        bluetoothState = "Connecting..."
        central.connect(peripheral, options: nil)
    }

    private func setPage(_ page: Int) {
        guard let pageCharacteristic, let peripheral else { return }
        let payload = Data(String(page).utf8)
        peripheral.writeValue(payload, for: pageCharacteristic, type: .withResponse)
    }

    private func refreshAll() {
        guard let peripheral else { return }
        if let summaryCharacteristic {
            peripheral.readValue(for: summaryCharacteristic)
        }
        if let pageCharacteristic {
            peripheral.readValue(for: pageCharacteristic)
        }
        if let catalogCharacteristic {
            peripheral.readValue(for: catalogCharacteristic)
        }
    }

    private func parseSummary(_ text: String) {
        var values: [String: String] = [:]

        for line in text.split(separator: "\n") {
            let parts = line.split(separator: "=", maxSplits: 1).map(String.init)
            if parts.count == 2 {
                values[parts[0]] = parts[1]
            }
        }

        currentPage = Int(values["page"] ?? "") ?? currentPage
        totalPages = max(Int(values["pages"] ?? "") ?? totalPages, 1)
        totalEntries = Int(values["count"] ?? "") ?? totalEntries
        revision = Int(values["revision"] ?? "") ?? revision
        callsEnabled = (Int(values["calls"] ?? "") ?? 1) != 0
    }

    private func parseCatalog(_ text: String) {
        entries = text
            .split(separator: "\n")
            .compactMap { line in
                let parts = line.split(separator: "|", maxSplits: 1).map(String.init)
                guard parts.count == 2 else { return nil }
                return ConfigEntry(id: parts[0], allowed: parts[1] == "1")
            }
    }
}

extension BLEConfigClient: CBCentralManagerDelegate {
    nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        Task { @MainActor in
            switch central.state {
            case .poweredOn:
                bluetoothState = "Bluetooth ready"
                startScanning()
            case .poweredOff:
                bluetoothState = "Bluetooth is off"
            case .unauthorized:
                bluetoothState = "Bluetooth unauthorized"
            case .unsupported:
                bluetoothState = "Bluetooth unsupported"
            default:
                bluetoothState = "Bluetooth unavailable"
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        Task { @MainActor in
            let name = peripheral.name ?? (advertisementData[CBAdvertisementDataLocalNameKey] as? String) ?? "Unknown"
            guard name.contains(targetName) else { return }
            if !discoveredNames.contains(name) {
                discoveredNames.append(name)
            }
            central.stopScan()
            connect(to: peripheral)
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        Task { @MainActor in
            bluetoothState = "Connected"
            isConnected = true
            peripheral.discoverServices([serviceUUID])
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        Task { @MainActor in
            bluetoothState = "Disconnected"
            isConnected = false
            entries = []
            currentPage = 0
            totalPages = 1
            startScanning()
        }
    }
}

extension BLEConfigClient: CBPeripheralDelegate {
    nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        Task { @MainActor in
            guard error == nil else {
                bluetoothState = "Service discovery failed"
                return
            }

            guard let service = peripheral.services?.first(where: { $0.uuid == serviceUUID }) else {
                bluetoothState = "Config service not found"
                return
            }

            peripheral.discoverCharacteristics([summaryUUID, pageUUID, catalogUUID, toggleUUID], for: service)
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        Task { @MainActor in
            guard error == nil else {
                bluetoothState = "Characteristic discovery failed"
                return
            }

            service.characteristics?.forEach { characteristic in
                switch characteristic.uuid {
                case summaryUUID:
                    summaryCharacteristic = characteristic
                    peripheral.setNotifyValue(true, for: characteristic)
                case pageUUID:
                    pageCharacteristic = characteristic
                    peripheral.setNotifyValue(true, for: characteristic)
                case catalogUUID:
                    catalogCharacteristic = characteristic
                    peripheral.setNotifyValue(true, for: characteristic)
                case toggleUUID:
                    toggleCharacteristic = characteristic
                default:
                    break
                }
            }

            guard summaryCharacteristic != nil,
                  pageCharacteristic != nil,
                  catalogCharacteristic != nil,
                  toggleCharacteristic != nil else {
                bluetoothState = "Config characteristics missing"
                return
            }

            refreshAll()
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        Task { @MainActor in
            guard error == nil, let data = characteristic.value, let text = String(data: data, encoding: .utf8) else {
                return
            }

            switch characteristic.uuid {
            case summaryUUID:
                parseSummary(text)
                if let catalogCharacteristic {
                    peripheral.readValue(for: catalogCharacteristic)
                }
            case pageUUID:
                currentPage = Int(text) ?? currentPage
            case catalogUUID:
                parseCatalog(text)
            default:
                break
            }
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        Task { @MainActor in
            guard error == nil else {
                bluetoothState = "Write failed"
                return
            }
            refreshAll()
        }
    }
}
