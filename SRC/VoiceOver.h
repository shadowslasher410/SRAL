#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "../Include/SRAL.h"
#include "Engine.h"

namespace Sral {

class VoiceOver final : public Engine {
private:
	enum class CommandType { Speak, Stop };

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

	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_VOICE_OVER; }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	[[nodiscard]] int GetFeatures() override { return SRAL_SUPPORTS_SPEECH; }
	[[nodiscard]] int GetKeyFlags() override { return HANDLE_NONE; }
	[[nodiscard]] bool GetActive() override;
	[[nodiscard]] bool Initialize() override;
	[[nodiscard]] bool Uninitialize() override;

private:
	void BackgroundWorkerLoop() noexcept;

	mutable std::mutex instanceMutex;
	std::atomic<bool> m_isSpeakingCache{false};
	std::thread m_workerThread;
	std::atomic<bool> m_running{false};
	std::queue<ThreadCommand> m_commandQueue;
	std::mutex m_queueMutex;
	std::condition_variable m_cv;
};

} // namespace Sral
