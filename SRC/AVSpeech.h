#ifndef AV_SPEECH_H_
#define AV_SPEECH_H_
#pragma once

#if defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>

#if TARGET_OS_OSX || TARGET_OS_IPHONE

#include <version>
#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <string_view>

#include "SRAL.h"
#include "Engine.h"

class AVSpeechSynthesizerWrapper;

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


class alignas(hardware_destructive_interference_size) AvSpeech final : public Engine {
public:
	AvSpeech() noexcept;
	~AvSpeech() noexcept override;

	AvSpeech(const AvSpeech&) = delete;
	AvSpeech& operator=(const AvSpeech&) = delete;
	AvSpeech(AvSpeech&&) noexcept = default;
	AvSpeech& operator=(AvSpeech&&) noexcept = default;

	[[nodiscard]] bool Speak(const char* text, bool interrupt) override;
	[[nodiscard]] bool SpeakSsml(const char* ssml, bool interrupt) override;
	[[nodiscard]] bool Braille(const char* text) override;

	void* SpeakToMemory(const char* text,
		uint64_t* buffer_size,
		int* channels,
		int* sample_rate,
		int* bits_per_sample) override;

	bool SetParameter(int param, const void* value) override;
	bool GetParameter(int param, void* value) override;

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
	enum class TaskType : uint8_t { Speak, Stop, Pause, Resume, SetVolume, SetRate, SetVoice };

	struct AsyncSpeechTask {
		std::array<char, 512> text{};
		std::atomic<size_t> sequence{0};
		float parameter_value{0.0f};
		TaskType type{TaskType::Speak};
		bool interrupt{false};
	};

	void BackgroundWorkerLoop(std::stop_token stop_token) noexcept;
	bool PushTask(TaskType type, std::string_view text, float param_val, bool interrupt) noexcept;

	static constexpr size_t RING_BUFFER_SIZE = 128;
	static constexpr size_t RING_MASK = RING_BUFFER_SIZE - 1;

	std::array<AsyncSpeechTask, RING_BUFFER_SIZE> m_ring_queue;
	alignas(hardware_destructive_interference_size) std::atomic<size_t> m_head{0};
	alignas(hardware_destructive_interference_size) std::atomic<size_t> m_tail{0};
	alignas(hardware_destructive_interference_size) std::atomic<bool> m_ring_bell{false};

	alignas(hardware_destructive_interference_size) std::mutex m_init_mutex;
	std::jthread m_worker_thread;
	AVSpeechSynthesizerWrapper* obj = nullptr;

	std::atomic<uint8_t> m_cached_volume{100};
	std::atomic<uint8_t> m_cached_rate{100};
	std::atomic<bool> m_initialized{false};
};

} // namespace Sral

#endif /* TARGET_OS_OSX || TARGET_OS_IPHONE */
#endif /* defined(__APPLE__) || defined(__MACH__) */
#endif /* AV_SPEECH_H_ */
