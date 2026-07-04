#include "NVDA.h"

#ifdef _WIN32

#include <windows.h>
#include <iostream>
#include <string>

#include "../Dep/nvda_control.h"
#include "Encoding.h"

namespace Sral {

Nvda::Nvda() = default;

Nvda::~Nvda() {
	(void)Uninitialize();
}

bool Nvda::InitializeInternal() noexcept {
	if (lib || extended) {
		return true;
	}

	if (nvda_connect() == 0) {
		extended = true;
		return true;
	}
	extended = false;

	lib = ::LoadLibraryW(L"nvdaControllerClient.dll");
	if (lib == nullptr) {
		return false;
	}

	nvdaController_speakText =
		reinterpret_cast<NVDAController_speakText>(::GetProcAddress(lib, "nvdaController_speakText"));
	nvdaController_brailleMessage =
		reinterpret_cast<NVDAController_brailleMessage>(::GetProcAddress(lib, "nvdaController_brailleMessage"));
	nvdaController_cancelSpeech =
		reinterpret_cast<NVDAController_cancelSpeech>(::GetProcAddress(lib, "nvdaController_cancelSpeech"));
	nvdaController_testIfRunning =
		reinterpret_cast<NVDAController_testIfRunning>(::GetProcAddress(lib, "nvdaController_testIfRunning"));
	nvdaController_speakSsml =
		reinterpret_cast<NVDAController_speakSsml>(::GetProcAddress(lib, "nvdaController_speakSsml"));

	if (!nvdaController_speakText || !nvdaController_brailleMessage || !nvdaController_cancelSpeech ||
		!nvdaController_testIfRunning) {
		UninitializeInternal();
		return false;
	}
	return true;
}

void Nvda::UninitializeInternal() noexcept {
	nvda_disconnect();
	extended = false;

	if (lib) {
		::FreeLibrary(lib);
		lib = nullptr;
	}

	nvdaController_speakText = nullptr;
	nvdaController_brailleMessage = nullptr;
	nvdaController_cancelSpeech = nullptr;
	nvdaController_testIfRunning = nullptr;
	nvdaController_speakSsml = nullptr;
}

[[nodiscard]] bool Nvda::Initialize() {
	std::lock_guard<std::mutex> lock(m_mutex);
	return InitializeInternal();
}

[[nodiscard]] bool Nvda::Uninitialize() {
	std::lock_guard<std::mutex> lock(m_mutex);
	UninitializeInternal();
	return true;
}

[[nodiscard]] bool Nvda::GetActive() {
	NVDAController_testIfRunning test_func = nullptr;
	bool currently_extended = false;
	const ULONGLONG now = ::GetTickCount64();

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_lastConnCheckTime > 0 && (now - m_lastConnCheckTime) < 500) {
			return (lib != nullptr || extended);
		}

		if (nvda_active() == 0) {
			extended = true;
			m_lastConnCheckTime = now;

			nvdaController_speakText = nullptr;
			nvdaController_brailleMessage = nullptr;
			nvdaController_cancelSpeech = nullptr;
			nvdaController_testIfRunning = nullptr;
			nvdaController_speakSsml = nullptr;
			return true;
		}

		if (!lib) {
			(void)InitializeInternal();
		}

		test_func = nvdaController_testIfRunning;
		currently_extended = extended;
	}

	if (currently_extended) {
		return true;
	}

	if (test_func && test_func() == 0) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_lastConnCheckTime = now;
		return true;
	}
	return false;
}

[[nodiscard]] bool Nvda::Speak(const char* text, bool interrupt) {
	if (!text) {
		text = "";
	}
	if (!GetActive()) {
		return false;
	}

	NVDAController_speakText speak_func = nullptr;
	NVDAController_speakSsml ssml_func = nullptr;
	NVDAController_cancelSpeech cancel_func = nullptr;
	bool is_extended = false;
	bool local_enable_spelling = false;
	bool local_use_char_descriptions = false;
	int local_symbol_level = -1;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		is_extended = extended;
		speak_func = nvdaController_speakText;
		ssml_func = nvdaController_speakSsml;
		cancel_func = nvdaController_cancelSpeech;
		local_enable_spelling = enable_spelling;
		local_use_char_descriptions = use_character_descriptions;
		local_symbol_level = symbolLevel;
	}

	if (interrupt) {
		if (is_extended) {
			nvda_cancel_speech();
		}
		else if (cancel_func) {
			cancel_func();
		}
	}

	if (is_extended) {
		return !local_enable_spelling ? (nvda_speak(text, local_symbol_level) == 0)
									  : (nvda_speak_spelling(text, "", local_use_char_descriptions) == 0);
	}

	std::wstring out;
	if (local_symbol_level == -1 || !ssml_func) {
		if (!UnicodeConvert(text, out)) {
			return false;
		}
		return speak_func && speak_func(out.c_str()) == 0;
	}

	std::string safe_content(text);
	XmlEncode(safe_content);

	std::string final_xml;
	final_xml.reserve(15 + safe_content.size());
	final_xml.append("<speak>").append(safe_content).append("</speak>");

	if (!UnicodeConvert(final_xml.c_str(), out)) {
		return false;
	}

	error_status_t result = 0;
	if (ssml_func) {
		result = ssml_func(out.c_str(), local_symbol_level, 0, true);
	}

	if (result == 1717) {
		if (!UnicodeConvert(text, out)) {
			return false;
		}
		return speak_func && speak_func(out.c_str()) == 0;
	}
	return result == 0;
}

