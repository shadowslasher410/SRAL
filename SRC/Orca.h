#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <string_view>
#include <version>

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

#if defined(__linux__) && !defined(__ANDROID__)
struct DBusConnection;
#endif

namespace Sral {

class alignas(hardware_destructive_interference_size) Orca final : public Engine {
public:
    Orca() noexcept = default;
    ~Orca() noexcept final = default;

    Orca(const Orca&) = delete;
    Orca& operator=(const Orca&) = delete;
    Orca(Orca&&) noexcept = delete;
    Orca& operator=(Orca&&) noexcept = delete;

    [[nodiscard]] bool Speak(std::string_view text, bool interrupt) noexcept;
    [[nodiscard]] bool SpeakSsml(std::string_view ssml, bool interrupt) noexcept;
    bool Braille(std::string_view text) noexcept;

    [[nodiscard]] bool Speak(const char* text, bool interrupt) noexcept final {
        return Speak(text ? std::string_view(text) : std::string_view(), interrupt);
    }

    [[nodiscard]] bool SpeakSsml(const char* ssml, bool interrupt) noexcept final {
        return SpeakSsml(ssml ? std::string_view(ssml) : std::string_view(), interrupt);
    }

    bool Braille(const char* text) noexcept final {
        return Braille(text ? std::string_view(text) : std::string_view());
    }

    [[nodiscard]] bool Speak(std::nullptr_t, bool) noexcept;
    [[nodiscard]] bool SpeakSsml(std::nullptr_t, bool) noexcept;
    bool Braille(std::nullptr_t) noexcept;

    [[nodiscard]] bool StopSpeech() noexcept final;
    [[nodiscard]] bool IsSpeaking() noexcept final;
    [[nodiscard]] bool PauseSpeech() noexcept final { return false; }
    [[nodiscard]] bool ResumeSpeech() noexcept final { return false; }

    [[nodiscard]] bool GetActive() noexcept final;
    [[nodiscard]] bool Initialize() noexcept final;
    bool Uninitialize() noexcept final;

    [[nodiscard]] constexpr int GetNumber() noexcept final { return 1 << 12; }
    [[nodiscard]] constexpr int GetCategory() noexcept final { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
    [[nodiscard]] constexpr int GetFeatures() noexcept final { return SRAL_SUPPORTS_SPEECH; }
    [[nodiscard]] constexpr int GetKeyFlags() noexcept final { return HANDLE_NONE; }

    [[nodiscard]] void* SpeakToMemory([[maybe_unused]] const char* text,
        [[maybe_unused]] uint64_t* buffer_size,
        [[maybe_unused]] int* channels,
        [[maybe_unused]] int* sample_rate,
        [[maybe_unused]] int* bits_per_sample) noexcept final {

        if (buffer_size) *buffer_size = 0;
        if (channels) *channels = 0;
        if (sample_rate) *sample_rate = 0;
        if (bits_per_sample) *bits_per_sample = 0;
        return nullptr;
    }

    bool SetParameter([[maybe_unused]] int param, [[maybe_unused]] const void* value) noexcept final { return false; }
    bool GetParameter([[maybe_unused]] int param, [[maybe_unused]] void* value) noexcept final { return false; }

private:
    static std::atomic<bool> is_active;
    alignas(hardware_destructive_interference_size) static std::mutex orca_mutex;

#if defined(__linux__) && !defined(__ANDROID__)
    static DBusConnection* _dbus_connection;
#endif
};

} // namespace Sral
