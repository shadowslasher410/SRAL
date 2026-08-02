#ifndef ACANNOUNCER_H_
#define ACANNOUNCER_H_

#pragma once

#include <version>
#include "Engine.h"
#include "SRAL.h"

#if defined(SRAL_WITH_ACCESSKIT)
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
#include <accesskit.h>
#endif

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <stop_token>
#include <thread>
#include <type_traits>

namespace Sral {
#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201907L
    using std::hardware_destructive_interference_size;
#else
    #if defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64) || defined(TARGET_CPU_ARM64)
        static constexpr size_t hardware_destructive_interference_size = 128;
    #else
        static constexpr size_t hardware_destructive_interference_size = 64;
    #endif
#endif

struct SpeechTask {
	std::array<char, 512> text{};
	std::atomic<size_t> sequence{0};
	bool interrupt{false};
};

class alignas(hardware_destructive_interference_size) ACAnnouncer final : public Engine {
public:
	ACAnnouncer();
	~ACAnnouncer() noexcept override;

	ACAnnouncer(const ACAnnouncer&) = delete;
	ACAnnouncer& operator=(const ACAnnouncer&) = delete;
	ACAnnouncer(ACAnnouncer&& other) noexcept = delete;
	ACAnnouncer& operator=(ACAnnouncer&& other) noexcept = delete;

	bool InitializeWithContext(void* window_handle);
	bool Speak(const char* text, bool interrupt) override;
	bool SpeakSsml(const char*, bool) override { return false; }
	void* SpeakToMemory(const char*, uint64_t*, int*, int*, int*) override { return nullptr; }
	bool SetParameter(int, const void*) override { return false; }
	bool GetParameter(int, void*) override { return false; }
	bool Braille(const char*) override { return false; }
	bool StopSpeech() override;
	bool PauseSpeech() override { return false; }
	bool ResumeSpeech() override { return false; }

	[[nodiscard]] int GetNumber() noexcept override { return SRAL_ENGINE_ACCESSKIT; }
	[[nodiscard]] int GetCategory() noexcept override { return SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER; }
	[[nodiscard]] bool IsSpeaking() override { return GetActive(); }
	[[nodiscard]] bool GetActive() override;
	bool Initialize() override;
	bool Uninitialize() override;
	[[nodiscard]] int GetFeatures() noexcept override { return SRAL_SUPPORTS_SPEECH; }
	[[nodiscard]] int GetKeyFlags() noexcept override { return HANDLE_NONE; }

	[[nodiscard]] static constexpr uint64_t GetVoiceCount() noexcept { return 0; }
	[[nodiscard]] static constexpr const char* GetVoiceName(uint64_t) noexcept { return nullptr; }
	static constexpr bool SetVoice(uint64_t) noexcept { return false; }

private:
	void BackgroundWorkerLoop(std::stop_token stop_token) noexcept;
	[[nodiscard]] bool IsScreenReaderActive() noexcept;

	static constexpr size_t RING_BUFFER_SIZE = 128;
	static constexpr size_t RING_MASK = RING_BUFFER_SIZE - 1;

	std::array<SpeechTask, RING_BUFFER_SIZE> m_ring_queue;

	alignas(hardware_destructive_interference_size) std::atomic<size_t> m_head{0};
	alignas(hardware_destructive_interference_size) std::atomic<size_t> m_tail{0};
    alignas(hardware_destructive_interference_size) std::atomic<bool> m_ring_bell{false};

	std::mutex m_init_mutex;
	std::jthread m_worker_thread;
	void* m_context_handle = nullptr;

#if defined(SRAL_WITH_ACCESSKIT)
	static void OnActionRequestCallback(struct accesskit_action_request* request, void* userdata);
	static struct accesskit_tree_update* ProvideUpdateCallback(void* userdata);
	void HandleActionRequest(struct accesskit_action_request* request) noexcept;
	[[nodiscard]] struct accesskit_tree_update* InterceptUpdatePayload() noexcept;

	std::atomic<accesskit_tree_update*> m_active_update_packet{nullptr};
	std::atomic<bool> m_use_id_b{false};
	std::atomic<void*> m_adapter{nullptr};

	static constexpr accesskit_node_id WINDOW_ID = 1;
	static constexpr accesskit_node_id ANNOUNCEMENT_ID_A = 2;
	static constexpr accesskit_node_id ANNOUNCEMENT_ID_B = 3;
#endif
};

} // namespace Sral

#endif // ACANNOUNCER_H_
