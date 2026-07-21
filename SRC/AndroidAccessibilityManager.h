#ifndef ANDROIDACCESSIBILITYMANAGER_H_
#define ANDROIDACCESSIBILITYMANAGER_H_
#pragma once

#include <memory>

#include "Engine.h"
#include "SRAL.h"

namespace Sral {

inline constexpr size_t CACHE_LINE_SIZE = 64;

class alignas(CACHE_LINE_SIZE) AndroidAccessibilityManager final : public Engine {
public:
	AndroidAccessibilityManager();
	~AndroidAccessibilityManager() override;

	AndroidAccessibilityManager(const AndroidAccessibilityManager&) = delete;
	AndroidAccessibilityManager& operator=(const AndroidAccessibilityManager&) = delete;
	AndroidAccessibilityManager(AndroidAccessibilityManager&&) noexcept = delete;
	AndroidAccessibilityManager& operator=(AndroidAccessibilityManager&&) noexcept = delete;

	bool Speak(const char*, bool) override;
	bool SpeakSsml(const char*, bool) override { return false; }
	void* SpeakToMemory(const char*, uint64_t*, int*, int*, int*) override { return nullptr; }
	bool SetParameter(int, const void*) override { return false; }
	bool GetParameter(int, void*) override { return false; }
	bool Braille(const char*) override { return false; }
	bool StopSpeech() override;
	bool PauseSpeech() override { return false; }
	bool ResumeSpeech() override { return false; }

	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_ANDROID_ACCESSIBILITY_MANAGER; }
	[[nodiscard]] bool GetActive() override;
	bool Initialize() override;
	bool Uninitialize() override;
	[[nodiscard]] int GetFeatures() override { return SRAL_SUPPORTS_SPEECH; }
	[[nodiscard]] int GetKeyFlags() override { return HANDLE_NONE; }
	[[nodiscard]] uint64_t GetVoiceCount() { return 0; }
	[[nodiscard]] const char* GetVoiceName(uint64_t) { return nullptr; }
	bool SetVoice(uint64_t) { return false; }
	[[nodiscard]] bool IsSpeaking() override { return GetActive(); }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER; }

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace Sral

#endif // ANDROIDACCESSIBILITYMANAGER_H_