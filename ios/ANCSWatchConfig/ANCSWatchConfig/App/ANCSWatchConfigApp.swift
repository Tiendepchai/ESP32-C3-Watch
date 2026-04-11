import SwiftUI

@main
struct ANCSWatchConfigApp: App {
    @StateObject private var bleClient = BLEConfigClient()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(bleClient)
        }
    }
}
