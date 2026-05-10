import SwiftUI

struct ContentView: View {
    @EnvironmentObject private var bridge: StackChanBridge

    var body: some View {
        VStack(spacing: 18) {
            Text("Nukoevi Bridge")
                .font(.largeTitle.bold())

            Text(bridge.connectionState)
                .font(.headline)

            Text(bridge.lastPrompt.isEmpty ? "Waiting for StackChan" : bridge.lastPrompt)
                .font(.body)
                .multilineTextAlignment(.center)
                .frame(maxWidth: .infinity)
                .padding()
                .background(.thinMaterial)
                .clipShape(RoundedRectangle(cornerRadius: 16))

            if let image = bridge.lastCameraImage {
                Image(uiImage: image)
                    .resizable()
                    .scaledToFit()
                    .frame(maxHeight: 180)
                    .clipShape(RoundedRectangle(cornerRadius: 16))
            }

            Text(bridge.lastResponse.isEmpty ? "Apple Intelligence response will appear here" : bridge.lastResponse)
                .font(.body)
                .multilineTextAlignment(.center)
                .frame(maxWidth: .infinity)
                .padding()
                .background(.thinMaterial)
                .clipShape(RoundedRectangle(cornerRadius: 16))

            Button(bridge.isScanning ? "Scanning" : "Connect") {
                bridge.start()
            }
            .buttonStyle(.borderedProminent)
            .controlSize(.large)
        }
        .padding(24)
        .onAppear {
            bridge.start()
        }
    }
}
