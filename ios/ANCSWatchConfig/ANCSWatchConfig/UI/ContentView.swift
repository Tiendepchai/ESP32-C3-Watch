import SwiftUI

struct ContentView: View {
    @EnvironmentObject private var bleClient: BLEConfigClient

    var body: some View {
        NavigationStack {
            List {
                Section("Connection") {
                    LabeledContent("State", value: bleClient.bluetoothState)
                    LabeledContent("Page", value: "\(bleClient.currentPage + 1)/\(bleClient.totalPages)")
                    LabeledContent("Entries", value: "\(bleClient.totalEntries)")
                    LabeledContent("Revision", value: "\(bleClient.revision)")

                    if bleClient.isConnected {
                        Button("Disconnect", role: .destructive) {
                            bleClient.disconnect()
                        }
                    } else {
                        Button("Scan Again") {
                            bleClient.startScanning()
                        }
                    }
                }

                if !bleClient.discoveredNames.isEmpty {
                    Section("Seen Devices") {
                        ForEach(bleClient.discoveredNames, id: \.self) { name in
                            Text(name)
                        }
                    }
                }

                Section("Allowed Apps") {
                    if bleClient.entries.isEmpty {
                        Text("No app catalog on current page yet.")
                            .foregroundStyle(.secondary)
                    } else {
                        ForEach(bleClient.entries) { entry in
                            Toggle(entry.displayName, isOn: Binding(
                                get: { entry.allowed },
                                set: { bleClient.toggle(entry, allowed: $0) }
                            ))
                        }
                    }
                }

                Section("Navigation") {
                    HStack {
                        Button("Previous") {
                            bleClient.previousPage()
                        }
                        .disabled(bleClient.currentPage <= 0 || !bleClient.isConnected)

                        Spacer()

                        Button("Next") {
                            bleClient.nextPage()
                        }
                        .disabled((bleClient.currentPage + 1) >= bleClient.totalPages || !bleClient.isConnected)
                    }
                }
            }
            .navigationTitle("ANCS Config")
        }
    }
}

#Preview {
    ContentView()
        .environmentObject(BLEConfigClient())
}
