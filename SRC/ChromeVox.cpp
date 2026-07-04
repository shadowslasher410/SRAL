#include "ChromeVox.h"

#include <atomic>
#include <mutex>
#include <string_view>

#include "../Include/SRAL.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace Sral {

std::atomic<int> ChromeVox::_mode{0};
std::atomic<bool> ChromeVox::is_active{false};
std::mutex ChromeVox::chromevox_mutex;

ChromeVox::ChromeVox() = default;

ChromeVox::~ChromeVox() {
	static_cast<void>(ChromeVox::Uninitialize());
}

bool ChromeVox::Initialize() {
	std::lock_guard<std::mutex> lock(chromevox_mutex);
	if (is_active.load(std::memory_order_acquire)) {
		return true;
	}

#if defined(__EMSCRIPTEN__)
	int detected_mode = 0;
#if defined(__INTELLISENSE__) && !defined(__EMSCRIPTEN__)
	detected_mode = 0;
#else
	// clang-format off
	detected_mode = MAIN_THREAD_EM_ASM_INT({
		
		if (typeof window !== 'undefined' && (window.cvox || typeof cvox !== 'undefined')) {
			return 1;
		}
		if (typeof navigator !== 'undefined' && /\bCrOS\b /.test(navigator.userAgent)) {
			return 2;
		}
		return 0;
	});
	// clang-format on
#endif

	if (detected_mode == 0) {
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

bool ChromeVox::Uninitialize() {
	std::lock_guard<std::mutex> lock(chromevox_mutex);
	if (!is_active.load(std::memory_order_acquire)) {
		return true;
	}

#if defined(__EMSCRIPTEN__)
	if (_mode.load(std::memory_order_acquire) == 2) {
		MAIN_THREAD_EM_ASM({
			var r = document.getElementById('sral-chromevox-region');
			if (r) {
				r.remove();
			}
			var container = document.getElementById('sral-chromevox-container');
			if (container) {
				container.remove();
			}
		});
	}
#endif

	_mode.store(0, std::memory_order_release);
	is_active.store(false, std::memory_order_release);
	return true;
}

bool ChromeVox::GetActive() {
	return is_active.load(std::memory_order_acquire);
}

bool ChromeVox::Speak(const char* text, bool interrupt) {
	if (!text || !is_active.load(std::memory_order_acquire)) [[unlikely]] {
		return false;
	}

	std::string_view text_view(text);
	if (text_view.empty())
		return false;

	const int current_mode = _mode.load(std::memory_order_acquire);

#if defined(__EMSCRIPTEN__)
	const char* str_ptr = text_view.data();
	const int str_len = static_cast<int>(text_view.size());

	if (current_mode == 1) {
		MAIN_THREAD_EM_ASM(
			{
				try {
					var target = window.cvox || cvox;
					if (target && target.Api) {
						var textStr = UTF8ArrayToString(HEAPU8, $0, $1);
						target.Api.speak(textStr, $2, {});
					}
				}
				catch (e) {
				}
			},
			str_ptr,
			str_len,
			interrupt ? 0 : 1);
		return true;
	}

	if (current_mode == 2) {
		MAIN_THREAD_EM_ASM(
			{
				var container = document.getElementById('sral-chromevox-container');
				var r = document.getElementById('sral-chromevox-region');
				if (container && r) {
					var textStr = UTF8ArrayToString(HEAPU8, $0, $1);

					if ($2) {
						r.remove();
						r = document.createElement('div');
						r.id = 'sral-chromevox-region';
						r.setAttribute('aria-live', 'assertive');
						r.setAttribute('aria-atomic', 'true');
						r.textContent = textStr;
						container.appendChild(r);
					}
					else {
						var breakToken = r.textContent.length > 0 ? " " : "";
						r.textContent += breakToken + textStr;
					}
				}
			},
			str_ptr,
			str_len,
			interrupt ? 1 : 0);
		return true;
	}
#endif

	return false;
}

bool ChromeVox::SpeakSsml(const char* ssml, bool interrupt) {
	return Speak(ssml, interrupt);
}

bool ChromeVox::Braille(const char* text) {
	if (!text || !is_active.load(std::memory_order_acquire)) [[unlikely]] {
		return false;
	}

	std::string_view text_view(text);
	if (text_view.empty())
		return false;

	const int current_mode = _mode.load(std::memory_order_acquire);

	if (current_mode == 1) {
#if defined(__EMSCRIPTEN__)
		MAIN_THREAD_EM_ASM(
			{
				try {
					var target = window.cvox || cvox;
					if (target && target.Api) {
						var textStr = UTF8ArrayToString(HEAPU8, $0, $1);
						target.Api.braille(textStr, {});
					}
				}
				catch (e) {
				}
			},
			text_view.data(),
			static_cast<int>(text_view.size()));
		return true;
#endif
	}

	return (current_mode == 2);
}

bool ChromeVox::StopSpeech() {
	if (!is_active.load(std::memory_order_acquire)) {
		return false;
	}

	const int current_mode = _mode.load(std::memory_order_acquire);

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
			var container = document.getElementById('sral-chromevox-container');
			var r = document.getElementById('sral-chromevox-region');
			if (container && r) {
				r.remove();
				r = document.createElement('div');
				r.id = 'sral-chromevox-region';
				r.setAttribute('aria-live', 'assertive');
				r.setAttribute('aria-atomic', 'true');
				container.appendChild(r);
			}
		});
		return true;
	}
#endif

	return false;
}

bool ChromeVox::PauseSpeech() {
	return false;
}
bool ChromeVox::ResumeSpeech() {
	return false;
}
bool ChromeVox::IsSpeaking() {
	return false;
}
bool ChromeVox::SetParameter(int, const void*) {
	return false;
}
bool ChromeVox::GetParameter(int, void*) {
	return false;
}

int ChromeVox::GetFeatures() {
	const int current_mode = _mode.load(std::memory_order_acquire);
	if (current_mode == 1) {
		return SRAL_SUPPORTS_SPEECH | SRAL_SUPPORTS_BRAILLE;
	}
	return SRAL_SUPPORTS_SPEECH;
}

} // namespace Sral
