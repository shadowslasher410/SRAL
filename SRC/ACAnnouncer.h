#ifndef ACANNOUNCER_H_
#define ACANNOUNCER_H_
#pragma once

#include "Engine.h"
#include "../Include/SRAL.h"

#if defined(SRAL_WITH_ACCESSKIT)

    #if defined(_WIN32)
    struct accesskit_windows_adapter;
    #elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
    struct accesskit_ios_adapter;
    #else
    struct accesskit_macos_adapter;
    #endif
    #elif defined(__ANDROID__)
    struct accesskit_android_adapter;
    #else 
    struct accesskit_unix_adapter;
    #endif

    #include <accesskit.h>

#endif

#include <mutex>
#include <thread>
#include <atomic>
#include <array>
#include <new>
#include <optional>

namespace Sral {

#if defined(__cpp_lib_hardware_interference_size)
    using std::hardware_destructive_interference_size;
#else
    constexpr size_t hardware_destructive_interference_size = 64; 
#endif

class alignas(hardware_destructive_interference_size) ACAnnouncer final : public Engine {
public:
    ACAnnouncer();
    ~ACAnnouncer() override;

    ACAnnouncer(const ACAnnouncer&) = delete;
    ACAnnouncer& operator=(const ACAnnouncer&) = delete;
    ACAnnouncer(ACAnnouncer&& other) noexcept;
    ACAnnouncer& operator=(ACAnnouncer&& other) noexcept;

    bool InitializeWithContext(void*);

    bool Speak(const char*, bool) override;
    bool SpeakSsml(const char*, bool) override { return false; }
    void* SpeakToMemory(const char*, uint64_t*, int*, int*, int*) override { return nullptr; }

    bool SetParameter(int, const void*) override { return false; }
    bool GetParameter(int, void*) override { return false; }

    bool Braille(const char*) override { return false; }
    bool StopSpeech() override;
    bool PauseSpeech() override { return false; }
    bool ResumeSpeech() override { return false; }

    [[nodiscard]] int GetNumber() override { return SRAL_ENGINE_ACCESSKIT; }
    [[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER; }
    [[nodiscard]] bool IsSpeaking() override { return GetActive(); }

    [[nodiscard]] bool GetActive() override;
    bool Initialize() override;
    bool Uninitialize() override;
    [[nodiscard]] int GetFeatures() override { return SRAL_SUPPORTS_SPEECH; }
    [[nodiscard]] int GetKeyFlags() override { return HANDLE_NONE; }
    
    [[nodiscard]] static uint64_t GetVoiceCount() { return 0; }
    [[nodiscard]] static const char* GetVoiceName(uint64_t) { return nullptr; }
    static bool SetVoice(uint64_t) { return false; }

private:
    struct alignas(hardware_destructive_interference_size) SpeechTask {
        std::array<char, 512> text{};
        std::atomic<size_t>  sequence{0};
        bool                 interrupt{false};
        SpeechTask() noexcept = default;
    };

    void BackgroundWorkerLoop(std::stop_token);
    [[nodiscard]] bool IsScreenReaderActive() noexcept;
    
    static constexpr size_t RING_BUFFER_SIZE = 128;
    static constexpr size_t RING_MASK = RING_BUFFER_SIZE - 1;
    
    alignas(hardware_destructive_interference_size) std::array<SpeechTask, RING_BUFFER_SIZE> m_ring_queue;

    alignas(hardware_destructive_interference_size) std::atomic<size_t> m_head{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_t> m_tail{0};
    alignas(hardware_destructive_interference_size) std::atomic<bool> m_ring_bell{false};

    std::mutex m_init_mutex;
    std::jthread m_worker_thread;
    void* m_context_handle = nullptr;

#if defined(SRAL_WITH_ACCESSKIT)
    static void OnActionRequestCallback(struct accesskit_action_request*, void*);
    static struct accesskit_tree_update* ProvideUpdateCallback(void*);
    void HandleActionRequest(struct accesskit_action_request* request) noexcept;
    [[nodiscard]] struct accesskit_tree_update* InterceptUpdatePayload() noexcept;

    std::atomic<accesskit_tree_update*> m_active_update_packet{nullptr};
    std::atomic<bool> m_use_id_b{false};
    std::atomic<void*> m_adapter{nullptr};

    static constexpr accesskit_node_id WINDOW_ID = 1;
    static constexpr accesskit_node_id ANNOUNCEMENT_ID_A = 2;
    static constexpr accesskit_node_id ANNOUNCEMENT_ID_B = 3;
#endif
};
}

#endif
