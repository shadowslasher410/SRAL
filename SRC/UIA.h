#ifndef UIA_H_
#define UIA_H_
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <array>
#include "../Include/SRAL.h"
#include "Engine.h"

namespace Sral {

class alignas(destructive_alignment) Uia final : public Engine {
private:
	enum class CommandType : uint8_t {
		Speak,
		Stop
	};

	struct alignas(destructive_alignment) ThreadCommand {
		std::array<char, 512> payload{};
		std::atomic<size_t>   sequence{0};
		CommandType           type{CommandType::Stop};
		bool                  interrupt{false};

		ThreadCommand() noexcept = default;
	};

public:
	Uia() noexcept : m_ring_queue{} {}
	~Uia() override;

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
	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_UIA; }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	[[nodiscard]] int GetFeatures() override { return SRAL_SUPPORTS_SPEECH; }
	[[nodiscard]] int GetKeyFlags() override { return HANDLE_NONE; }
	[[nodiscard]] bool GetActive() override;
	bool Initialize() override;
	bool Uninitialize() override;

private:
	void BackgroundWorkerLoop(std::stop_token stop_token) noexcept;
	void CleanUpMembers() noexcept;

	void* pAutomation{ nullptr };
	void* pCondition{ nullptr };
	void* pElement{ nullptr };
	void* pProvider{ nullptr };

	mutable std::mutex instanceMutex;
	std::atomic<bool>  isInitialized{ false };
	std::atomic<bool>  m_isSpeakingCache{ false };

	static constexpr size_t RING_BUFFER_SIZE = 128;
	static constexpr size_t RING_MASK = RING_BUFFER_SIZE - 1;

	std::array<ThreadCommand, RING_BUFFER_SIZE> m_ring_queue;

	std::atomic<size_t> m_head{0};
	std::atomic<size_t> m_tail{0};
	std::atomic<bool>   m_ring_bell{false};
	std::jthread        m_workerThread;
};

} // namespace Sral

#endif // UIA_H_