#ifndef VOICEOVER_H_
#define VOICEOVER_H_
#pragma once
#include "../Include/SRAL.h"
#include "Engine.h"

namespace Sral {
class VoiceOver final : public Engine {
public:
	bool Speak(const char* text, bool interrupt) override;

	bool StopSpeech() override;
	int GetNumber() override { return SRAL_ENGINE_VOICE_OVER; }
	int GetCategory() override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	bool GetActive() override;
	bool Initialize() override;
	bool Uninitialize() override;
	int GetFeatures() override { return SRAL_SUPPORTS_SPEECH; }
};
} // namespace Sral
#endif
