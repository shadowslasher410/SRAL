#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <new>
#include <queue>
#include <string>
#include <thread>
#include <type_traits>
#include <version>

#include "Engine.h"
#include "SRAL.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
using HMODULE = void*;
using FARPROC = void (*)();
#define WINAPI
#define TRUE 1
#define FALSE 0
using BOOL = int;
#endif

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

class alignas(hardware_destructive_interference_size) Zdsr final : public Engine {
private:
	struct LibraryDeleter {
		using pointer = HMODULE;
		void operator()(HMODULE handle) const noexcept;
	};
	using UniqueLibraryHandle = std::unique_ptr<std::remove_pointer_t<HMODULE>, LibraryDeleter>;

	enum class CommandType : uint8_t { Speak, Stop };
	struct ThreadCommand {
		CommandType type = CommandType::Stop;
		std::string payload;
		bool interrupt = false;
	};

public:
	Zdsr() noexcept = default;
	~Zdsr() noexcept override;

	Zdsr(const Zdsr&) = delete;
	Zdsr& operator=(const Zdsr&) = delete;
	Zdsr(Zdsr&&) = delete;
	Zdsr& operator=(Zdsr&&) = delete;

	[[nodiscard]] bool Speak(const char* text, bool interrupt) override;
	[[nodiscard]] bool SpeakSsml(const char* ssml, bool interrupt) override { return Speak(ssml, interrupt); }
	bool Braille(const char*) override { return false; }

	[[nodiscard]] bool StopSpeech() override;
	[[nodiscard]] bool IsSpeaking() override;
	[[nodiscard]] bool PauseSpeech() override { return false; }
	[[nodiscard]] bool ResumeSpeech() override { return false; }

	[[nodiscard]] int GetNumber() noexcept override { return SRAL_ENGINE_ZDSR; }
	[[nodiscard]] int GetCategory() noexcept override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	[[nodiscard]] int GetFeatures() noexcept override { return SRAL_SUPPORTS_SPEECH; }
	[[nodiscard]] int GetKeyFlags() noexcept override { return HANDLE_NONE; }
	[[nodiscard]] bool GetActive() override;

	[[nodiscard]] bool Initialize() override;
	[[nodiscard]] bool Uninitialize() override;

private:
	void CleanUpMembers() noexcept;
	void BackgroundWorkerLoop(std::stop_token stop_token) noexcept;

	UniqueLibraryHandle lib{nullptr};

	using InitTTS_t = int(WINAPI*)(int, const wchar_t*);
	using Speak_t = int(WINAPI*)(const wchar_t*, BOOL);
	using GetSpeakState_t = int(WINAPI*)();
	using StopSpeak_t = int(WINAPI*)();

	InitTTS_t fInitTTS{nullptr};
	Speak_t fSpeak{nullptr};
	GetSpeakState_t fGetSpeakState{nullptr};
	StopSpeak_t fStopSpeak{nullptr};

	mutable std::mutex instanceMutex;
	alignas(hardware_destructive_interference_size) std::atomic<bool> isInitialized{false};
	alignas(hardware_destructive_interference_size) std::atomic<bool> m_isSpeakingCache{false};

	alignas(hardware_destructive_interference_size) std::jthread m_workerThread;
	std::atomic<bool> m_running{false};

	alignas(hardware_destructive_interference_size) std::queue<ThreadCommand> m_commandQueue;
	alignas(hardware_destructive_interference_size) std::mutex m_queueMutex;
	alignas(hardware_destructive_interference_size) std::condition_variable m_cv;
};

} // namespace Sral