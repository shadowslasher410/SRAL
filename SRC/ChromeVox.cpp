#include "ChromeVox.h"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <string_view>

#include "SRAL.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#define MAIN_THREAD_EM_ASM_INT(...) 0
#define MAIN_THREAD_EM_ASM(...) ((void)0)
#endif

namespace Sral {

std::atomic<int> ChromeVox::_mode{0};
std::atomic<bool> ChromeVox::is_active{false};
std::mutex ChromeVox::chromevox_mutex;

bool ChromeVox::Initialize() noexcept {
    std::lock_guard<std::mutex> lock(chromevox_mutex);
    if (is_active.load(std::memory_order_acquire)) [[unlikely]] {
        return true;
    }

#if defined(__EMSCRIPTEN__)
    const int detected_mode = MAIN_THREAD_EM_ASM_INT({
        if (typeof window !== 'undefined' && (window.cvox || typeof cvox !== 'undefined')) {
            return 1;
        }
        if (typeof navigator !== 'undefined' && /\bCrOS\b/.test(navigator.userAgent)) {
            return 2;
        }
        return 0;
    });

    if (detected_mode == 0) [[unlikely]] {
        return false;
    }

    _mode.store(detected_mode, std::memory_order_release);

    if (detected_mode == 2) {
        MAIN_THREAD_EM_ASM({
            var container = document.getElementById('sral-chromevox-container');
            if (!container) {
                container = document.createElement('div');
                container.id = 'sral-chromevox-container';
                Object.assign(container.style, {
                    position : 'absolute',
                    width : '1px',
                    height : '1px',
                    overflow : 'hidden',
                    clip : 'rect(1px, 1px, 1px, 1px)',
                    whiteSpace : 'nowrap'
                });
                document.body.appendChild(container);
            }

            var r = document.getElementById('sral-chromevox-region');
            if (!r) {
                r = document.createElement('div');
                r.id = 'sral-chromevox-region';
                r.setAttribute('aria-live', 'assertive');
                r.setAttribute('aria-atomic', 'true');
                container.appendChild(r);
            }
        });
    }

    is_active.store(true, std::memory_order_release);
    return true;
#else
    return false;
#endif
}

bool ChromeVox::Uninitialize() noexcept {
    std::lock_guard<std::mutex> lock(chromevox_mutex);
    if (!is_active.load(std::memory_order_acquire)) [[unlikely]] {
        return true;
    }

#if defined(__EMSCRIPTEN__)
    if (_mode.load(std::memory_order_relaxed) == 2) {
        MAIN_THREAD_EM_ASM({
            var r = document.getElementById('sral-chromevox-region');
            if (r) r.remove();
            var container = document.getElementById('sral-chromevox-container');
            if (container) container.remove();
        });
    }
#endif

    _mode.store(0, std::memory_order_release);
    is_active.store(false, std::memory_order_release);
    return true;
}

bool ChromeVox::GetActive() noexcept {
    return is_active.load(std::memory_order_acquire);
}

bool ChromeVox::Speak(const char* speech_text, bool interrupt) noexcept {
    if (!speech_text || speech_text[0] == '\0') [[unlikely]] {
        return false;
    }
    if (!is_active.load(std::memory_order_acquire)) [[unlikely]] {
        return false;
    }

    const int current_mode = _mode.load(std::memory_order_relaxed);

#if defined(__EMSCRIPTEN__)
    if (current_mode == 1) {
        MAIN_THREAD_EM_ASM({
            try {
                var target = window.cvox || cvox;
                if (target && target.Api) {
                    target.Api.speak(UTF8ToString($0), $1, {});
                }
            }
            catch (e) {
            }
        }, speech_text, interrupt ? 0 : 1);
        return true;
    }

    if (current_mode == 2) {
        MAIN_THREAD_EM_ASM({
            var r = document.getElementById('sral-chromevox-region');
            if (r) {
                if ($1) {
                    r.textContent = UTF8ToString($0);
                } else {
                    r.textContent += (r.textContent.length > 0 ? " " : "") + UTF8ToString($0);
                }
            }
        }, speech_text, interrupt ? 1 : 0);
        return true;
    }
#else
    (void)interrupt;
    (void)current_mode;
#endif

    return false;
}

bool ChromeVox::SpeakSsml(const char* ssml, bool interrupt) noexcept {
    return Speak(ssml, interrupt);
}

bool ChromeVox::Braille(const char* text) noexcept {
    if (!text || text[0] == '\0') [[unlikely]] {
        return false;
    }
    if (!is_active.load(std::memory_order_acquire)) [[unlikely]] {
        return false;
    }

    const int current_mode = _mode.load(std::memory_order_relaxed);

    if (current_mode == 1) {
#if defined(__EMSCRIPTEN__)
        MAIN_THREAD_EM_ASM({
            try {
                var target = window.cvox || cvox;
                if (target && target.Api) {
                    target.Api.braille(UTF8ToString($0), {});
                }
            }
            catch (e) {
            }
        }, text);
        return true;
#endif
    }

    return (current_mode == 2);
}

bool ChromeVox::StopSpeech() noexcept {
    if (!is_active.load(std::memory_order_acquire)) [[unlikely]] {
        return false;
    }

    const int current_mode = _mode.load(std::memory_order_relaxed);

#if defined(__EMSCRIPTEN__)
    if (current_mode == 1) {
        MAIN_THREAD_EM_ASM({
            try {
                var target = window.cvox || cvox;
                if (target && target.Api) {
                    target.Api.stop();
                }
            }
            catch (e) {
            }
        });
        return true;
    }

    if (current_mode == 2) {
        MAIN_THREAD_EM_ASM({
            var r = document.getElementById('sral-chromevox-region');
            if (r) {
                r.textContent = "";
            }
        });
        return true;
    }
#else
    (void)current_mode;
#endif

    return false;
}

bool ChromeVox::PauseSpeech() noexcept {
    return false;
}

bool ChromeVox::ResumeSpeech() noexcept {
    return false;
}

bool ChromeVox::IsSpeaking() noexcept {
    return false;
}

bool ChromeVox::SetParameter(int, const void*) noexcept {
    return false;
}

bool ChromeVox::GetParameter(int, void*) noexcept {
    return false;
}

int ChromeVox::GetFeatures() noexcept {
    if (_mode.load(std::memory_order_relaxed) == 1) {
        return SRAL_SUPPORTS_SPEECH | SRAL_SUPPORTS_BRAILLE;
    }
    return SRAL_SUPPORTS_SPEECH;
}

} // namespace Sral