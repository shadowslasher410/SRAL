#ifndef JAWS_H_
#define JAWS_H_
#pragma once

#include "../Dep/fsapi.h"
#include "../Include/SRAL.h"
#include "Engine.h"
#include <windows.h>
#include <comdef.h>
#include <atomic>
#include <thread>
#include <semaphore>
#include <array>
#include <mutex>
#include <memory>
#include <algorithm>

_COM_SMARTPTR_TYPEDEF(IJawsApi, __uuidof(IJawsApi));

namespace Sral {

class Jaws final : public Engine {
public:
	Jaws() noexcept = default;
	~Jaws() override = default;

	Jaws(const Jaws&) = delete;
	Jaws& operator=(const Jaws&) = delete;
	Jaws(Jaws&&) = delete;
	Jaws& operator=(Jaws&&) = delete;

	[[nodiscard]] bool Speak(const char* text, bool interrupt) override;
	[[nodiscard]] bool Braille(const char* text) override;
	[[nodiscard]] bool StopSpeech() override;
	[[nodiscard]] bool IsSpeaking() override;

	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_JAWS; }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	[[nodiscard]] bool GetActive() override;
	[[nodiscard]] int GetFeatures() override { return SRAL_SUPPORTS_SPEECH | SRAL_SUPPORTS_BRAILLE; }

	[[nodiscard]] bool Initialize() override;
	[[nodiscard]] bool Uninitialize() override;

private:
	enum class CmdType { None, SpeakCmd, BrailleCmd, StopCmd };
	
	static constexpr size_t TEXT_BUFFER_SIZE = 512;
	static constexpr size_t RING_BUFFER_CAPACITY = 32;

	struct Command {
		CmdType type = CmdType::None;
		std::array<wchar_t, TEXT_BUFFER_SIZE> textBuffer{};
		bool interrupt = false;
	};

	struct SharedState {
		std::mutex queueMutex;
		std::array<Command, RING_BUFFER_CAPACITY> ringBuffer{};
		size_t rbHead{0};
		size_t rbTail{0};
		std::counting_semaphore<RING_BUFFER_CAPACITY> queueSemaphore{0};
		std::counting_semaphore<1> initSemaphore{0};
	};

	struct RuntimeContext {
		std::jthread workerThread;
		SharedState state;
	};

	static void WorkerThreadLoop(std::stop_token stopToken, std::shared_ptr<RuntimeContext> context);

	static inline std::shared_ptr<RuntimeContext> s_context{ nullptr };
};

} // namespace Sral

#endif // JAWS_H_
