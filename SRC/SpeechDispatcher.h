#ifndef SPEECHDISPATCHER_H_
#define SPEECHDISPATCHER_H_
#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <new>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "../Include/SRAL.h"
#include "Engine.h"

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_destructive_interference_size;
#else
constexpr size_t hardware_destructive_interference_size = 64;
#endif

#if defined(__linux__) && !defined(__ANDROID__)
#include <speech-dispatcher/libspeechd.h>
#else
struct SPDConnection;
struct SPDVoice;
using SPDNotificationType = int;
#endif

namespace Sral {

class alignas(hardware_destructive_interference_size) SpeechDispatcher final : public Engine {
public:
	SpeechDispatcher() noexcept = default;
	~SpeechDispatcher() override;

	SpeechDispatcher(const SpeechDispatcher&) = delete;
	SpeechDispatcher& operator=(const SpeechDispatcher&) = delete;
	SpeechDispatcher(SpeechDispatcher&&) noexcept = delete;
	SpeechDispatcher& operator=(SpeechDispatcher&&) noexcept = delete;

	bool Speak(const char* text, bool interrupt) override;
	bool SpeakSsml(const char* ssml, bool interrupt) override;
	bool Braille(const char* text) override;

	bool IsSpeaking() noexcept override;
	bool GetActive() noexcept override;

	bool SetParameter(int param, const void* value) override;
	bool GetParameter(int param, void* value) override;

	bool StopSpeech() noexcept override;
	bool PauseSpeech() noexcept override;
	bool ResumeSpeech() noexcept override;

	int GetNumber() override { return SRAL_ENGINE_SPEECH_DISPATCHER; }
	int GetCategory() override { return SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE; }

	bool Initialize() override;
	bool Uninitialize() override;

	int GetFeatures() override {
		return SRAL_SUPPORTS_SPEECH | SRAL_SUPPORTS_BRAILLE | SRAL_SUPPORTS_SPEECH_RATE | SRAL_SUPPORTS_SPEECH_VOLUME |
			SRAL_SUPPORTS_PAUSE_SPEECH | SRAL_SUPPORTS_SPELLING | SRAL_SUPPORTS_SSML | SRAL_SUPPORTS_SELECT_VOICE;
	}

	int GetKeyFlags() override { return HANDLE_NONE; }

	static void SpeechNotificationCallback(size_t msg_id, size_t client_id, SPDNotificationType type) noexcept;

private:
	enum class TaskType : uint8_t { Speak, SpeakSsml, Stop, SetParam };

	struct AsyncSpdTask {
		const char* text_ptr;
		std::atomic<size_t> sequence;
		TaskType type;
		int param_id;
		int param_val;
		bool interrupt;
	};

	void BackgroundWorkerLoop(std::stop_token stop_token) noexcept;

	static constexpr size_t RING_BUFFER_SIZE = 128;
	static constexpr size_t RING_MASK = RING_BUFFER_SIZE - 1;

	alignas(hardware_destructive_interference_size) std::array<AsyncSpdTask, RING_BUFFER_SIZE> m_ring_queue;
	alignas(hardware_destructive_interference_size) std::atomic<size_t> m_head{0};
	alignas(hardware_destructive_interference_size) std::atomic<size_t> m_tail{0};
	std::atomic<bool> m_ring_bell{false};
	static std::atomic<bool> is_active;
	static std::mutex speechd_mutex;
	static std::atomic<size_t> m_activeMsgId;

	SPDConnection* speech = nullptr;
	SPDVoice** m_voiceList = nullptr;
	bool enableSpelling = false;
	bool brailleInitialized = false;
	int m_voiceCount = 0;
	int m_voiceIndex = 0;
	int m_speechRate = 0;
	int m_speechVolume = 0;
	std::atomic<bool> m_isSpeakingLocal{false};

	std::mutex m_mutex;
	std::jthread m_worker_thread;

	int SetVoiceIndex() noexcept;

	std::vector<std::unique_ptr<char[]>> m_voice_strings;
	std::mutex m_string_pool_mutex;
	std::vector<std::unique_ptr<char[]>> m_string_pool;
	const char* AddString(const char* text);
	void ClearStringPool() noexcept;
	void ClearVoiceList() noexcept;
	void RefreshVoiceList();
};

} // namespace Sral

#endif // SPEECHDISPATCHER_H_
