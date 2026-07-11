#include "NVDA.h"

#ifdef _WIN32

#include <windows.h>
#include <iostream>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <semaphore>
#include <array>
#include <memory>
#include <algorithm>
#include <cstring>

#include "../Dep/nvda_control.h"
#include "Encoding.h"

namespace Sral {

static std::atomic<bool> g_nvdaConnected{false};

class NvdaWrapper final {
public:
    enum class CmdType { None, SpeakCmd, SpeakSsmlCmd, BrailleCmd, StopCmd, PauseCmd, ResumeCmd };
    
    static constexpr size_t TEXT_BUFFER_SIZE = 512;
    static constexpr size_t RING_BUFFER_CAPACITY = 32;

    struct Command {
        CmdType type = CmdType::None;
        std::array<char, TEXT_BUFFER_SIZE> textBuffer{};
        bool interrupt = false;
        bool booleanFlag = false;
        int symbolLevel = -1;
        bool spellingEnabled = false;
        bool charDescriptions = false;
    };

    struct SharedState {
        std::mutex queueMutex;
        std::array<Command, RING_BUFFER_CAPACITY> ringBuffer{};
        size_t rbHead{0};
        size_t rbTail{0};
        std::counting_semaphore<RING_BUFFER_CAPACITY> queueSemaphore{0};
        std::counting_semaphore<1> initSemaphore{0};
        
        std::atomic<int> symbolLevel{-1};
        std::atomic<bool> enableSpelling{false};
        std::atomic<bool> useCharacterDescriptions{false};
        std::atomic<bool> extended{false};
        std::atomic<ULONGLONG> lastConnCheckTime{0};
        
        HMODULE lib{nullptr};
        NVDAController_speakText nvdaController_speakText{nullptr};
        NVDAController_brailleMessage nvdaController_brailleMessage{nullptr};
        NVDAController_cancelSpeech nvdaController_cancelSpeech{nullptr};
        NVDAController_testIfRunning nvdaController_testIfRunning{nullptr};
        NVDAController_speakSsml nvdaController_speakSsml{nullptr};
    };

    struct RuntimeContext {
        std::jthread workerThread;
        SharedState state;
    };

    static inline std::shared_ptr<RuntimeContext> s_context{ nullptr };

    static bool InitializeInternal(std::shared_ptr<RuntimeContext> ctx) noexcept {
        auto& state = ctx->state;
        if (state.lib || state.extended.load(std::memory_order_relaxed)) {
            return true;
        }

        if (nvda_connect() == 0) {
            state.extended.store(true, std::memory_order_relaxed);
            return true;
        }
        state.extended.store(false, std::memory_order_relaxed);

        state.lib = ::LoadLibraryW(L"nvdaControllerClient.dll");
        if (state.lib == nullptr) {
            return false;
        }

        state.nvdaController_speakText =
            reinterpret_cast<NVDAController_speakText>(::GetProcAddress(state.lib, "nvdaController_speakText"));
        state.nvdaController_brailleMessage =
            reinterpret_cast<NVDAController_brailleMessage>(::GetProcAddress(state.lib, "nvdaController_brailleMessage"));
        state.nvdaController_cancelSpeech =
            reinterpret_cast<NVDAController_cancelSpeech>(::GetProcAddress(state.lib, "nvdaController_cancelSpeech"));
        state.nvdaController_testIfRunning =
            reinterpret_cast<NVDAController_testIfRunning>(::GetProcAddress(state.lib, "nvdaController_testIfRunning"));
        state.nvdaController_speakSsml =
            reinterpret_cast<NVDAController_speakSsml>(::GetProcAddress(state.lib, "nvdaController_speakSsml"));

        if (!state.nvdaController_speakText || !state.nvdaController_brailleMessage || 
            !state.nvdaController_cancelSpeech || !state.nvdaController_testIfRunning) {
            UninitializeInternal(ctx);
            return false;
        }
        return true;
    }

