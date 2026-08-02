#ifndef CHROMEVOX_H_
#define CHROMEVOX_H_
#pragma once

#include <atomic>
#include <mutex>
#include <new>
#include <string_view>
#include <version>

#include "SRAL.h"
#include "Engine.h"

#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201907L
    using std::hardware_destructive_interference_size;
#else
    #if defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64) || defined(TARGET_CPU_ARM64)
        static constexpr size_t hardware_destructive_interference_size = 128;
    #else
        static constexpr size_t hardware_destructive_interference_size = 64;
    #endif
#endif

namespace Sral {

class alignas(hardware_destructive_interference_size) ChromeVox final : public Engine {
public:
    ChromeVox() noexcept = default;
    ~ChromeVox() noexcept override = default;

    ChromeVox(const ChromeVox&) = delete;
    ChromeVox& operator=(const ChromeVox&) = delete;
    ChromeVox(ChromeVox&&) noexcept = delete;
    ChromeVox& operator=(ChromeVox&&) noexcept = delete;

    [[nodiscard]] bool Speak(const char* speech_text, bool interrupt) noexcept override;
    [[nodiscard]] bool SpeakSsml(const char* ssml, bool interrupt) noexcept override;
    bool Braille(const char* text) noexcept override;

    void* SpeakToMemory(const char*, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample) noexcept override {
        if (buffer_size) *buffer_size = 0;
        if (channels) *channels = 0;
        if (sample_rate) *sample_rate = 0;
        if (bits_per_sample) *bits_per_sample = 0;
        return nullptr;
    }

    bool SetParameter(int param, const void* value) noexcept override;
    [[nodiscard]] bool GetParameter(int param, void* value) noexcept override;

    bool StopSpeech() noexcept override;
    bool PauseSpeech() noexcept override;
    bool ResumeSpeech() noexcept override;
    [[nodiscard]] bool IsSpeaking() noexcept override;

    [[nodiscard]] bool GetActive() noexcept override;
    bool Initialize() noexcept override;
    bool Uninitialize() noexcept override;
    [[nodiscard]] int GetFeatures() noexcept override;
    [[nodiscard]] constexpr int GetNumber() noexcept override { return SRAL_ENGINE_CHROMEVOX; }
    [[nodiscard]] constexpr int GetCategory() noexcept override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
    [[nodiscard]] constexpr int GetKeyFlags() noexcept override { return HANDLE_NONE; }

private:
    static std::atomic<int> _mode;
    static std::atomic<bool> is_active;
    alignas(hardware_destructive_interference_size) static std::mutex chromevox_mutex;
};

} // namespace Sral
#endif /* CHROMEVOX_H_ */