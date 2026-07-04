#ifndef NVDA_H_
#define NVDA_H_
#ifdef _WIN32
#pragma once

#include <windows.h>
#include <mutex>
#include <new>

#include "../Include/SRAL.h"
#include "Engine.h"

namespace Sral {

typedef error_status_t(__stdcall* NVDAController_speakText)(const wchar_t*);
typedef error_status_t(__stdcall* NVDAController_brailleMessage)(const wchar_t*);
typedef error_status_t(__stdcall* NVDAController_cancelSpeech)(void);
typedef error_status_t(__stdcall* NVDAController_testIfRunning)(void);
typedef error_status_t(__stdcall* NVDAController_speakSsml)(const wchar_t*, int, int, int);

class alignas(destructive_alignment) Nvda final : public Engine {
public:
	Nvda();
	~Nvda() override;

	Nvda(const Nvda&) = delete;
	Nvda& operator=(const Nvda&) = delete;
	Nvda(Nvda&&) = delete;
	Nvda& operator=(Nvda&&) = delete;

	[[nodiscard]] bool Speak(const char* text, bool interrupt) override;
	[[nodiscard]] bool SpeakSsml(const char* ssml, bool interrupt) override;
	
	void* SpeakToMemory(
		const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample) override {
		(void)text;
		(void)buffer_size;
		(void)channels;
		(void)sample_rate;
		(void)bits_per_sample;
		return nullptr;
	}

	[[nodiscard]] bool SetParameter(int param, const void* value) override;
	[[nodiscard]] bool GetParameter(int param, void* value) override;

	[[nodiscard]] bool Braille(const char* text) override;
	[[nodiscard]] bool StopSpeech() override;
	[[nodiscard]] bool PauseSpeech() override;
	[[nodiscard]] bool ResumeSpeech() override;

	[[nodiscard]] bool IsSpeaking() override; 

	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_NVDA; }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	
	[[nodiscard]] bool GetActive() override;
	[[nodiscard]] int GetFeatures() override {
		return SRAL_SUPPORTS_SPEECH | SRAL_SUPPORTS_BRAILLE | SRAL_SUPPORTS_PAUSE_SPEECH | SRAL_SUPPORTS_SPELLING;
	}

	[[nodiscard]] bool Initialize() override;
	[[nodiscard]] bool Uninitialize() override;

private:
	bool InitializeInternal() noexcept;
	void UninitializeInternal() noexcept;

	alignas(destructive_alignment) std::mutex m_mutex;
	alignas(destructive_alignment) HINSTANCE lib = nullptr;

	NVDAController_speakText nvdaController_speakText = nullptr;
	NVDAController_brailleMessage nvdaController_brailleMessage = nullptr;
	NVDAController_cancelSpeech nvdaController_cancelSpeech = nullptr;
	NVDAController_testIfRunning nvdaController_testIfRunning = nullptr;
	NVDAController_speakSsml nvdaController_speakSsml = nullptr;

	int symbolLevel = -1;
	bool extended = false;
	bool enable_spelling = false;
	bool use_character_descriptions = false;

	alignas(destructive_alignment) ULONGLONG m_lastConnCheckTime = 0;
};
} // namespace Sral
#endif
#endif
