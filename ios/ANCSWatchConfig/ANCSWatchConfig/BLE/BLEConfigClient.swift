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
            if lowercased.contains("google.maps") { return "Google Maps" }
            if lowercased.contains("apple.maps") { return "Apple Maps" }
            if lowercased.contains("waze") { return "Waze" }
            if lowercased.contains("momo") || lowercased.contains("mservice") { return "MoMo" }
            if lowercased.contains("tpbank") || lowercased.contains("tpb") { return "TPBank" }
            if lowercased.contains("mbbank") || lowercased.contains("mb mobile") || lowercased.contains("mbmobile") { return "MB Bank" }
            return id
        }
    }
}

struct NavigationRelayState: Equatable {
    var active = false
    var sequence = 0
    var source = ""
    var title = ""
    var instruction = ""
    var distance = ""
    var eta = ""
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
    @Published var navigationState = NavigationRelayState()

    private let targetName = "C3-ANCS"
    private let serviceUUID = CBUUID(string: "605E4D3C-2B9A-019C-5B4A-3E4F00104A9F")
    private let summaryUUID = CBUUID(string: "605E4D3C-2B9A-019C-5B4A-3E4F01104A9F")
    private let pageUUID = CBUUID(string: "605E4D3C-2B9A-019C-5B4A-3E4F02104A9F")
    private let catalogUUID = CBUUID(string: "605E4D3C-2B9A-019C-5B4A-3E4F03104A9F")
    private let toggleUUID = CBUUID(string: "605E4D3C-2B9A-019C-5B4A-3E4F04104A9F")
    private let navigationUUID = CBUUID(string: "605E4D3C-2B9A-019C-5B4A-3E4F05104A9F")

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var summaryCharacteristic: CBCharacteristic?
    private var pageCharacteristic: CBCharacteristic?
    private var catalogCharacteristic: CBCharacteristic?
    private var toggleCharacteristic: CBCharacteristic?
    private var navigationCharacteristic: CBCharacteristic?
    private var shouldAutoReconnect = true

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: nil)
    }

    func startScanning() {
        guard central.state == .poweredOn else { return }
        shouldAutoReconnect = true
        bluetoothState = "Scanning..."
        discoveredNames = []
        central.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }

    func disconnect() {
        guard let peripheral else { return }
        shouldAutoReconnect = false
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

    func pushNavigation(source: String, title: String, instruction: String, distance: String, eta: String) {
        let trimmedTitle = title.trimmingCharacters(in: .whitespacesAndNewlines)
        let trimmedInstruction = instruction.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmedTitle.isEmpty || !trimmedInstruction.isEmpty else { return }

        var next = navigationState
        next.active = true
        next.sequence = max(navigationState.sequence + 1, 1)
        next.source = source.trimmingCharacters(in: .whitespacesAndNewlines)
        next.title = trimmedTitle
        next.instruction = trimmedInstruction
        next.distance = distance.trimmingCharacters(in: .whitespacesAndNewlines)
        next.eta = eta.trimmingCharacters(in: .whitespacesAndNewlines)
        writeNavigation(next)
    }

    func clearNavigation() {
        guard let navigationCharacteristic, let peripheral else { return }
        peripheral.writeValue(Data("clear".utf8), for: navigationCharacteristic, type: .withResponse)
    }

    private func writeNavigation(_ state: NavigationRelayState) {
        guard let navigationCharacteristic, let peripheral else { return }
        let payload = """
        active=\(state.active ? 1 : 0)
        sequence=\(state.sequence)
        source=\(state.source)
        title=\(state.title)
        instruction=\(state.instruction)
        distance=\(state.distance)
        eta=\(state.eta)
        """
        peripheral.writeValue(Data(payload.utf8), for: navigationCharacteristic, type: .withResponse)
    }

    private func connect(to peripheral: CBPeripheral) {
        if self.peripheral?.identifier != peripheral.identifier {
            resetConnectionState(clearDiscoveredNames: false)
        }
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
        if let navigationCharacteristic {
            peripheral.readValue(for: navigationCharacteristic)
        }
    }

    private func resetConnectionState(clearDiscoveredNames: Bool) {
        peripheral = nil
        summaryCharacteristic = nil
        pageCharacteristic = nil
        catalogCharacteristic = nil
        toggleCharacteristic = nil
        navigationCharacteristic = nil
        isConnected = false
        entries = []
        currentPage = 0
        totalPages = 1
        totalEntries = 0
        revision = 0
        callsEnabled = true
        navigationState = NavigationRelayState()
        if clearDiscoveredNames {
            discoveredNames = []
        }
    }

    private func isCurrentPeripheral(_ peripheral: CBPeripheral) -> Bool {
        self.peripheral?.identifier == peripheral.identifier
    }

    private func handleUnavailableBluetoothState(_ status: String) {
        bluetoothState = status
        shouldAutoReconnect = false
        central.stopScan()
        resetConnectionState(clearDiscoveredNames: true)
    }

    private func parseKeyValueText(_ text: String) -> [String: String] {
        var values: [String: String] = [:]

        for line in text.split(separator: "\n") {
            let parts = line.split(separator: "=", maxSplits: 1).map(String.init)
            if parts.count == 2 {
                values[parts[0]] = parts[1]
            }
        }

        return values
    }

    private func parseSummary(_ text: String) {
        let values = parseKeyValueText(text)

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

    private func parseNavigation(_ text: String) {
        let values = parseKeyValueText(text)
        let active = (Int(values["active"] ?? "") ?? 0) != 0

        if !active {
            navigationState = NavigationRelayState(sequence: Int(values["sequence"] ?? "") ?? navigationState.sequence)
            return
        }

        navigationState = NavigationRelayState(
            active: true,
            sequence: Int(values["sequence"] ?? "") ?? navigationState.sequence,
            source: values["source"] ?? navigationState.source,
            title: values["title"] ?? navigationState.title,
            instruction: values["instruction"] ?? navigationState.instruction,
            distance: values["distance"] ?? navigationState.distance,
            eta: values["eta"] ?? navigationState.eta
        )
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
                handleUnavailableBluetoothState("Bluetooth is off")
            case .unauthorized:
                handleUnavailableBluetoothState("Bluetooth unauthorized")
            case .unsupported:
                handleUnavailableBluetoothState("Bluetooth unsupported")
            default:
                handleUnavailableBluetoothState("Bluetooth unavailable")
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        Task { @MainActor in
            let name = peripheral.name ?? (advertisementData[CBAdvertisementDataLocalNameKey] as? String) ?? "Unknown"
            guard name.contains(targetName) else { return }
            guard self.peripheral == nil else { return }
            if !discoveredNames.contains(name) {
                discoveredNames.append(name)
            }
            central.stopScan()
            connect(to: peripheral)
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        Task { @MainActor in
            guard isCurrentPeripheral(peripheral) else { return }
            bluetoothState = "Connected"
            isConnected = true
            peripheral.discoverServices([serviceUUID])
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        Task { @MainActor in
            guard isCurrentPeripheral(peripheral) else { return }
            bluetoothState = "Disconnected"
            resetConnectionState(clearDiscoveredNames: false)
            if shouldAutoReconnect {
                startScanning()
            }
        }
    }
}

extension BLEConfigClient: CBPeripheralDelegate {
    nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        Task { @MainActor in
            guard isCurrentPeripheral(peripheral) else { return }
            guard error == nil else {
                bluetoothState = "Service discovery failed"
                return
            }

            guard let service = peripheral.services?.first(where: { $0.uuid == serviceUUID }) else {
                bluetoothState = "Config service not found"
                return
            }

            peripheral.discoverCharacteristics([summaryUUID, pageUUID, catalogUUID, toggleUUID, navigationUUID], for: service)
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        Task { @MainActor in
            guard isCurrentPeripheral(peripheral) else { return }
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
                case navigationUUID:
                    navigationCharacteristic = characteristic
                    peripheral.setNotifyValue(true, for: characteristic)
                default:
                    break
                }
            }

            guard summaryCharacteristic != nil,
                  pageCharacteristic != nil,
                  catalogCharacteristic != nil,
                  toggleCharacteristic != nil,
                  navigationCharacteristic != nil else {
                bluetoothState = "Config characteristics missing"
                return
            }

            refreshAll()
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        Task { @MainActor in
            guard isCurrentPeripheral(peripheral) else { return }
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
            case navigationUUID:
                parseNavigation(text)
            default:
                break
            }
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        Task { @MainActor in
            guard isCurrentPeripheral(peripheral) else { return }
            guard error == nil else {
                bluetoothState = "Write failed"
                return
            }
            refreshAll()
        }
    }
}
