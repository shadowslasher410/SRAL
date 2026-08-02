#ifndef NS_SPEECH_H
#define NS_SPEECH_H

#if defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>

#if defined(TARGET_OS_OSX) && TARGET_OS_OSX

#include <version>
#include <new>
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

class alignas(hardware_destructive_interference_size) NSSpeech final : public Engine {
public:
    NSSpeech() noexcept = default;
    ~NSSpeech() noexcept override = default;

    NSSpeech(const NSSpeech&) = delete;
    NSSpeech& operator=(const NSSpeech&) = delete;
    NSSpeech(NSSpeech&&) noexcept = delete;
    NSSpeech& operator=(NSSpeech&&) noexcept = delete;

    [[nodiscard]] bool Speak(const char* text, bool interrupt) noexcept override;
    [[nodiscard]] bool StopSpeech() noexcept override;
    [[nodiscard]] bool IsSpeaking() noexcept override;
    [[nodiscard]] bool GetActive() noexcept override;
    [[nodiscard]] bool SetParameter(int param, const void* value) noexcept override;
    [[nodiscard]] bool GetParameter(int param, void* value) noexcept override;

    [[nodiscard]] bool Initialize() noexcept override;
    [[nodiscard]] bool Uninitialize() noexcept override;
    [[nodiscard]] constexpr int GetNumber() noexcept override { return SRAL_ENGINE_NS_SPEECH; }
    [[nodiscard]] constexpr int GetCategory() noexcept override { return SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE; }
    [[nodiscard]] constexpr int GetFeatures() noexcept override { return SRAL_SUPPORTS_SPEECH; }

private:
    static void* obj;
};

} // namespace Sral

#endif /* defined(TARGET_OS_OSX) && TARGET_OS_OSX */
#endif /* defined(__APPLE__) || defined(__MACH__) */
#endif /* NS_SPEECH_H */