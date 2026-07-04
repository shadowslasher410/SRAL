#ifndef ANDROIDACCESSIBILITYMANAGER_H_
#define ANDROIDACCESSIBILITYMANAGER_H_
#pragma once

#if defined(__ANDROID__)
#include <jni.h>
#include <mutex>
#include <queue>
#include <semaphore>
#include <string>
#include <thread>

#include "Engine.h"

namespace Sral {

class AndroidAccessibilityManager final : public Engine {
public:
	AndroidAccessibilityManager();
	~AndroidAccessibilityManager() override;

	AndroidAccessibilityManager(const AndroidAccessibilityManager&) = delete;
	AndroidAccessibilityManager& operator=(const AndroidAccessibilityManager&) = delete;
	AndroidAccessibilityManager(AndroidAccessibilityManager&&) = delete;
	AndroidAccessibilityManager& operator=(AndroidAccessibilityManager&&) = delete;

	bool Speak(const char* text, bool interrupt) override;
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
	[[nodiscard]] int GetFeatures() override { return SUPPORTS_SPEECH; }
	[[nodiscard]] uint64_t GetVoiceCount() override { return 0; }
	[[nodiscard]] const char* GetVoiceName(uint64_t index) override { return nullptr; }
	bool SetVoice(uint64_t index) override { return false; }
	[[nodiscard]] int GetKeyFlags() override { return HANDLE_NONE; }

private:
	enum class TaskType { Speak, Stop };

	struct AsyncSpeechTask {
		TaskType type;
		std::string text;
		bool interrupt;
	};

	void BackgroundWorkerLoop(std::stop_token stop_token) noexcept;
	void UninitializeInternal(JNIEnv* localEnv) noexcept;

	std::mutex m_mutex;
	std::counting_semaphore<0> m_semaphore{0};
	std::queue<AsyncSpeechTask> m_queue;
	std::jthread m_worker_thread;

	JavaVM* m_jvm = nullptr;
	jclass m_announcerClass = nullptr;
	jobject m_announcerObj = nullptr;

	jmethodID m_constructor = nullptr;
	jmethodID m_midIsActive = nullptr;
	jmethodID m_midAnnounce = nullptr;
	jmethodID m_midStop = nullptr;
	jmethodID m_midShutdown = nullptr;

	bool m_initialized = false;
};

} // namespace Sral
#endif
#endif
