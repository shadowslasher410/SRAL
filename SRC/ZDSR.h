#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

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

class Zdsr final : public Engine {
private:
	struct LibraryDeleter {
		using pointer = HMODULE;
		void operator()(HMODULE handle) const noexcept;
	};
	using UniqueLibraryHandle = std::unique_ptr<HMODULE, LibraryDeleter>;

	enum class CommandType { Speak, Stop };
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

	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_ZDSR; }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	[[nodiscard]] int GetFeatures() override { return SRAL_SUPPORTS_SPEECH; }
	[[nodiscard]] int GetKeyFlags() override { return HANDLE_NONE; }
	[[nodiscard]] bool GetActive() override;

	[[nodiscard]] bool Initialize() override;
	[[nodiscard]] bool Uninitialize() override;

private:
	void CleanUpMembers() noexcept;
	void BackgroundWorkerLoop() noexcept;

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
	std::atomic<bool> isInitialized{false};
	std::atomic<bool> m_isSpeakingCache{false};

	std::thread m_workerThread;
	std::atomic<bool> m_running{false};
	std::queue<ThreadCommand> m_commandQueue;
	std::mutex m_queueMutex;
	std::condition_variable m_cv;
};

} // namespace Sral
