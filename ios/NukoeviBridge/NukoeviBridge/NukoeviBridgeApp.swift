import SwiftUI

@main
struct NukoeviBridgeApp: App {
    @StateObject private var bridge = StackChanBridge()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(bridge)
        }
    }
}
