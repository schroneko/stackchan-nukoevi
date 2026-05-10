import Foundation

#if canImport(FoundationModels)
import FoundationModels
#endif

actor AppleIntelligenceResponder {
    func availabilityStatus() -> String {
#if canImport(FoundationModels)
        if #available(iOS 26.0, *) {
            switch SystemLanguageModel.default.availability {
            case .available:
                return "AI ready. Tap StackChan."
            case .unavailable(let reason):
                return "AI unavailable: \(reason)"
            }
        }
#endif
        return "AI unavailable: FoundationModels"
    }

    func response(for prompt: String) async -> String {
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

            do {
                let session = LanguageModelSession(model: model, instructions: "あなたはぬこエビちゃんです。日本語で短くかわいく返事してください。")
                let answer = try await session.respond(to: prompt)
                return String(answer.content.prefix(80))
            } catch let error as LanguageModelSession.GenerationError {
                let message = "AI generation failed: \(error)"
                print(message)
                return message
            } catch {
                let message = "AI failed: \(error.localizedDescription)"
                print(message)
                return message
            }
        }
#endif
        return "AI unavailable: FoundationModels"
    }
}
