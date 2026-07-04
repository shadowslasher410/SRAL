#ifndef VOICEOVER_H_
#define VOICEOVER_H_
#pragma once

#if defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>

#if TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_OSX

#include "../Include/SRAL.h"
#include "Engine.h"

namespace Sral {

class VoiceOver final : public Engine {
public:
	VoiceOver() noexcept = default;
	~VoiceOver() noexcept override = default;

	VoiceOver(const VoiceOver&) = delete;
	VoiceOver& operator=(const VoiceOver&) = delete;
	VoiceOver(VoiceOver&&) = delete;
	VoiceOver& operator=(VoiceOver&&) = delete;

	bool Speak(const char* text, bool interrupt) override;
	bool StopSpeech() override;

	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_VOICE_OVER; }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	[[nodiscard]] bool GetActive() override;

	bool Initialize() override;
	bool Uninitialize() override;

	[[nodiscard]] int GetFeatures() override { return SRAL_SUPPORTS_SPEECH; }
};

} // namespace Sral

#endif /* TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_OSX */
#endif /* defined(__APPLE__) || defined(__MACH__) */
#endif /* VOICEOVER_H_ */
