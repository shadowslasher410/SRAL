#ifndef ZDSR_H_
#define ZDSR_H_

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <atomic>
#include <memory>
#include <mutex>
#include "../Include/SRAL.h"
#include "Engine.h"

namespace Sral {

class Zdsr final : public Engine {
private:
	struct LibraryDeleter {
		using pointer = HMODULE;
		void operator()(HMODULE handle) const noexcept {
			if (handle) {
				::FreeLibrary(handle);
			}
		}
	};
	using UniqueLibraryHandle = std::unique_ptr<HMODULE, LibraryDeleter>;

public:
	Zdsr() noexcept = default;
	~Zdsr() noexcept override;

	Zdsr(const Zdsr&) = delete;
	Zdsr& operator=(const Zdsr&) = delete;
	Zdsr(Zdsr&&) = delete;
	Zdsr& operator=(Zdsr&&) = delete;

	[[nodiscard]] bool Speak(const char* text, bool interrupt) override;
	[[nodiscard]] bool StopSpeech() override;
	[[nodiscard]] bool IsSpeaking() override;
	[[nodiscard]] bool GetActive() override;

	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_ZDSR; }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	[[nodiscard]] int GetFeatures() override { return SRAL_SUPPORTS_SPEECH; }
	
	[[nodiscard]] bool Initialize() override;
	[[nodiscard]] bool Uninitialize() override;

private:
	/**
	 * @brief Safely tears down the driver context and nullifies function pointers.
	 * @note Caller must hold an exclusive lock on instanceMutex before invoking.
	 */
	void CleanUpMembers() noexcept;

	UniqueLibraryHandle lib{ nullptr };

	using InitTTS_t       = int(WINAPI*)(int, const wchar_t*);
	using Speak_t         = int(WINAPI*)(const wchar_t*, BOOL);
	using GetSpeakState_t = int(WINAPI*)();
	using StopSpeak_t     = int(WINAPI*)();

	InitTTS_t       fInitTTS{ nullptr };
	Speak_t         fSpeak{ nullptr };
	GetSpeakState_t fGetSpeakState{ nullptr };
	StopSpeak_t     fStopSpeak{ nullptr };

	mutable std::mutex instanceMutex;
	std::atomic<bool>  isInitialized{ false };
};

} // namespace Sral

#endif /* defined(_WIN32) || defined(_WIN64) */
#endif /* ZDSR_H_ */
