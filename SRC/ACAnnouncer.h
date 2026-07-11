#ifndef ACANNOUNCER_H_
#define ACANNOUNCER_H_
#pragma once

#include "Engine.h"
#include "../Include/SRAL.h"

#if defined(_WIN32)
struct accesskit_windows_adapter;
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
struct accesskit_ios_adapter;
#else
struct accesskit_macos_adapter;
#endif
#elif defined(__ANDROID__)
struct accesskit_android_adapter;
#else 
struct accesskit_unix_adapter;
#endif

struct accesskit_tree_update;
struct accesskit_action_request;

#include "Dep/accesskit.h"
#include <mutex>
#include <thread>
#include <atomic>
#include <array>

namespace Sral {

class alignas(destructive_alignment) ACAnnouncer final : public Engine {
public:
	ACAnnouncer();
	~ACAnnouncer() override;

	ACAnnouncer(const ACAnnouncer&) = delete;
	ACAnnouncer& operator=(const ACAnnouncer&) = delete;
	ACAnnouncer(ACAnnouncer&& other) noexcept;
	ACAnnouncer& operator=(ACAnnouncer&& other) noexcept;

	bool InitializeWithContext(void* platform_window_or_context);

	bool Speak(const char* speech_text, bool interrupt) override;
	bool SpeakSsml(const char* ssml, bool interrupt) override { return false; }
	void* SpeakToMemory(const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample) override { return nullptr; }

	bool SetParameter(int param, const void* value) override { return false; }
	bool GetParameter(int param, void* value) override { return false; }

	bool Braille(const char* text) override { return false; }
	bool StopSpeech() override;
	bool PauseSpeech() override { return false; }
	bool ResumeSpeech() override { return false; }

	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_ACCESSKIT; }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER; }
	[[nodiscard]] bool IsSpeaking() override { return GetActive(); }

	[[nodiscard]] bool GetActive() override;
	bool Initialize() override;
	bool Uninitialize() override;
	[[nodiscard]] int GetFeatures() override { return SRAL_SUPPORTS_SPEECH; }
	[[nodiscard]] int GetKeyFlags() override { return HANDLE_NONE; }
	
	[[nodiscard]] static uint64_t GetVoiceCount() { return 0; }
	[[nodiscard]] static const char* GetVoiceName(uint64_t index) { return nullptr; }
	static bool SetVoice(uint64_t index) { return false; }

private:
	struct alignas(destructive_alignment) SpeechTask {
		std::array<char, 512> text{};
		std::atomic<size_t>  sequence{0};
		bool                 interrupt{false};
		SpeechTask() noexcept = default;
	};

	void HandleActionRequest(struct accesskit_action_request* request) noexcept;
	
	[[nodiscard]] struct accesskit_tree_update* InterceptUpdatePayload() noexcept;
	static void OnActionRequestCallback(struct accesskit_action_request* request, void* userdata);
	static struct accesskit_tree_update* ProvideUpdateCallback(void* userdata);

	void BackgroundWorkerLoop(std::stop_token stop_token);
	[[nodiscard]] bool IsScreenReaderActive() noexcept;
	
	static constexpr size_t RING_BUFFER_SIZE = 128;
	static constexpr size_t RING_MASK = RING_BUFFER_SIZE - 1;
	alignas(destructive_alignment) std::array<SpeechTask, RING_BUFFER_SIZE> m_ring_queue;

	alignas(destructive_alignment) std::atomic<size_t> m_head{0};
	alignas(destructive_alignment) std::atomic<size_t> m_tail{0};
	alignas(destructive_alignment) std::atomic<bool> m_ring_bell{false};

	std::mutex m_init_mutex;
	std::jthread m_worker_thread;
	
	std::atomic<accesskit_tree_update*> m_active_update_packet{nullptr};
	std::atomic<bool> m_use_id_b{false};

	std::atomic<void*> m_adapter{nullptr};
	void* m_context_handle = nullptr;

	static constexpr accesskit_node_id WINDOW_ID = 1;
	static constexpr accesskit_node_id ANNOUNCEMENT_ID_A = 2;
	static constexpr accesskit_node_id ANNOUNCEMENT_ID_B = 3;
};
}
#endif // ACANNOUNCER_H_
