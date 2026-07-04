#ifndef ACANNOUNCER_H_
#define ACANNOUNCER_H_
#pragma once

#include "../Include/SRAL.h"
#include "Engine.h"

struct accesskit_windows_adapter;
struct accesskit_tree_update;
struct accesskit_action_request;

#include <accesskit.h>
#include <mutex>
#include <queue>
#include <semaphore>
#include <string>
#include <thread>

class ACAnnouncer final : public Engine {
public:
	ACAnnouncer();
	~ACAnnouncer() override;

	ACAnnouncer(const ACAnnouncer&) = delete;
	ACAnnouncer& operator=(const ACAnnouncer&) = delete;
	ACAnnouncer(ACAnnouncer&& other) noexcept;
	ACAnnouncer& operator=(ACAnnouncer&& other) noexcept;

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

	[[nodiscard]] int GetNumber() override { return ENGINE_AC_ANNOUNCER; }
	[[nodiscard]] bool GetActive() override;
	bool Initialize() override;
	bool Uninitialize() override;
	[[nodiscard]] int GetFeatures() override { return SUPPORTS_SPEECH; }
	[[nodiscard]] uint64_t GetVoiceCount() override { return 0; }
	[[nodiscard]] const char* GetVoiceName(uint64_t index) override { return nullptr; }
	bool SetVoice(uint64_t index) override { return false; }
	[[nodiscard]] int GetKeyFlags() override { return HANDLE_NONE; }

private:
	struct SpeechTask {
		std::string text;
		bool interrupt;
	};

	void HandleActionRequest(struct accesskit_action_request* request) noexcept;
	[[nodiscard]] struct accesskit_tree_update* InterceptUpdatePayload() noexcept;
	static void OnActionRequestCallback(struct accesskit_action_request* request, void* userdata);
	static struct accesskit_tree_update* ProvideUpdateCallback(void* userdata);

	void BackgroundWorkerLoop(std::stop_token stop_token);
	[[nodiscard]] bool IsScreenReaderActive() noexcept;
	std::mutex m_mutex;
	std::counting_semaphore<0> m_semaphore{0};
	std::queue<SpeechTask> m_queue;
	accesskit_windows_adapter* m_adapter = nullptr;
	std::jthread m_worker_thread;
	HWND m_bound_window = nullptr;
	accesskit_tree_update* m_active_update_packet = nullptr;

	bool m_use_id_b = false;

	static constexpr accesskit_node_id WINDOW_ID = 1;
	static constexpr accesskit_node_id ANNOUNCEMENT_ID_A = 2;
	static constexpr accesskit_node_id ANNOUNCEMENT_ID_B = 3;
};
#endif
