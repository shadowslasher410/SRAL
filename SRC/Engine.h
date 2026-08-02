#ifndef ENGINE_H_
#define ENGINE_H_
#pragma once

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4514)
#pragma warning(disable : 4820)
#pragma warning(disable : 4324)
#endif

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <vector>
#include <version>

#include "SRAL.h"

namespace Sral {

enum KeyboardFlags : int { HANDLE_NONE = 0, HANDLE_INTERRUPT = 2, HANDLE_PAUSE_RESUME = 4 };

#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201907L
    using std::hardware_destructive_interference_size;
#else
    #if defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64) || defined(TARGET_CPU_ARM64)
        static constexpr size_t hardware_destructive_interference_size = 128;
    #else
        static constexpr size_t hardware_destructive_interference_size = 64;
    #endif
#endif

class alignas(hardware_destructive_interference_size) Engine {
public:
    Engine() noexcept = default;
    virtual ~Engine() noexcept = default;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    Engine(Engine&&) noexcept = default;
    Engine& operator=(Engine&&) noexcept = default;

    [[nodiscard]] virtual bool Speak(const char* text, bool interrupt) noexcept = 0;
    [[nodiscard]] virtual bool SpeakSsml(const char* ssml, bool interrupt) noexcept;

    virtual void* SpeakToMemory(
        const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample) noexcept;

    [[nodiscard]] virtual bool Braille(const char* text) noexcept;
    [[nodiscard]] virtual bool StopSpeech() noexcept = 0;
    [[nodiscard]] virtual bool PauseSpeech() noexcept;
    [[nodiscard]] virtual bool ResumeSpeech() noexcept;
    [[nodiscard]] virtual bool IsSpeaking() noexcept = 0;
    
    [[nodiscard]] virtual int GetNumber() noexcept = 0;
    [[nodiscard]] virtual int GetCategory() noexcept = 0;
    [[nodiscard]] virtual bool GetActive() noexcept = 0;
    [[nodiscard]] virtual int GetFeatures() noexcept = 0;
    [[nodiscard]] virtual int GetKeyFlags() noexcept;
    
    [[nodiscard]] virtual bool SetParameter(int param, const void* value) noexcept;
    [[nodiscard]] virtual bool GetParameter(int param, void* value) noexcept;
    
    virtual bool Initialize() noexcept = 0;
    virtual bool Uninitialize() noexcept = 0;

protected:
    std::vector<std::string> m_strings;
    std::vector<std::pair<std::string_view, const char*>> m_stringCache;
    alignas(hardware_destructive_interference_size) mutable std::mutex m_stringMutex;
    bool paused{false};

    [[nodiscard]] const char* AddString(const char* str) noexcept {
        if (!str || *str == '\0') [[unlikely]] {
            return nullptr;
        }

        const std::string_view lookupView(str);
        std::lock_guard<std::mutex> lock(m_stringMutex);
        
        for (const auto& [view, raw_ptr] : m_stringCache) {
            if (view == lookupView) [[likely]] {
                return raw_ptr;
            }
        }

        m_strings.emplace_back(str);
        const std::string& stableStr = m_strings.back();
        const char* const raw_ptr = stableStr.c_str();
        
        m_stringCache.emplace_back(std::string_view(raw_ptr, stableStr.size()), raw_ptr);
        return raw_ptr;
    }

    void ReleaseAllStrings() noexcept {
        std::lock_guard<std::mutex> lock(m_stringMutex);
        m_stringCache.clear();
        m_strings.clear();
    }
};

} // namespace Sral

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif // ENGINE_H_