[[nodiscard]] bool Nvda::SpeakSsml(const char* ssml, bool interrupt) {
	if (!ssml) {
		ssml = "";
	}
	if (!GetActive()) {
		return false;
	}

	NVDAController_speakSsml ssml_func = nullptr;
	NVDAController_cancelSpeech cancel_func = nullptr;
	bool is_extended = false;
	int local_symbol_level = -1;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		is_extended = extended;
		ssml_func = nvdaController_speakSsml;
		cancel_func = nvdaController_cancelSpeech;
		local_symbol_level = symbolLevel;
	}

	if (interrupt) {
		if (is_extended) {
			nvda_cancel_speech();
		}
		else if (cancel_func) {
			cancel_func();
		}
	}

	if (is_extended) {
		return nvda_speak_ssml(ssml, local_symbol_level) == 0;
	}

	if (!ssml_func) {
		return false;
	}

	std::wstring out;
	if (!UnicodeConvert(ssml, out)) {
		return false;
	}
	return ssml_func(out.c_str(), local_symbol_level, 0, true) == 0;
}

[[nodiscard]] bool Nvda::SetParameter(int param, const void* value) {
	if (!value) {
		return false;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	switch (param) {
	case SRAL_PARAM_SYMBOL_LEVEL:
		symbolLevel = *reinterpret_cast<const int*>(value);
		break;
	case SRAL_PARAM_ENABLE_SPELLING:
		enable_spelling = *reinterpret_cast<const bool*>(value);
		break;
	case SRAL_PARAM_USE_CHARACTER_DESCRIPTIONS:
		use_character_descriptions = *reinterpret_cast<const bool*>(value);
		break;
	default:
		return false;
	}
	return true;
}

[[nodiscard]] bool Nvda::GetParameter(int param, void* value) {
	if (!value) {
		return false;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	switch (param) {
	case SRAL_PARAM_SYMBOL_LEVEL:
		*static_cast<int*>(value) = symbolLevel;
		return true;
	case SRAL_PARAM_ENABLE_SPELLING:
		*static_cast<bool*>(value) = enable_spelling;
		return true;
	case SRAL_PARAM_USE_CHARACTER_DESCRIPTIONS:
		*static_cast<bool*>(value) = use_character_descriptions;
		return true;
	case SRAL_PARAM_NVDA_IS_CONTROL_EX:
		*static_cast<bool*>(value) = extended;
		return true;
	default:
		return false;
	}
}

[[nodiscard]] bool Nvda::Braille(const char* text) {
	if (!text) {
		text = "";
	}
	if (!GetActive()) {
		return false;
	}

	NVDAController_brailleMessage braille_func = nullptr;
	bool is_extended = false;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		is_extended = extended;
		braille_func = nvdaController_brailleMessage;
	}

	if (is_extended) {
		return nvda_braille(text) == 0;
	}

	if (!braille_func) {
		return false;
	}

	std::wstring out;
	if (!UnicodeConvert(text, out)) {
		return false;
	}
	return braille_func(out.c_str()) == 0;
}

[[nodiscard]] bool Nvda::StopSpeech() {
	if (!GetActive()) {
		return false;
	}

	NVDAController_cancelSpeech cancel_func = nullptr;
	bool is_extended = false;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		is_extended = extended;
		cancel_func = nvdaController_cancelSpeech;
	}

	return is_extended ? (nvda_cancel_speech() == 0) : (cancel_func ? (cancel_func() == 0) : false);
}

[[nodiscard]] bool Nvda::PauseSpeech() {
	if (!GetActive()) {
		return false;
	}

	bool is_extended = false;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		is_extended = extended;
	}

	if (is_extended) {
		return nvda_pause_speech(true) == 0;
	}

	INPUT inputs[2] = {{.type = INPUT_KEYBOARD, .ki = {.wVk = VK_SHIFT, .dwFlags = 0}},
		{.type = INPUT_KEYBOARD, .ki = {.wVk = VK_SHIFT, .dwFlags = KEYEVENTF_KEYUP}}};

	return ::SendInput(2, inputs, sizeof(INPUT)) == 2;
}

[[nodiscard]] bool Nvda::ResumeSpeech() {
	bool is_extended = false;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		is_extended = extended;
	}

	if (is_extended) {
		return nvda_pause_speech(false) == 0;
	}

	return Nvda::PauseSpeech();
}

[[nodiscard]] bool Nvda::IsSpeaking() {
	return false; 
}

}
#endif
