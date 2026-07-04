#ifndef ANDROID_TEXT_TO_SPEECH_H_
#define ANDROID_TEXT_TO_SPEECH_H_
#pragma once

#include <cstdint>
#include <memory>

#include "../Include/SRAL.h"
#include "Engine.h"

namespace Sral {

class alignas(hardware_destructive_interference_size) AndroidTextToSpeech final : public Engine {
public:
	AndroidTextToSpeech();
	~AndroidTextToSpeech() noexcept override;
	AndroidTextToSpeech(const AndroidTextToSpeech&) = delete;
	AndroidTextToSpeech& operator=(const AndroidTextToSpeech&) = delete;
	AndroidTextToSpeech(AndroidTextToSpeech&&) = delete;
	AndroidTextToSpeech& operator=(AndroidTextToSpeech&&) = delete;

	bool Speak(const char* text, bool interrupt) override;
	bool SpeakSsml(const char* ssml, bool interrupt) override;

	void* SpeakToMemory(
		const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample) override {
		return nullptr;
	}

	bool SetParameter(int param, const void* value) override;
	bool GetParameter(int param, void* value) override;

	bool Braille(const char* text) override;
	bool StopSpeech() override;
	bool PauseSpeech() override;
	bool ResumeSpeech() override;
	bool IsSpeaking() override;

	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_ANDROID_TEXT_TO_SPEECH; }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE; }
	bool GetActive() override;
	bool Initialize() override;
	bool Uninitialize() override;

	[[nodiscard]] int GetFeatures() override;
	[[nodiscard]] int GetKeyFlags() override { return HANDLE_NONE; }

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace Sral
#endif /* ANDROID_TEXT_TO_SPEECH_H_ */
