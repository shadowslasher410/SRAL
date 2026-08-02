#include "NVDA.h"

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <string_view>
#include "nvda_control.h"
#include "Encoding.h"

namespace Sral {

static std::atomic<bool> g_nvdaConnected{false};

Nvda::~Nvda() noexcept {
    static_cast<void>(Nvda::Uninitialize());
}

bool Nvda::Initialize() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_isInitialized.load(std::memory_order_acquire)) [[unlikely]] {
        return true;
    }

    for (size_t i = 0; i < RING_BUFFER_SIZE; ++i) {
        m_ring_queue[i].sequence.store(i, std::memory_order_relaxed);
    }

    m_head.store(0, std::memory_order_relaxed);
    m_tail.store(0, std::memory_order_relaxed);
    m_ring_bell.store(false, std::memory_order_relaxed);
    m_fastPathInterrupt.store(false, std::memory_order_relaxed);
    
    m_workerThread = std::jthread([this](std::stop_token st) noexcept { 
        this->BackgroundWorkerLoop(st); 
    });

    m_isInitialized.store(true, std::memory_order_release);
    return true;
}

bool Nvda::Uninitialize() noexcept {
    std::jthread thread_to_join;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_isInitialized.load(std::memory_order_acquire)) [[unlikely]] {
            return true;
        }

        m_workerThread.request_stop();

        m_ring_bell.store(true, std::memory_order_release);
        m_ring_bell.notify_one();

        thread_to_join = std::move(m_workerThread);
        m_isInitialized.store(false, std::memory_order_release);
    }

    if (thread_to_join.joinable()) {
        thread_to_join.join();
    }
    return true;
}

bool Nvda::GetActive() noexcept {
    if (!m_isInitialized.load(std::memory_order_acquire)) [[unlikely]] return false;

    if (nvda_active() == 0) {
        g_nvdaConnected.store(true, std::memory_order_relaxed);
        return true;
    }

    if (fnTestIfRunning && fnTestIfRunning() == 0) {
        g_nvdaConnected.store(true, std::memory_order_relaxed);
        return true;
    }

    return false;
}

