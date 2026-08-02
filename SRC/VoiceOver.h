#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <new>
#include <queue>
#include <string>
#include <thread>
#include <version>

#include "Engine.h"
#include "SRAL.h"

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

class alignas(hardware_destructive_interference_size) VoiceOver final : public Engine {
private:
	enum class CommandType : uint8_t { Speak, Stop };

	struct ThreadCommand {
		CommandType type = CommandType::Stop;
		std::string payload;
		bool interrupt = false;
	};

public:
	VoiceOver() noexcept = default;
	~VoiceOver() noexcept override;

	VoiceOver(const VoiceOver&) = delete;
	VoiceOver& operator=(const VoiceOver&) = delete;
	VoiceOver(VoiceOver&&) = delete;
	VoiceOver& operator=(VoiceOver&&) = delete;

	[[nodiscard]] bool Speak(const char* text, bool interrupt) override;
	[[nodiscard]] bool SpeakSsml(const char* ssml, bool interrupt) override { return Speak(ssml, interrupt); }
	bool Braille(const char*) override { return false; }

	[[nodiscard]] bool StopSpeech() override;
	[[nodiscard]] bool IsSpeaking() override;
	[[nodiscard]] bool PauseSpeech() override { return false; }
	[[nodiscard]] bool ResumeSpeech() override { return false; }
	[[nodiscard]] int GetNumber() noexcept override { return SRAL_ENGINE_VOICE_OVER; }
	[[nodiscard]] int GetCategory() noexcept override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	[[nodiscard]] int GetFeatures() noexcept override { return SRAL_SUPPORTS_SPEECH; }
	[[nodiscard]] int GetKeyFlags() noexcept override { return HANDLE_NONE; }
	[[nodiscard]] bool GetActive() override;
	[[nodiscard]] bool Initialize() override;
	[[nodiscard]] bool Uninitialize() override;

private:
	void BackgroundWorkerLoop(std::stop_token stop_token) noexcept;
	mutable std::mutex instanceMutex;
	std::atomic<bool> m_isSpeakingCache{false};
	alignas(hardware_destructive_interference_size) std::jthread m_workerThread;
	alignas(hardware_destructive_interference_size) std::queue<ThreadCommand> m_commandQueue;
	alignas(hardware_destructive_interference_size) std::mutex m_queueMutex;
	alignas(hardware_destructive_interference_size) std::condition_variable_any m_cv;
};

} // namespace Sral
