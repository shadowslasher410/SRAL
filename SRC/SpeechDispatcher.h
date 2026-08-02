#ifndef SPEECHDISPATCHER_H_
#define SPEECHDISPATCHER_H_
#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <new>
#include <stop_token>
#include <string_view>
#include <thread>
#include <vector>
#include <version>

#include "Engine.h"
#include "SRAL.h"

#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201907L
    using std::hardware_destructive_interference_size;
#else
    #if defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64) || defined(TARGET_CPU_ARM64)
        static constexpr size_t hardware_destructive_interference_size = 128;
    #else
        static constexpr size_t hardware_destructive_interference_size = 64;
    #endif
#endif

#if defined(__linux__) && !defined(__ANDROID__)
#include <speech-dispatcher/libspeechd.h>
#else
struct SPDConnection;
struct SPDVoice;
using SPDNotificationType = int;
#endif

namespace Sral {

class alignas(hardware_destructive_interference_size) SpeechDispatcher final : public Engine {
private:
    enum class TaskType : uint8_t { Speak, SpeakSsml, Stop, SetParam };

    struct alignas(hardware_destructive_interference_size) AsyncSpdTask {
        std::array<char, 512> payload{};
        std::atomic<size_t> sequence{0};
        size_t payload_length{0};
        TaskType type{TaskType::Stop};
        int param_id{0};
        int param_val{0};
        bool interrupt{false};

        AsyncSpdTask() noexcept = default;
    };

public:
    SpeechDispatcher() noexcept = default;
    ~SpeechDispatcher() noexcept override;

    SpeechDispatcher(const SpeechDispatcher&) = delete;
    SpeechDispatcher& operator=(const SpeechDispatcher&) = delete;
    SpeechDispatcher(SpeechDispatcher&&) noexcept = delete;
    SpeechDispatcher& operator=(SpeechDispatcher&&) noexcept = delete;

    [[nodiscard]] bool Speak(const char* text, bool interrupt) noexcept override;
    [[nodiscard]] bool SpeakSsml(const char* ssml, bool interrupt) noexcept override;
    [[nodiscard]] bool Braille(const char* text) noexcept override;

    [[nodiscard]] bool IsSpeaking() noexcept override;
    [[nodiscard]] bool GetActive() noexcept override;

    [[nodiscard]] bool SetParameter(int param, const void* value) noexcept override;
    [[nodiscard]] bool GetParameter(int param, void* value) noexcept override;

    [[nodiscard]] bool StopSpeech() noexcept override;
    [[nodiscard]] bool PauseSpeech() noexcept override;
    [[nodiscard]] bool ResumeSpeech() noexcept override;

    [[nodiscard]] bool Initialize() noexcept override;
    [[nodiscard]] bool Uninitialize() noexcept override;

    [[nodiscard]] constexpr int GetNumber() noexcept override { return SRAL_ENGINE_SPEECH_DISPATCHER; }
    [[nodiscard]] constexpr int GetCategory() noexcept override { return SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE; }
    [[nodiscard]] constexpr int GetKeyFlags() noexcept override { return HANDLE_NONE; }
    [[nodiscard]] constexpr int GetFeatures() noexcept override {
        return SRAL_SUPPORTS_SPEECH | SRAL_SUPPORTS_BRAILLE | SRAL_SUPPORTS_SPEECH_RATE | SRAL_SUPPORTS_SPEECH_VOLUME |
               SRAL_SUPPORTS_PAUSE_SPEECH | SRAL_SUPPORTS_SPELLING | SRAL_SUPPORTS_SSML | SRAL_SUPPORTS_SELECT_VOICE;
    }

    static void SpeechNotificationCallback(size_t msg_id, size_t client_id, SPDNotificationType type) noexcept;

private:
    void BackgroundWorkerLoop(std::stop_token stop_token) noexcept;
    int SetVoiceIndex() noexcept;
    void ClearStringPool() noexcept;
    void ClearVoiceList() noexcept;
    void RefreshVoiceList() noexcept;

    static std::atomic<bool> is_active;
    static std::mutex speechd_mutex;
    static std::atomic<size_t> m_activeMsgId;
    static constexpr size_t RING_BUFFER_SIZE = 128;
    static constexpr size_t RING_MASK = RING_BUFFER_SIZE - 1;

    alignas(hardware_destructive_interference_size) std::array<AsyncSpdTask, RING_BUFFER_SIZE> m_ring_queue{};
    alignas(hardware_destructive_interference_size) std::atomic<size_t> m_head{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_t> m_tail{0};
    alignas(hardware_destructive_interference_size) std::atomic<bool> m_ring_bell{false};
    alignas(hardware_destructive_interference_size) std::atomic<bool> m_isSpeakingLocal{false};
    alignas(hardware_destructive_interference_size) std::atomic<bool> m_fastPathInterrupt{false};
    alignas(hardware_destructive_interference_size) std::jthread m_worker_thread;
    alignas(hardware_destructive_interference_size) mutable std::mutex m_mutex;
    SPDConnection* speech{nullptr};
    SPDVoice** m_voiceList{nullptr};
    int m_voiceCount{0};
    int m_voiceIndex{0};
    int m_speechRate{0};
    int m_speechVolume{0};
    bool enableSpelling{false};
    bool brailleInitialized{false};
    std::mutex m_string_pool_mutex;
    std::vector<std::wstring> m_voice_strings;
    std::vector<std::string> m_string_pool;
};

} // namespace Sral

#endif // SPEECHDISPATCHER_H_
