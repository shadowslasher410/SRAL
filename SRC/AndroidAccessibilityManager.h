#ifndef ANDROIDACCESSIBILITYMANAGER_H_
#define ANDROIDACCESSIBILITYMANAGER_H_
#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <thread>

#include "../Dep/AndroidContext.h"
#include "../Include/SRAL.h"
#include "Engine.h"

namespace Sral {

class alignas(destructive_alignment) AndroidAccessibilityManager final : public Engine {
public:
	AndroidAccessibilityManager();
	~AndroidAccessibilityManager() override;
	AndroidAccessibilityManager(const AndroidAccessibilityManager&) = delete;
	AndroidAccessibilityManager& operator=(const AndroidAccessibilityManager&) = delete;
	AndroidAccessibilityManager(AndroidAccessibilityManager&&) noexcept = delete;
	AndroidAccessibilityManager& operator=(AndroidAccessibilityManager&&) noexcept = delete;
	bool Speak(const char* speech_text, bool interrupt) override;
	bool SpeakSsml(const char* ssml, bool interrupt) override { return false; }
	void* SpeakToMemory(
		const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample) override {
		return nullptr;
	}
	bool SetParameter(int param, const void* value) override { return false; }
	bool GetParameter(int param, void* value) override { return false; }
	bool Braille(const char* text) override { return false; }
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
	[[nodiscard]] const char* GetVoiceName(uint64_t index) { return nullptr; }
	bool SetVoice(uint64_t index) { return false; }
	[[nodiscard]] bool IsSpeaking() override { return GetActive(); }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER; }

private:
	enum class TaskType : uint8_t { Speak, Stop };

	struct alignas(destructive_alignment) AsyncSpeechTask {
		std::array<char, 512> text;
		std::atomic<size_t> sequence;
		TaskType type;
		bool interrupt;
	};
	void BackgroundWorkerLoop(std::stop_token stop_token) noexcept;
	void UninitializeInternal(JNIEnv* localEnv) noexcept;
	[[nodiscard]] bool IsScreenReaderActive() noexcept;
	static constexpr size_t RING_BUFFER_SIZE = 128;
	static constexpr size_t RING_MASK = RING_BUFFER_SIZE - 1;
	alignas(destructive_alignment) std::array<AsyncSpeechTask, RING_BUFFER_SIZE> m_ring_queue;
	alignas(destructive_alignment) std::atomic<size_t> m_head{0};
	alignas(destructive_alignment) std::atomic<size_t> m_tail{0};
	alignas(destructive_alignment) std::atomic<bool> m_ring_bell{false};
	std::mutex m_init_mutex;
	std::jthread m_worker_thread;
	JavaVM* m_jvm = nullptr;
	jclass m_announcerClass = nullptr;
	jobject m_announcerObj = nullptr;
	void* m_context_handle = nullptr;
	jmethodID m_constructor = nullptr;
	jmethodID m_midIsActive = nullptr;
	jmethodID m_midAnnounce = nullptr;
	jmethodID m_midStop = nullptr;
	jmethodID m_midShutdown = nullptr;
	bool m_initialized = false;
};

} // namespace Sral

#endif // ANDROIDACCESSIBILITYMANAGER_H_
