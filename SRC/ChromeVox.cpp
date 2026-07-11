#include "ChromeVox.h"
#include <atomic>
#include <mutex>
#include <string_view>
#include <algorithm>
#include "../Include/SRAL.h"

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
	
	detected_mode = MAIN_THREAD_EM_ASM_INT({
		if (typeof window !== 'undefined' && (window.cvox || typeof cvox !== 'undefined')) {
			return 1;
		}
		if (typeof navigator !== 'undefined' && /\bCrOS\b /.test(navigator.userAgent)) {
			return 2;
		}
		return 0;
	});

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
bool ChromeVox::Speak(const char* speech_text, bool interrupt) {
	if (!speech_text || !is_active.load(std::memory_order_acquire)) [[unlikely]] {
		return false;
	}

	if (speech_text[0] == '\0') [[unlikely]] {
		return false;
	}

	const int current_mode = _mode.load(std::memory_order_acquire);

#if defined(__EMSCRIPTEN__)
	if (current_mode == 1) {
		MAIN_THREAD_EM_ASM(
			{
				try {
					var target = window.cvox || cvox;
					if (target && target.Api) {
						var textStr = UTF8ToString($0);
						target.Api.speak(textStr, $1, {});
					}
				}
				catch (e) {
				}
			},
			speech_text,
			interrupt ? 0 : 1);
		return true;
	}

	if (current_mode == 2) {
		MAIN_THREAD_EM_ASM(
			{
				var container = document.getElementById('sral-chromevox-container');
				var r = document.getElementById('sral-chromevox-region');
				if (container && r) {
					var textStr = UTF8ToString($0);

					if ($1) {
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
			speech_text,
			interrupt ? 1 : 0);
		return true;
	}
#else
	(void)speech_text; (void)interrupt; (void)current_mode;
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

	if (text[0] == '\0') [[unlikely]] {
		return false;
	}

	const int current_mode = _mode.load(std::memory_order_acquire);

	if (current_mode == 1) {
#if defined(__EMSCRIPTEN__)
		MAIN_THREAD_EM_ASM(
			{
				try {
					var target = window.cvox || cvox;
					if (target && target.Api) {
						var textStr = UTF8ToString($0);
						target.Api.braille(textStr, {});
					}
				}
				catch (e) {
				}
			},
			text);
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
#else
	(void)current_mode;
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

bool ChromeVox::SetParameter(int param, const void* value) {
	(void)param; (void)value;
	return false;
}

bool ChromeVox::GetParameter(int param, void* value) {
	(void)param; (void)value;
	return false;
}

int ChromeVox::GetFeatures() {
	const int current_mode = _mode.load(std::memory_order_acquire);
	if (current_mode == 1) {
		return SRAL_SUPPORTS_SPEECH | SRAL_SUPPORTS_BRAILLE;
	}
	return SRAL_SUPPORTS_SPEECH;
}

int ChromeVox::GetNumber() { 
	return SRAL_ENGINE_CHROMEVOX; 
}

int ChromeVox::GetCategory() { 
	return SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE; 
}

int ChromeVox::GetKeyFlags() { 
	return HANDLE_NONE; 
}

} // namespace Sral