bool Nvda::Speak(const char* text, bool interrupt) noexcept {
    if (!text || text[0] == '\0') [[unlikely]] return false;
    if (!GetActive()) return false;

    if (interrupt) {
        m_fastPathInterrupt.store(true, std::memory_order_release);
        m_tail.store(m_head.load(std::memory_order_relaxed), std::memory_order_release);
    }

    ThreadCommand* task = nullptr;
    size_t ticket = m_head.load(std::memory_order_acquire);

    while (true) {
        task = &m_ring_queue[ticket & RING_MASK];
        size_t seq = task->sequence.load(std::memory_order_acquire);
        intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

        if (difference == 0) {
            if (m_head.compare_exchange_weak(ticket, ticket + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                break;
            }
        } else if (difference < 0) {
            return false; // Queue full boundary
        } else {
            ticket = m_head.load(std::memory_order_acquire);
        }
    }

    int converted_len = MultiByteToWideChar(CP_UTF8, 0, text, -1, task->payload.data(), static_cast<int>(task->payload.size() - 1));
    if (converted_len > 0) {
        task->payload_length = static_cast<size_t>(converted_len - 1);
        task->payload[task->payload_length] = L'\0';
    } else {
        task->sequence.store(ticket + RING_BUFFER_SIZE, std::memory_order_release);
        return false;
    }
    
    task->type = CommandType::Speak;
    task->interrupt = interrupt;
    
    task->sequence.store(ticket + 1, std::memory_order_release);
    m_ring_bell.store(true, std::memory_order_release);
    m_ring_bell.notify_one();
    return true;
}

bool Nvda::SpeakSsml(const char* ssml, bool interrupt) noexcept {
    if (!ssml || ssml[0] == '\0') [[unlikely]] return false;
    if (!GetActive()) return false;

    if (interrupt) {
        m_fastPathInterrupt.store(true, std::memory_order_release);
        m_tail.store(m_head.load(std::memory_order_relaxed), std::memory_order_release);
    }

    ThreadCommand* task = nullptr;
    size_t ticket = m_head.load(std::memory_order_acquire);

    while (true) {
        task = &m_ring_queue[ticket & RING_MASK];
        size_t seq = task->sequence.load(std::memory_order_acquire);
        intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

        if (difference == 0) {
            if (m_head.compare_exchange_weak(ticket, ticket + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                break;
            }
        } else if (difference < 0) {
            return false;
        } else {
            ticket = m_head.load(std::memory_order_acquire);
        }
    }

    int converted_len = MultiByteToWideChar(CP_UTF8, 0, ssml, -1, task->payload.data(), static_cast<int>(task->payload.size() - 1));
    if (converted_len > 0) {
        task->payload_length = static_cast<size_t>(converted_len - 1);
        task->payload[task->payload_length] = L'\0';
    } else {
        task->sequence.store(ticket + RING_BUFFER_SIZE, std::memory_order_release);
        return false;
    }
    
    task->type = CommandType::SpeakSsml;
    task->interrupt = interrupt;
    
    task->sequence.store(ticket + 1, std::memory_order_release);
    m_ring_bell.store(true, std::memory_order_release);
    m_ring_bell.notify_one();
    return true;
}

bool Nvda::SetParameter(int param, const void* value) noexcept {
    if (!value) return false;
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

bool Nvda::GetParameter(int param, void* value) noexcept {
    if (!value) return false;
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
    default:
        return false;
    }
}

bool Nvda::Braille(const char* text) noexcept {
    if (!text || text[0] == '\0') [[unlikely]] return false;
    if (!GetActive()) return false;

    ThreadCommand* task = nullptr;
    size_t ticket = m_head.load(std::memory_order_acquire);

    while (true) {
        task = &m_ring_queue[ticket & RING_MASK];
        size_t seq = task->sequence.load(std::memory_order_acquire);
        intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

        if (difference == 0) {
            if (m_head.compare_exchange_weak(ticket, ticket + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                break;
            }
        } else if (difference < 0) {
            return false;
        } else {
            ticket = m_head.load(std::memory_order_acquire);
        }
    }

    int converted_len = MultiByteToWideChar(CP_UTF8, 0, text, -1, task->payload.data(), static_cast<int>(task->payload.size() - 1));
    if (converted_len > 0) {
        task->payload_length = static_cast<size_t>(converted_len - 1);
        task->payload[task->payload_length] = L'\0';
    } else {
        task->sequence.store(ticket + RING_BUFFER_SIZE, std::memory_order_release);
        return false;
    }
    
    task->type = CommandType::Braille;
    task->interrupt = false;
    
    task->sequence.store(ticket + 1, std::memory_order_release);
    m_ring_bell.store(true, std::memory_order_release);
    m_ring_bell.notify_one();
    return true;
}

bool Nvda::StopSpeech() noexcept {
    m_fastPathInterrupt.store(true, std::memory_order_release);
    m_tail.store(m_head.load(std::memory_order_relaxed), std::memory_order_release);

    m_ring_bell.store(true, std::memory_order_release);
    m_ring_bell.notify_one();
    return true;
}

bool Nvda::PauseSpeech() noexcept {
    if (!GetActive()) return false;

    ThreadCommand* task = nullptr;
    size_t ticket = m_head.load(std::memory_order_acquire);

    while (true) {
        task = &m_ring_queue[ticket & RING_MASK];
        size_t seq = task->sequence.load(std::memory_order_acquire);
        intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

        if (difference == 0) {
            if (m_head.compare_exchange_weak(ticket, ticket + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                break;
            }
        } else if (difference < 0) {
            return false;
        } else {
            ticket = m_head.load(std::memory_order_acquire);
        }
    }

    task->type = CommandType::Stop; 
    task->sequence.store(ticket + 1, std::memory_order_release);
    m_ring_bell.store(true, std::memory_order_release);
    m_ring_bell.notify_one();
    return true;
}

bool Nvda::ResumeSpeech() noexcept {
    return true; 
}

bool Nvda::IsSpeaking() noexcept {
    return m_isSpeakingCache.load(std::memory_order_acquire);
}

void Nvda::BackgroundWorkerLoop(std::stop_token stopToken) noexcept {
    (void)SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    m_nvdaDll = ::LoadLibraryW(L"nvdaControllerClient.dll");
    if (m_nvdaDll) {
        fnSpeakText = reinterpret_cast<NVDAController_speakText>(::GetProcAddress(m_nvdaDll, "nvdaController_speakText"));
        fnBrailleMessage = reinterpret_cast<NVDAController_brailleMessage>(::GetProcAddress(m_nvdaDll, "nvdaController_brailleMessage"));
        fnCancelSpeech = reinterpret_cast<NVDAController_cancelSpeech>(::GetProcAddress(m_nvdaDll, "nvdaController_cancelSpeech"));
        fnTestIfRunning = reinterpret_cast<NVDAController_testIfRunning>(::GetProcAddress(m_nvdaDll, "nvdaController_testIfRunning"));
        fnSpeakSsml = reinterpret_cast<NVDAController_speakSsml>(::GetProcAddress(m_nvdaDll, "nvdaController_speakSsml"));
    }

    bool extendedMode = (nvda_connect() == 0);
    g_nvdaConnected.store(extendedMode || m_nvdaDll, std::memory_order_relaxed);
    std::array<char, 1024> stack_narrow_buf{};

    while (!stopToken.stop_requested()) {
        m_ring_bell.store(false, std::memory_order_release);

        if (m_fastPathInterrupt.exchange(false, std::memory_order_acq_rel)) {
            if (extendedMode) nvda_cancel_speech();
            else if (fnCancelSpeech) fnCancelSpeech();
        }

        size_t tail = m_tail.load(std::memory_order_relaxed);
        size_t head = m_head.load(std::memory_order_acquire);

        if (tail == head) {
            m_ring_bell.wait(false, std::memory_order_acquire);
            continue;
        }

        while (tail != head) {
            if (stopToken.stop_requested()) [[unlikely]] break;

            if (m_fastPathInterrupt.exchange(false, std::memory_order_acq_rel)) {
                if (extendedMode) nvda_cancel_speech();
                else if (fnCancelSpeech) fnCancelSpeech();
                tail = m_head.load(std::memory_order_relaxed);
                m_tail.store(tail, std::memory_order_release);
                break;
            }

            ThreadCommand& cmd = m_ring_queue[tail & RING_MASK];
            size_t seq = cmd.sequence.load(std::memory_order_acquire);

            if (seq != (tail + 1)) {
                break; 
            }

            m_isSpeakingCache.store(true, std::memory_order_relaxed);

             switch (cmd.type) {
            case CommandType::Speak: {
                if (cmd.interrupt) {
                    if (extendedMode) nvda_cancel_speech();
                    else if (fnCancelSpeech) fnCancelSpeech();
                }

                if (extendedMode) {
                    bool spelling;
                    bool descriptions;
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        spelling = enable_spelling;
                        descriptions = use_character_descriptions;
                    }

                    int len = WideCharToMultiByte(CP_UTF8, 0, cmd.payload.data(), static_cast<int>(cmd.payload_length), stack_narrow_buf.data(), static_cast<int>(stack_narrow_buf.size() - 1), nullptr, nullptr);
                    if (len > 0) {
                        stack_narrow_buf[len] = '\0';
                        if (!spelling) {
                            nvda_speak(stack_narrow_buf.data());
                        } else {
                            nvda_speak_spelling(stack_narrow_buf.data());
                        }
                    }
                } else if (fnSpeakText) {
                    fnSpeakText(cmd.payload.data());
                }
                break;
            }
            case CommandType::SpeakSsml: {
                if (extendedMode) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, cmd.payload.data(), static_cast<int>(cmd.payload_length), stack_narrow_buf.data(), static_cast<int>(stack_narrow_buf.size() - 1), nullptr, nullptr);
                    if (len > 0) {
                        stack_narrow_buf[len] = '\0';
                        nvda_speak_ssml(stack_narrow_buf.data());
                    }
                } else if (fnSpeakSsml) {
                    fnSpeakSsml(cmd.payload.data(), symbolLevel, 0, true);
                }
                break;
            }
            case CommandType::Braille: {
                if (extendedMode) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, cmd.payload.data(), static_cast<int>(cmd.payload_length), stack_narrow_buf.data(), static_cast<int>(stack_narrow_buf.size() - 1), nullptr, nullptr);
                    if (len > 0) {
                        stack_narrow_buf[len] = '\0';
                        nvda_braille(stack_narrow_buf.data());
                    }
                } else if (fnBrailleMessage) {
                    fnBrailleMessage(cmd.payload.data());
                }
                break;
            }
            case CommandType::Stop: {
                if (extendedMode) nvda_cancel_speech();
                else if (fnCancelSpeech) fnCancelSpeech();
                break;
            }
            default:
                break;
            }

            m_isSpeakingCache.store(false, std::memory_order_relaxed);

            cmd.sequence.store(tail + RING_BUFFER_SIZE, std::memory_order_release);
            tail++;
            m_tail.store(tail, std::memory_order_release);
            
            head = m_head.load(std::memory_order_acquire);
        }
    }

    nvda_disconnect();
    if (m_nvdaDll) {
        ::FreeLibrary(m_nvdaDll);
        m_nvdaDll = nullptr;
    }
    g_nvdaConnected.store(false, std::memory_order_relaxed);
}

} // namespace Sral
#endif
