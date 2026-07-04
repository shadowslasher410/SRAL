#ifndef AV_SPEECH_H_
#define AV_SPEECH_H_
#pragma once

#if defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>

#if TARGET_OS_OSX || TARGET_OS_IPHONE

#include <cstddef>
#include <new>
#include <string>

#include "../Include/SRAL.h"
#include "Engine.h"

class AVSpeechSynthesizerWrapper;

namespace Sral {

#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201907L
inline constexpr std::size_t DestructiveInterferenceSize = std::hardware_destructive_interference_size;
#elif defined(__apple_build_version__) && (defined(__arm64__) || defined(__aarch64__))
inline constexpr std::size_t DestructiveInterferenceSize = 128;
#else
inline constexpr std::size_t DestructiveInterferenceSize = 64;
#endif

class alignas(DestructiveInterferenceSize) AvSpeech final : public Engine {
public:
	AvSpeech() noexcept = default;
	~AvSpeech() noexcept override = default;

	AvSpeech(const AvSpeech&) = delete;
	AvSpeech& operator=(const AvSpeech&) = delete;
	AvSpeech(AvSpeech&&) = delete;
	AvSpeech& operator=(AvSpeech&&) = delete;

	bool Speak(const char* const text, const bool interrupt) override;
	bool SpeakSsml(const char* const ssml, const bool interrupt) override;
	bool Braille(const char* const text) override;

	void* SpeakToMemory(const char* const text,
		uint64_t* const buffer_size,
		int* const channels,
		int* const sample_rate,
		int* const bits_per_sample) override;

	bool SetParameter(const int param, const void* const value) override;
	bool GetParameter(const int param, void* const value) override;

	bool StopSpeech() override;
	bool PauseSpeech() override;
	bool ResumeSpeech() override;
	bool IsSpeaking() override;

	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_AV_SPEECH; }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE; }

	[[nodiscard]] bool GetActive() override;
	bool Initialize() override;
	bool Uninitialize() override;
	[[nodiscard]] int GetFeatures() override;
	[[nodiscard]] int GetKeyFlags() override { return HANDLE_NONE; }

private:
	AVSpeechSynthesizerWrapper* obj = nullptr;
};

} // namespace Sral

#endif /* TARGET_OS_OSX || TARGET_OS_IPHONE */
#endif /* defined(__APPLE__) || defined(__MACH__) */
#endif /* AV_SPEECH_H_ */
