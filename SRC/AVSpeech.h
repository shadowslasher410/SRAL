#ifndef AV_SPEECH_H_
#define AV_SPEECH_H_
#pragma once

#if defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>

#if TARGET_OS_OSX || TARGET_OS_IPHONE

#include <cstddef>
#include <new>
#include <string>
#include <array>
#include <atomic>
#include <thread>
#include <mutex>

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
	AvSpeech() noexcept;
	~AvSpeech() noexcept override;
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
	enum class TaskType : uint8_t { 
		Speak, 
		Stop, 
		Pause, 
		Resume, 
		SetVolume, 
		SetRate 
	};
	
	struct alignas(DestructiveInterferenceSize) AsyncSpeechTask {
		std::array<char, 512> text{};
		std::atomic<size_t>   sequence{0};
		float                 parameter_value{0.0f};
		TaskType              type{TaskType::Speak};
		bool                  interrupt{false};
	};

	void BackgroundWorkerLoop(std::stop_token stop_token) noexcept;
	bool PushTask(TaskType type, std::string_view text, float param_val, bool interrupt) noexcept;

	static constexpr size_t RING_BUFFER_SIZE = 128;
	static constexpr size_t RING_MASK = RING_BUFFER_SIZE - 1;
	alignas(DestructiveInterferenceSize) std::array<AsyncSpeechTask, RING_BUFFER_SIZE> m_ring_queue;
	alignas(DestructiveInterferenceSize) std::atomic<size_t> m_head{0};
	alignas(DestructiveInterferenceSize) std::atomic<size_t> m_tail{0};
	alignas(DestructiveInterferenceSize) std::atomic<bool>   m_ring_bell{false};

	std::mutex                        m_init_mutex;
	std::jthread                      m_worker_thread;
	AVSpeechSynthesizerWrapper*       obj = nullptr;

	std::atomic<uint64_t>             m_cached_volume{100};
	std::atomic<uint64_t>             m_cached_rate{100};
	std::atomic<bool>                 m_initialized{false};
};

} // namespace Sral

#endif /* TARGET_OS_OSX || TARGET_OS_IPHONE */
#endif /* defined(__APPLE__) || defined(__MACH__) */
#endif /* AV_SPEECH_H_ */
