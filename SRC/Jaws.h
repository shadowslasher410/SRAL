#ifndef JAWS_H_
#define JAWS_H_
#pragma once

#include <windows.h>

#include <comdef.h>

#include <array>
#include <atomic>
#include <mutex>
#include <new>
#include <stop_token>
#include <string_view>
#include <thread>

#include "Engine.h"
#include "SRAL.h"
#include "fsapi.h"

_COM_SMARTPTR_TYPEDEF(IJawsApi, __uuidof(IJawsApi));

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

class alignas(hardware_destructive_interference_size) Jaws final : public Engine {
private:
	enum class CommandType : uint8_t { None, Speak, Braille, Stop };

	struct alignas(hardware_destructive_interference_size) ThreadCommand {
		union {
			std::array<char, 512> char_payload;
			std::array<wchar_t, 256> wchar_payload;
		} data{};
		std::atomic<size_t> sequence{0};
		size_t payload_length{0};
		CommandType type{CommandType::Stop};
		bool interrupt{false};

		ThreadCommand() noexcept = default;
	};

public:
	Jaws() noexcept = default;
	~Jaws() noexcept override;

	Jaws(const Jaws&) = delete;
	Jaws& operator=(const Jaws&) = delete;
	Jaws(Jaws&&) = delete;
	Jaws& operator=(Jaws&&) = delete;

	[[nodiscard]] bool Speak(const char* text, bool interrupt) noexcept override;
	[[nodiscard]] bool SpeakSsml(const char* ssml, bool interrupt) noexcept override { return Speak(ssml, interrupt); }
	bool Braille(const char* text) noexcept override;
	[[nodiscard]] bool StopSpeech() noexcept override;
	[[nodiscard]] bool IsSpeaking() noexcept override;
	[[nodiscard]] bool PauseSpeech() noexcept override { return false; }
	[[nodiscard]] bool ResumeSpeech() noexcept override { return false; }

	[[nodiscard]] constexpr int GetNumber() noexcept override { return SRAL_ENGINE_JAWS; }
	[[nodiscard]] constexpr int GetCategory() noexcept override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	[[nodiscard]] constexpr int GetFeatures() noexcept override { return SRAL_SUPPORTS_SPEECH | SRAL_SUPPORTS_BRAILLE; }
	[[nodiscard]] constexpr int GetKeyFlags() noexcept override { return HANDLE_NONE; }
	[[nodiscard]] bool GetActive() noexcept override;
	bool Initialize() noexcept override;
	bool Uninitialize() noexcept override;

private:
	void BackgroundWorkerLoop(std::stop_token stop_token) noexcept;

	void* pAutomation{nullptr};
	void* pCondition{nullptr};
	void* pElement{nullptr};
	void* pProvider{nullptr};
	mutable std::mutex instanceMutex;
	std::atomic<bool> isInitialized{false};

	static constexpr size_t RING_BUFFER_SIZE = 128;
	static constexpr size_t RING_MASK = RING_BUFFER_SIZE - 1;

	alignas(hardware_destructive_interference_size) std::array<ThreadCommand, RING_BUFFER_SIZE> m_ring_queue{};

	alignas(hardware_destructive_interference_size) std::atomic<size_t> m_head{0};
	alignas(hardware_destructive_interference_size) std::atomic<size_t> m_tail{0};
	alignas(hardware_destructive_interference_size) std::atomic<bool> m_ring_bell{false};
	alignas(hardware_destructive_interference_size) std::atomic<bool> m_isSpeakingCache{false};
	alignas(hardware_destructive_interference_size) std::atomic<bool> m_fastPathInterrupt{false};
	alignas(hardware_destructive_interference_size) std::jthread m_workerThread;
};

} // namespace Sral

#endif // JAWS_H_