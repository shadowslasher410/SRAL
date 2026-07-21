#ifndef ANDROIDTEXTTOSPEECH_H_
#define ANDROIDTEXTTOSPEECH_H_
#pragma once

#include <cstdint>
#include <memory>

#include "SRAL.h"
#include "Engine.h"

namespace Sral {

class alignas(destructive_alignment) AndroidTextToSpeech final : public Engine {
public:
	AndroidTextToSpeech();
	~AndroidTextToSpeech() override;
	AndroidTextToSpeech(const AndroidTextToSpeech&) = delete;
	AndroidTextToSpeech& operator=(const AndroidTextToSpeech&) = delete;
	AndroidTextToSpeech(AndroidTextToSpeech&&) noexcept = delete;
	AndroidTextToSpeech& operator=(AndroidTextToSpeech&&) noexcept = delete;

	bool Speak(const char*, bool) override;
	bool SpeakSsml(const char*, bool) override;
	void* SpeakToMemory(const char* text, uint64_t*, int*, int*, int*) override;
	bool SetParameter(int, const void*) override;
	bool GetParameter(int, void*) override;
	bool Braille(const char* text) override;
	bool StopSpeech() override;
	bool PauseSpeech() override;
	bool ResumeSpeech() override;
	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_ANDROID_TEXT_TO_SPEECH; }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE; }
	[[nodiscard]] bool GetActive() override;
	bool Initialize() override;
	bool Uninitialize() override;
	[[nodiscard]] int GetFeatures() override;
	[[nodiscard]] bool IsSpeaking() override;
	[[nodiscard]] int GetKeyFlags() override { return HANDLE_NONE; }

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace Sral

#endif // ANDROIDTEXTTOSPEECH_H_
