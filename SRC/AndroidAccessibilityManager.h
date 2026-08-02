#ifndef ANDROIDACCESSIBILITYMANAGER_H_
#define ANDROIDACCESSIBILITYMANAGER_H_
#pragma once

#include "Engine.h"
#include "SRAL.h"
#include <memory>
#include <cstdint>

namespace Sral {

/**
 * @class AndroidAccessibilityManager
 * @brief Concrete Android framework driver routing text notifications to the OS AccessibilityManager via JNI bridges.
 */
class alignas(destructive_alignment) AndroidAccessibilityManager final : public Engine {
public:
	AndroidAccessibilityManager();
	~AndroidAccessibilityManager() override;

	AndroidAccessibilityManager(const AndroidAccessibilityManager&) = delete;
	AndroidAccessibilityManager& operator=(const AndroidAccessibilityManager&) = delete;
	AndroidAccessibilityManager(AndroidAccessibilityManager&&) noexcept = default;
	AndroidAccessibilityManager& operator=(AndroidAccessibilityManager&&) noexcept = default;

	[[nodiscard]] bool Speak(const char* text, bool interrupt) override;
	[[nodiscard]] bool SpeakSsml(const char* ssml, bool interrupt) override { return Engine::SpeakSsml(ssml, interrupt); }
	void* SpeakToMemory(const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample) override {
		return Engine::SpeakToMemory(text, buffer_size, channels, sample_rate, bits_per_sample);
	}
	[[nodiscard]] bool Braille(const char* text) override { return Engine::Braille(text); }

	[[nodiscard]] bool StopSpeech() override;
	bool PauseSpeech() override { return Engine::PauseSpeech(); }
	bool ResumeSpeech() override { return Engine::ResumeSpeech(); }

	bool Initialize() override;
	bool Uninitialize() override;

	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_ANDROID_ACCESSIBILITY_MANAGER; }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER; }
	[[nodiscard]] bool GetActive() override;
	[[nodiscard]] int GetFeatures() override { return SRAL_SUPPORTS_SPEECH; }
	[[nodiscard]] int GetKeyFlags() override { return Engine::GetKeyFlags(); }
	[[nodiscard]] bool IsSpeaking() override { return GetActive(); }

	bool SetParameter(int param, const void* value) override { return Engine::SetParameter(param, value); }
	bool GetParameter(int param, void* value) override { return Engine::GetParameter(param, value); }

	[[nodiscard]] static constexpr uint64_t GetVoiceCount() noexcept { return 0; }
	[[nodiscard]] static constexpr const char* GetVoiceName(uint64_t) noexcept { return nullptr; }
	static constexpr bool SetVoice(uint64_t) noexcept { return false; }

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace Sral

#endif // ANDROIDACCESSIBILITYMANAGER_H_