    static void UninitializeInternal(std::shared_ptr<RuntimeContext> ctx) noexcept {
        auto& state = ctx->state;
        nvda_disconnect();
        state.extended.store(false, std::memory_order_relaxed);

        if (state.lib) {
            ::FreeLibrary(state.lib);
            state.lib = nullptr;
        }

        state.nvdaController_speakText = nullptr;
        state.nvdaController_brailleMessage = nullptr;
        state.nvdaController_cancelSpeech = nullptr;
        state.nvdaController_testIfRunning = nullptr;
        state.nvdaController_speakSsml = nullptr;
    }

private:
    static void HandleSpeak(const Command& cmd, SharedState& state) noexcept {
        if (state.extended.load(std::memory_order_relaxed)) {
            if (!cmd.spellingEnabled) {
                nvda_speak(cmd.textBuffer.data(), cmd.symbolLevel);
            } else {
                nvda_speak_spelling(cmd.textBuffer.data(), "", cmd.charDescriptions);
            }
            return;
        }
        
        if (!state.nvdaController_speakText) return;

        std::wstring out;
        if (cmd.symbolLevel == -1 || !state.nvdaController_speakSsml) {
            if (UnicodeConvert(cmd.textBuffer.data(), out)) {
                state.nvdaController_speakText(out.c_str());
            }
            return;
        }

        std::string safe_content(cmd.textBuffer.data());
        XmlEncode(safe_content);
        std::string final_xml = "<speak>" + safe_content + "</speak>";
        
        if (UnicodeConvert(final_xml.c_str(), out)) {
            error_status_t result = state.nvdaController_speakSsml(out.c_str(), cmd.symbolLevel, 0, true);
            if (result == 1717 && UnicodeConvert(cmd.textBuffer.data(), out)) {
                state.nvdaController_speakText(out.c_str());
            }
        }
    }

    static void HandleSpeakSsml(const Command& cmd, SharedState& state) noexcept {
        if (state.extended.load(std::memory_order_relaxed)) {
            nvda_speak_ssml(cmd.textBuffer.data(), cmd.symbolLevel);
        } else if (state.nvdaController_speakSsml) {
            std::wstring out;
            if (UnicodeConvert(cmd.textBuffer.data(), out)) {
                state.nvdaController_speakSsml(out.c_str(), cmd.symbolLevel, 0, true);
            }
        }
    }

    static void HandleBraille(const Command& cmd, SharedState& state) noexcept {
        if (state.extended.load(std::memory_order_relaxed)) {
            nvda_braille(cmd.textBuffer.data());
        } else if (state.nvdaController_brailleMessage) {
            std::wstring out;
            if (UnicodeConvert(cmd.textBuffer.data(), out)) {
                state.nvdaController_brailleMessage(out.c_str());
            }
        }
    }

    static void HandleCancel(SharedState& state) noexcept {
        if (state.extended.load(std::memory_order_relaxed)) {
            nvda_cancel_speech();
        } else if (state.nvdaController_cancelSpeech) {
            state.nvdaController_cancelSpeech();
        }
    }

    static void HandlePauseResume(bool pause, SharedState& state) noexcept {
        if (state.extended.load(std::memory_order_relaxed)) {
            nvda_pause_speech(pause);
            return;
        }

        INPUT inputs[2] = {
            {.type = INPUT_KEYBOARD, .ki = {.wVk = VK_SHIFT, .dwFlags = 0}},
            {.type = INPUT_KEYBOARD, .ki = {.wVk = VK_SHIFT, .dwFlags = KEYEVENTF_KEYUP}}
        };
        ::SendInput(2, inputs, sizeof(INPUT));
    }

    static void DispatchCommand(const Command& cmd, SharedState& state) noexcept {
        if (cmd.interrupt) {
            HandleCancel(state);
        }

        switch (cmd.type) {
            case CmdType::SpeakCmd:
                HandleSpeak(cmd, state);
                break;
            case CmdType::SpeakSsmlCmd:
                HandleSpeakSsml(cmd, state);
                break;
            case CmdType::BrailleCmd:
                HandleBraille(cmd, state);
                break;
            case CmdType::StopCmd:
                HandleCancel(state);
                break;
            case CmdType::PauseCmd:
                HandlePauseResume(true, state);
                break;
            case CmdType::ResumeCmd:
                HandlePauseResume(false, state);
                break;
            default:
                break;
        }
    }

public:
    static void WorkerThreadLoop(std::stop_token stopToken, std::shared_ptr<RuntimeContext> context) {
        bool success = InitializeInternal(context);
        if (success) {
            g_nvdaConnected.store(true, std::memory_order_relaxed);
        }
        context->state.initSemaphore.release();

        if (!success) return;

        auto& state = context->state;

        while (!stopToken.stop_requested()) {
            context->state.queueSemaphore.acquire();
            if (stopToken.stop_requested()) break;

            Command cmd{};
            bool hasCommand = false;

            {
                std::lock_guard<std::mutex> lock(state.queueMutex);
                if (state.rbHead != state.rbTail) {
                    cmd = state.ringBuffer[state.rbHead];
                    state.rbHead = (state.rbHead + 1) % RING_BUFFER_CAPACITY;
                    hasCommand = true;
                }
            }

            if (hasCommand) {
                DispatchCommand(cmd, state);
            }
        }

        g_nvdaConnected.store(false, std::memory_order_relaxed);
        UninitializeInternal(context);
    }

