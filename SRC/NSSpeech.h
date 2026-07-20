#ifndef NS_SPEECH_H
#define NS_SPEECH_H

#if defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>

#if defined(TARGET_OS_OSX) && TARGET_OS_OSX

#include "../Include/SRAL.h"
#include "Engine.h"

namespace Sral {

class NSSpeech final : public Engine {
public:
	NSSpeech() noexcept = default;
	~NSSpeech() override = default;

	NSSpeech(const NSSpeech&) = delete;
	NSSpeech& operator=(const NSSpeech&) = delete;
	NSSpeech(NsSpeech&&) = delete;
	NSSpeech& operator=(NSSpeech&&) = delete;

	[[nodiscard]] bool Speak(const char* text, bool interrupt) override;
	[[nodiscard]] bool StopSpeech() override;
	[[nodiscard]] bool IsSpeaking() override;
	[[nodiscard]] bool GetActive() override;
	[[nodiscard]] bool SetParameter(int param, const void* value) override;
	[[nodiscard]] bool GetParameter(int param, void* value) override;

	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_NS_SPEECH; }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	[[nodiscard]] int GetFeatures() override { return SRAL_SUPPORTS_SPEECH; }

	[[nodiscard]] bool Initialize() override;
	[[nodiscard]] bool Uninitialize() override;

private:
	static void* obj;
};

} // namespace Sral

#endif /* defined(TARGET_OS_OSX) && TARGET_OS_OSX */
#endif /* defined(__APPLE__) || defined(__MACH__) */
#endif /* NS_SPEECH_H */
