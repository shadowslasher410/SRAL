#ifndef NVDA_H_
#define NVDA_H_

#if defined(_WIN32) || defined(_WIN64)
#pragma once

#include <windows.h>
#include <array>
#include <atomic>
#include <mutex>
#include <new>
#include <stop_token>
#include <string_view>
#include <thread>
#include <version>

#include "SRAL.h"
#include "Engine.h"

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

using NVDAController_speakText = error_status_t(__stdcall*)(const wchar_t*);
using NVDAController_brailleMessage = error_status_t(__stdcall*)(const wchar_t*);
using NVDAController_cancelSpeech = error_status_t(__stdcall*)(void);
using NVDAController_testIfRunning = error_status_t(__stdcall*)(void);
using NVDAController_speakSsml = error_status_t(__stdcall*)(const wchar_t*, int, int, int);

class alignas(hardware_destructive_interference_size) Nvda final : public Engine {
private:
    enum class CommandType : uint8_t { None, Speak, SpeakSsml, Braille, Stop };

    struct alignas(hardware_destructive_interference_size) ThreadCommand {
        std::array<wchar_t, 512> payload{};
        std::atomic<size_t> sequence{0};
        size_t payload_length{0};
        CommandType type{CommandType::Stop};
        bool interrupt{false};

        ThreadCommand() noexcept = default;
    };

public:
    Nvda() noexcept = default;
    ~Nvda() noexcept override;

    Nvda(const Nvda&) = delete;
    Nvda& operator=(const Nvda&) = delete;
    Nvda(Nvda&&) = delete;
    Nvda& operator=(Nvda&&) = delete;

    [[nodiscard]] bool Speak(const char* text, bool interrupt) noexcept override;
    [[nodiscard]] bool SpeakSsml(const char* ssml, bool interrupt) noexcept override;

    void* SpeakToMemory(const char*, uint64_t*, int*, int*, int*) noexcept override {
        return nullptr;
    }

    [[nodiscard]] bool SetParameter(int param, const void* value) noexcept override;
    [[nodiscard]] bool GetParameter(int param, void* value) noexcept override;

    [[nodiscard]] bool Braille(const char* text) noexcept override;
    [[nodiscard]] bool StopSpeech() noexcept override;
    [[nodiscard]] bool PauseSpeech() noexcept override;
    [[nodiscard]] bool ResumeSpeech() noexcept override;

    [[nodiscard]] bool IsSpeaking() noexcept override;
    [[nodiscard]] bool GetActive() noexcept override;
    
    [[nodiscard]] bool Initialize() noexcept override;
    [[nodiscard]] bool Uninitialize() noexcept override;

    [[nodiscard]] constexpr int GetNumber() noexcept override { return SRAL_ENGINE_NVDA; }
    [[nodiscard]] constexpr int GetCategory() noexcept override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
    [[nodiscard]] constexpr int GetFeatures() noexcept override {
        return SRAL_SUPPORTS_SPEECH | SRAL_SUPPORTS_BRAILLE | SRAL_SUPPORTS_PAUSE_SPEECH | SRAL_SUPPORTS_SPELLING;
    }

private:
    void BackgroundWorkerLoop(std::stop_token stop_token) noexcept;

    HMODULE m_nvdaDll{nullptr};
    NVDAController_speakText fnSpeakText{nullptr};
    NVDAController_brailleMessage fnBrailleMessage{nullptr};
    NVDAController_cancelSpeech fnCancelSpeech{nullptr};
    NVDAController_testIfRunning fnTestIfRunning{nullptr};
    NVDAController_speakSsml fnSpeakSsml{nullptr};
    
    mutable std::mutex m_mutex;
    std::atomic<bool> m_isInitialized{false};
    
    int symbolLevel{-1};
    bool enable_spelling{false};
    bool use_character_descriptions{false};

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

#endif // _WIN32
#endif // NVDA_H_
