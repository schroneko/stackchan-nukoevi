import Foundation
import UIKit
import VisionKit

#if canImport(FoundationModels)
import FoundationModels
#endif

actor AppleIntelligenceResponder {
#if canImport(FoundationModels)
    @available(iOS 26.0, *)
    private var session: LanguageModelSession?
#endif

    func prepare() async -> String {
#if canImport(FoundationModels)
        if #available(iOS 26.0, *) {
            let model = SystemLanguageModel.default
            switch model.availability {
            case .available:
                let session = currentSession(for: model)
                session.prewarm(promptPrefix: Prompt("ぬこエビちゃんとして、短く返事して。"))
                return "AI ready. Tap StackChan."
            case .unavailable(let reason):
                return "AI unavailable: \(reason)"
            }
        }
#endif
        return "AI unavailable: FoundationModels"
    }

    func response(for prompt: String, imageData: Data? = nil) async -> String {
        let cameraContext = await context(from: imageData)
        let modelPrompt: String
        if let cameraContext {
            modelPrompt = """
            \(prompt)

            StackChan camera analysis:
            \(cameraContext)
            """
        } else {
            modelPrompt = prompt
        }

        return await responseText(for: modelPrompt)
    }

    private func context(from imageData: Data?) async -> String? {
        guard let imageData, let image = UIImage(data: imageData) else {
            return nil
        }

        guard ImageAnalyzer.isSupported else {
            return "StackChan camera image was received, but ImageAnalyzer is not supported on this device."
        }

        do {
            let analyzer = ImageAnalyzer()
            var configuration = ImageAnalyzer.Configuration([.text, .machineReadableCode, .visualLookUp])
            configuration.locales = ["ja-JP", "en-US"]
            let analysis = try await analyzer.analyze(image, configuration: configuration)
            let transcript = analysis.transcript.trimmingCharacters(in: .whitespacesAndNewlines)
            if transcript.isEmpty {
                return "StackChan camera image was received, but no text or visual lookup transcript was extracted."
            }
            return transcript
        } catch {
            let nsError = error as NSError
            return "StackChan camera image was received, but image analysis failed: \(nsError.localizedDescription)"
        }
    }

    private func responseText(for prompt: String) async -> String {
#if canImport(FoundationModels)
        if #available(iOS 26.0, *) {
            let model = SystemLanguageModel.default
            switch model.availability {
            case .available:
                break
            case .unavailable(let reason):
                let message = "AI unavailable: \(reason)"
                print(message)
                return message
            }

            for attempt in 0..<5 {
                do {
                    let session = currentSession(for: model)
                    let answer = try await session.respond(to: prompt)
                    return String(answer.content.prefix(80))
                } catch let error as LanguageModelSession.GenerationError {
                    switch error {
                    case .rateLimited, .concurrentRequests:
                        if attempt < 4 {
                            try? await Task.sleep(for: .seconds(3))
                            continue
                        }
                    default:
                        break
                    }

                    let message = generationErrorMessage(error)
                    print(message)
                    return message
                } catch {
                    if attempt < 4 {
                        try? await Task.sleep(for: .seconds(3))
                        continue
                    }

                    let message = genericErrorMessage(error)
                    print(message)
                    return message
                }
            }
        }
#endif
        return "AI unavailable: FoundationModels"
    }

#if canImport(FoundationModels)
    @available(iOS 26.0, *)
    private func currentSession(for model: SystemLanguageModel) -> LanguageModelSession {
        if let session {
            return session
        }

        let newSession = LanguageModelSession(model: model, instructions: "あなたはぬこエビちゃんです。日本語で短くかわいく返事してください。")
        session = newSession
        return newSession
    }

    @available(iOS 26.0, *)
    private func generationErrorMessage(_ error: LanguageModelSession.GenerationError) -> String {
        switch error {
        case .rateLimited(let context):
            return "AI rateLimited: \(context.debugDescription)"
        case .concurrentRequests(let context):
            return "AI concurrent: \(context.debugDescription)"
        case .assetsUnavailable(let context):
            return "AI assetsUnavailable: \(context.debugDescription)"
        case .unsupportedLanguageOrLocale(let context):
            return "AI unsupportedLocale: \(context.debugDescription)"
        case .guardrailViolation(let context):
            return "AI guardrail: \(context.debugDescription)"
        case .decodingFailure(let context):
            return "AI decodingFailure: \(context.debugDescription)"
        case .exceededContextWindowSize(let context):
            return "AI contextTooLarge: \(context.debugDescription)"
        case .unsupportedGuide(let context):
            return "AI unsupportedGuide: \(context.debugDescription)"
        case .refusal(_, let context):
            return "AI refusal: \(context.debugDescription)"
        @unknown default:
            return "AI generation unknown: \(String(describing: error))"
        }
    }
#endif

    private func genericErrorMessage(_ error: Error) -> String {
        let nsError = error as NSError
        return "AI failed \(nsError.domain) \(nsError.code): \(nsError.localizedDescription)"
    }
}