        static bool InitializeGlobal() {
        if (s_context) {
            if (s_context->workerThread.joinable() && g_nvdaConnected.load(std::memory_order_relaxed)) {
                return true;
            }
            UninitializeGlobal();
        }

        s_context = std::make_shared<RuntimeContext>();
        
        {
            std::lock_guard<std::mutex> lock(s_context->state.queueMutex);
            s_context->state.rbHead = 0;
            s_context->state.rbTail = 0;
            while (s_context->state.queueSemaphore.try_acquire());
            while (s_context->state.initSemaphore.try_acquire());
        }

        s_context->workerThread = std::jthread(&NvdaWrapper::WorkerThreadLoop, s_context);
        s_context->state.initSemaphore.acquire();

        if (!g_nvdaConnected.load(std::memory_order_relaxed)) {
            if (s_context->workerThread.joinable()) {
                s_context->workerThread.detach();
            }
            s_context = nullptr;
            return false;
        }
        return true;
    }

    static void UninitializeGlobal() {
        if (!s_context || !s_context->workerThread.joinable()) return;

        {
            std::lock_guard<std::mutex> lock(s_context->state.queueMutex);
            s_context->workerThread.request_stop();
            s_context->state.queueSemaphore.release();
        }
        
        s_context->workerThread.detach();
        s_context = nullptr;
    }

    static bool PushCommand(const Command& cmd) {
        if (!s_context) return false;
        std::lock_guard<std::mutex> lock(s_context->state.queueMutex);
        
        size_t t = s_context->state.rbTail;
        size_t h = s_context->state.rbHead;
        
        if (((t + 1) % RING_BUFFER_CAPACITY) == h) [[unlikely]] {
            return false; 
        }

        s_context->state.ringBuffer[t] = cmd;
        s_context->state.rbTail = (t + 1) % RING_BUFFER_CAPACITY;
        s_context->state.queueSemaphore.release();
        return true;
    }

