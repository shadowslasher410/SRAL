#ifndef UIA_H_
#define UIA_H_
#pragma once

#include <version>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <new>

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

class alignas(hardware_destructive_interference_size) Uia final : public Engine {
private:
	enum class CommandType : uint8_t { Speak, Stop };

	struct ThreadCommand {
		std::array<char, 512> payload{};
		std::atomic<size_t> sequence{0};
		CommandType type{CommandType::Stop};
		bool interrupt{false};

		ThreadCommand() noexcept = default;
	};

public:
	Uia() noexcept : m_ring_queue{} {}
	~Uia() noexcept override; 

	Uia(const Uia&) = delete;
	Uia& operator=(const Uia&) = delete;
	Uia(Uia&&) = delete;
	Uia& operator=(Uia&&) = delete;

	[[nodiscard]] bool Speak(const char* text, bool interrupt) override;
	[[nodiscard]] bool SpeakSsml(const char* ssml, bool interrupt) override { return Speak(ssml, interrupt); }
	bool Braille(const char*) override { return false; }
	[[nodiscard]] bool StopSpeech() override;
	[[nodiscard]] bool IsSpeaking() override;
	[[nodiscard]] bool PauseSpeech() override { return false; }
	[[nodiscard]] bool ResumeSpeech() override { return false; }
    
	[[nodiscard]] int GetNumber() noexcept override { return SRAL_ENGINE_UIA; }
	[[nodiscard]] int GetCategory() noexcept override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	[[nodiscard]] int GetFeatures() noexcept override { return SRAL_SUPPORTS_SPEECH; }
	[[nodiscard]] int GetKeyFlags() noexcept override { return HANDLE_NONE; }
	[[nodiscard]] bool GetActive() override;
	bool Initialize() override;
	bool Uninitialize() override;

private:
	void BackgroundWorkerLoop(std::stop_token stop_token) noexcept;
	void CleanUpMembers() noexcept;

	void* pAutomation{nullptr};
	void* pCondition{nullptr};
	void* pElement{nullptr};
	void* pProvider{nullptr};

	mutable std::mutex instanceMutex;
	std::atomic<bool> isInitialized{false};
	std::atomic<bool> m_isSpeakingCache{false};

	static constexpr size_t RING_BUFFER_SIZE = 128;
	static constexpr size_t RING_MASK = RING_BUFFER_SIZE - 1;

	std::array<ThreadCommand, RING_BUFFER_SIZE> m_ring_queue;

	alignas(hardware_destructive_interference_size) std::atomic<size_t> m_head{0};
	alignas(hardware_destructive_interference_size) std::atomic<size_t> m_tail{0};
	alignas(hardware_destructive_interference_size) std::atomic<bool> m_ring_bell{false};
	alignas(hardware_destructive_interference_size) std::jthread m_workerThread;
};

} // namespace Sral

#endif // UIA_H_