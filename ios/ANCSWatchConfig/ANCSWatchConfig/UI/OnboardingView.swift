import SwiftUI

struct OnboardingView: View {
    @Binding var hasCompletedOnboarding: Bool
    @State private var currentPage = 0
    
    private let pages: [(title: String, subtitle: String, symbol: String)] = [
        ("Welcome to ANCS Watch", "Configure your ESP32-C3 smartwatch to receive iPhone notifications", "applewatch.side.right"),
        ("Power On Your Watch", "Turn on your ESP32-C3 watch and wait for it to start advertising", "power"),
        ("Pair via Bluetooth", "Go to iPhone Settings > Bluetooth and pair with your watch (named C3-ANCS-xxx)", "link"),
        ("Stay Connected", "Keep the app open or in background to maintain the connection", "antenna.radiowaves.left.and.right")
    ]
    
    var body: some View {
        ZStack {
            LinearGradient(
                colors: [Color(red: 0.05, green: 0.10, blue: 0.16), Color(red: 0.03, green: 0.22, blue: 0.28)],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
            .ignoresSafeArea()
            
            VStack(spacing: 32) {
                Spacer()
                
                Image(systemName: pages[currentPage].symbol)
                    .font(.system(size: 80))
                    .foregroundStyle(Color(red: 1.00, green: 0.77, blue: 0.50))
                    .padding(.bottom, 20)
                
                VStack(spacing: 16) {
                    Text(pages[currentPage].title)
                        .font(.system(.title, design: .rounded).weight(.bold))
                        .foregroundStyle(.white)
                        .multilineTextAlignment(.center)
                        .padding(.horizontal, 32)
                    
                    Text(pages[currentPage].subtitle)
                        .font(.body)
                        .foregroundStyle(.white.opacity(0.72))
                        .multilineTextAlignment(.center)
                        .padding(.horizontal, 40)
                }
                
                Spacer()
                
                VStack(spacing: 16) {
                    PageIndicator(currentPage: currentPage, totalPages: pages.count)
                    
                    HStack(spacing: 20) {
                        if currentPage > 0 {
                            Button {
                                withAnimation(.easeInOut(duration: 0.3)) {
                                    currentPage -= 1
                                }
                            } label: {
                                Text("Back")
                                    .font(.headline)
                                    .foregroundStyle(.white)
                                    .frame(maxWidth: .infinity)
                                    .padding(.vertical, 16)
                                    .background(Color.white.opacity(0.1))
                                    .cornerRadius(16)
                            }
                        }
                        
                        Button {
                            if currentPage < pages.count - 1 {
                                withAnimation(.easeInOut(duration: 0.3)) {
                                    currentPage += 1
                                }
                            } else {
                                hasCompletedOnboarding = true
                                UserDefaults.standard.set(true, forKey: "hasCompletedOnboarding")
                            }
                        } label: {
                            Text(currentPage < pages.count - 1 ? "Next" : "Get Started")
                                .font(.headline)
                                .foregroundStyle(Color(red: 0.05, green: 0.10, blue: 0.16))
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 16)
                                .background(Color(red: 0.98, green: 0.55, blue: 0.27))
                                .cornerRadius(16)
                        }
                    }
                }
                .padding(.horizontal, 24)
                .padding(.bottom, 40)
            }
        }
    }
}

struct PageIndicator: View {
    let currentPage: Int
    let totalPages: Int
    
    var body: some View {
        HStack(spacing: 8) {
            ForEach(0..<totalPages, id: \.self) { index in
                Circle()
                    .fill(index == currentPage 
                        ? Color(red: 0.98, green: 0.55, blue: 0.27) 
                        : Color.white.opacity(0.3))
                    .frame(width: index == currentPage ? 12 : 8, height: index == currentPage ? 12 : 8)
                    .animation(.easeInOut(duration: 0.2), value: currentPage)
            }
        }
    }
}

struct HelpView: View {
    @Environment(\.dismiss) private var dismiss
    
    var body: some View {
        NavigationStack {
            ZStack {
                LinearGradient(
                    colors: [Color(red: 0.05, green: 0.10, blue: 0.16), Color(red: 0.03, green: 0.22, blue: 0.28)],
                    startPoint: .topLeading,
                    endPoint: .bottomTrailing
                )
                .ignoresSafeArea()
                
                ScrollView {
                    VStack(alignment: .leading, spacing: 24) {
                        HelpSection(
                            title: "1. Power On Your Watch",
                            content: "Turn on your ESP32-C3 smartwatch. It should start advertising automatically.",
                            symbol: "power"
                        )
                        
                        HelpSection(
                            title: "2. Pair with iPhone",
                            content: "Go to Settings > Bluetooth on your iPhone and look for a device named 'C3-ANCS-xxx'. Tap to pair.",
                            symbol: "link"
                        )
                        
                        HelpSection(
                            title: "3. Connect in App",
                            content: "Open this app and tap on your watch to connect. The app will sync notification settings.",
                            symbol: "antenna.radiowaves.left.and.right"
                        )
                        
                        HelpSection(
                            title: "4. Manage Notifications",
                            content: "Use the 'Apps' tab to choose which apps can send notifications to your watch.",
                            symbol: "app.badge.fill"
                        )
                        
                        HelpSection(
                            title: "Troubleshooting",
                            content: "If your watch doesn't appear:\n• Make sure it's powered on\n• Check that Bluetooth is enabled on your iPhone\n• Try restarting the watch\n• Reset BLE bonds by holding both buttons on the watch",
                            symbol: "questionmark.circle"
                        )
                    }
                    .padding(20)
                }
            }
            .navigationTitle("Help")
            .navigationBarTitleDisplayMode(.large)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") {
                        dismiss()
                    }
                    .foregroundStyle(Color(red: 0.98, green: 0.55, blue: 0.27))
                }
            }
            .toolbarBackground(Color(red: 0.05, green: 0.10, blue: 0.16).opacity(0.96), for: .navigationBar)
            .toolbarColorScheme(.dark, for: .navigationBar)
        }
    }
}

struct HelpSection: View {
    let title: String
    let content: String
    let symbol: String
    
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Label {
                Text(title)
                    .font(.headline)
                    .foregroundStyle(.white)
            } icon: {
                Image(systemName: symbol)
                    .foregroundStyle(Color(red: 0.98, green: 0.55, blue: 0.27))
            }
            
            Text(content)
                .font(.body)
                .foregroundStyle(.white.opacity(0.72))
        }
        .padding(16)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Color.white.opacity(0.05))
        .cornerRadius(16)
    }
}