    static std::shared_ptr<RuntimeContext> GetContext() noexcept { return s_context; }
};

Nvda::Nvda() = default;

Nvda::~Nvda() {
	(void)Uninitialize();
}

[[nodiscard]] bool Nvda::Initialize() {
    return NvdaWrapper::InitializeGlobal();
}

[[nodiscard]] bool Nvda::Uninitialize() {
    NvdaWrapper::UninitializeGlobal();
	return true;
}

[[nodiscard]] bool Nvda::GetActive() {
    auto ctx = NvdaWrapper::GetContext();
    if (!ctx) return false;
    
    auto& state = ctx->state;
    const ULONGLONG now = ::GetTickCount64();
    ULONGLONG lastCheck = state.lastConnCheckTime.load(std::memory_order_relaxed);

    if (lastCheck > 0 && (now - lastCheck) < 500) {
        return g_nvdaConnected.load(std::memory_order_relaxed);
    }

    if (nvda_active() == 0) {
        state.extended.store(true, std::memory_order_relaxed);
        state.lastConnCheckTime.store(now, std::memory_order_relaxed);
        g_nvdaConnected.store(true, std::memory_order_relaxed);
        return true;
    }

    if (!g_nvdaConnected.load(std::memory_order_relaxed)) {
        return false;
    }

    if (state.extended.load(std::memory_order_relaxed)) {
        return true;
    }

    if (state.nvdaController_testIfRunning && state.nvdaController_testIfRunning() == 0) {
        state.lastConnCheckTime.store(now, std::memory_order_relaxed);
        g_nvdaConnected.store(true, std::memory_order_relaxed);
        return true;
    }
    return false;
}

[[nodiscard]] bool Nvda::Speak(const char* text, bool interrupt) {
	if (!text) {
		text = "";
	}
	if (!GetActive()) {
		return false;
	}

    auto ctx = NvdaWrapper::GetContext();
    if (!ctx) return false;

    NvdaWrapper::Command cmd{};
    cmd.type = NvdaWrapper::CmdType::SpeakCmd;
    cmd.interrupt = interrupt;
    cmd.symbolLevel = symbolLevel;
    cmd.spellingEnabled = enable_spelling;
    cmd.charDescriptions = use_character_descriptions;

    size_t copyLength = (std::min)(std::strlen(text), cmd.textBuffer.size() - 1);
    std::copy_n(text, copyLength, cmd.textBuffer.begin());
    cmd.textBuffer[copyLength] = '\0';

    return NvdaWrapper::PushCommand(cmd);
}

[[nodiscard]] bool Nvda::SpeakSsml(const char* ssml, bool interrupt) {
	if (!ssml) {
		ssml = "";
	}
	if (!GetActive()) {
		return false;
	}

    auto ctx = NvdaWrapper::GetContext();
    if (!ctx) return false;

    NvdaWrapper::Command cmd{};
    cmd.type = NvdaWrapper::CmdType::SpeakSsmlCmd;
    cmd.interrupt = interrupt;
    cmd.symbolLevel = symbolLevel;

    size_t copyLength = (std::min)(std::strlen(ssml), cmd.textBuffer.size() - 1);
    std::copy_n(ssml, copyLength, cmd.textBuffer.begin());
    cmd.textBuffer[copyLength] = '\0';

    return NvdaWrapper::PushCommand(cmd);
}

[[nodiscard]] bool Nvda::SetParameter(int param, const void* value) {
	if (!value) {
		return false;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	switch (param) {
	case SRAL_PARAM_SYMBOL_LEVEL:
		symbolLevel = *reinterpret_cast<const int*>(value);
		break;
	case SRAL_PARAM_ENABLE_SPELLING:
		enable_spelling = *reinterpret_cast<const bool*>(value);
		break;
	case SRAL_PARAM_USE_CHARACTER_DESCRIPTIONS:
		use_character_descriptions = *reinterpret_cast<const bool*>(value);
		break;
	default:
		return false;
	}
	return true;
}

[[nodiscard]] bool Nvda::GetParameter(int param, void* value) {
	if (!value) {
		return false;
	}

    auto ctx = NvdaWrapper::GetContext();
	std::lock_guard<std::mutex> lock(m_mutex);
	switch (param) {
	case SRAL_PARAM_SYMBOL_LEVEL:
		*static_cast<int*>(value) = symbolLevel;
		return true;
	case SRAL_PARAM_ENABLE_SPELLING:
		*static_cast<bool*>(value) = enable_spelling;
		return true;
	case SRAL_PARAM_USE_CHARACTER_DESCRIPTIONS:
		*static_cast<bool*>(value) = use_character_descriptions;
		return true;
	case SRAL_PARAM_NVDA_IS_CONTROL_EX:
		*static_cast<bool*>(value) = ctx ? ctx->state.extended.load(std::memory_order_relaxed) : false;
		return true;
	default:
		return false;
	}
}

[[nodiscard]] bool Nvda::Braille(const char* text) {
	if (!text) {
		text = "";
	}
	if (!GetActive()) {
		return false;
	}

    NvdaWrapper::Command cmd{};
    cmd.type = NvdaWrapper::CmdType::BrailleCmd;
    
    size_t copyLength = (std::min)(std::strlen(text), cmd.textBuffer.size() - 1);
    std::copy_n(text, copyLength, cmd.textBuffer.begin());
    cmd.textBuffer[copyLength] = '\0';

    return NvdaWrapper::PushCommand(cmd);
}

[[nodiscard]] bool Nvda::StopSpeech() {
	if (!GetActive()) {
		return false;
	}

    NvdaWrapper::Command cmd{};
    cmd.type = NvdaWrapper::CmdType::StopCmd;
    return NvdaWrapper::PushCommand(cmd);
}

[[nodiscard]] bool Nvda::PauseSpeech() {
	if (!GetActive()) {
		return false;
	}

    NvdaWrapper::Command cmd{};
    cmd.type = NvdaWrapper::CmdType::PauseCmd;
    cmd.booleanFlag = true;
    return NvdaWrapper::PushCommand(cmd);
}

[[nodiscard]] bool Nvda::ResumeSpeech() {
    if (!GetActive()) {
        return false;
    }

    NvdaWrapper::Command cmd{};
    cmd.type = NvdaWrapper::CmdType::ResumeCmd;
    cmd.booleanFlag = false;
    return NvdaWrapper::PushCommand(cmd);
}

[[nodiscard]] bool Nvda::IsSpeaking() {
    return false; 
}

} // namespace Sral
#endif
