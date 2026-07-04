#ifndef CHROMEVOX_H_
#define CHROMEVOX_H_
#pragma once

#include <atomic>
#include <mutex>
#include <string_view>

#include "../Include/SRAL.h"
#include "Engine.h"

namespace Sral {

class alignas(hardware_destructive_interference_size) ChromeVox final : public Engine {
public:
	ChromeVox();
	~ChromeVox() noexcept override;

	ChromeVox(const ChromeVox&) = delete;
	ChromeVox& operator=(const ChromeVox&) = delete;
	ChromeVox(ChromeVox&&) = delete;
	ChromeVox& operator=(ChromeVox&&) = delete;

	bool Speak(const char* text, bool interrupt) override;
	bool SpeakSsml(const char* ssml, bool interrupt) override;
	bool Braille(const char* text) override;

	void* SpeakToMemory(
		const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample) override {
		return nullptr;
	}

	bool SetParameter(int param, const void* value) override;
	bool GetParameter(int param, void* value) override;

	bool StopSpeech() override;
	bool PauseSpeech() override;
	bool ResumeSpeech() override;
	bool IsSpeaking() override;

	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_CHROMEVOX; }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	[[nodiscard]] bool GetActive() override;
	bool Initialize() override;
	bool Uninitialize() override;
	[[nodiscard]] int GetFeatures() override;
	[[nodiscard]] int GetKeyFlags() override { return HANDLE_NONE; }

private:
	static std::atomic<int> _mode;
	static std::atomic<bool> is_active;
	static std::mutex chromevox_mutex;
};

} // namespace Sral
#endif /* CHROMEVOX_H_ */
