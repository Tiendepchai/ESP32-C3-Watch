import SwiftUI

struct ContentView: View {
    @EnvironmentObject private var bleClient: BLEConfigClient

    @State private var navSource = "Google Maps"
    @State private var navTitle = "Turn right"
    @State private var navInstruction = "Turn right onto Nguyen Hue"
    @State private var navDistance = "200 m"
    @State private var navEta = "5 min"

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

                Section {
                    TextField("Source", text: $navSource)
                    TextField("Title", text: $navTitle)
                    TextField("Instruction", text: $navInstruction, axis: .vertical)
                        .lineLimit(2...4)
                    TextField("Distance", text: $navDistance)
                    TextField("ETA", text: $navEta)

                    HStack {
                        Button("Send Navigation") {
                            bleClient.pushNavigation(source: navSource,
                                                     title: navTitle,
                                                     instruction: navInstruction,
                                                     distance: navDistance,
                                                     eta: navEta)
                        }
                        .disabled(!bleClient.isConnected)

                        Spacer()

                        Button("Clear") {
                            bleClient.clearNavigation()
                        }
                        .disabled(!bleClient.isConnected)
                    }
                } header: {
                    Text("Navigation Relay")
                } footer: {
                    Text("Relay nay la kenh custom de test/manually push navigation sang ESP32. iOS khong doc duoc Live Activities cua Google Maps.")
                }

                Section("Navigation Preview") {
                    LabeledContent("Status", value: bleClient.navigationState.active ? "Active" : "Inactive")
                    LabeledContent("Source", value: previewText(bleClient.navigationState.source))
                    LabeledContent("Title", value: previewText(bleClient.navigationState.title))
                    LabeledContent("Instruction", value: previewText(bleClient.navigationState.instruction))
                    LabeledContent("Distance", value: previewText(bleClient.navigationState.distance))
                    LabeledContent("ETA", value: previewText(bleClient.navigationState.eta))
                    LabeledContent("Sequence", value: "\(bleClient.navigationState.sequence)")
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

                Section("Catalog Pages") {
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

    private func previewText(_ value: String) -> String {
        value.isEmpty ? "-" : value
    }
}

#Preview {
    ContentView()
        .environmentObject(BLEConfigClient())
}
