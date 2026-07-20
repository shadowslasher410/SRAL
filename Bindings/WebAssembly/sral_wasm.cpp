#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "../../Include/SRAL.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/bind.h>
#include <emscripten/val.h>
#else
namespace emscripten {
class val {
public:
	static val array() { return val(); }
	static val object() { return val(); }
	static val null() { return val(); }
	static val global(const char*) { return val(); }
	void set(const char*, int) {}
	void set(const char*, const std::string&) {}
	void set(const char*, const val&) {}
	template <typename R, typename... Args> R call(const char*, Args...) { return R(); }
	template <typename... Args> val new_(Args...) { return val(); }
};
template <typename T> class typed_memory_view {
public:
	typed_memory_view(size_t, T*) {}
};
template <typename T> void function(const char*, T) {}
} // namespace emscripten
#define EMSCRIPTEN_BINDINGS(name) void emscripten_bindings_##name()
#endif

static emscripten::val GetVoicesJS(int engine) {
	int count = 0;
	if (!SRAL_GetEngineParameter(engine, SRAL_PARAM_VOICE_COUNT, &count) || count <= 0) {
		return emscripten::val::array();
	}

	void* voice_ptr = nullptr;
	if (!SRAL_GetEngineParameter(engine, SRAL_PARAM_VOICE_PROPERTIES, &voice_ptr) || !voice_ptr) {
		return emscripten::val::array();
	}

	auto* raw_array = static_cast<SRAL_VoiceInfo*>(voice_ptr);
	emscripten::val js_array = emscripten::val::array();

	for (int i = 0; i < count; i++) {
		emscripten::val obj = emscripten::val::object();
		obj.set("index", raw_array[i].index);
		obj.set("name", std::string(raw_array[i].name ? raw_array[i].name : ""));
		obj.set("language", std::string(raw_array[i].language ? raw_array[i].language : ""));
		obj.set("gender", std::string(raw_array[i].gender ? raw_array[i].gender : ""));
		obj.set("vendor", std::string(raw_array[i].vendor ? raw_array[i].vendor : ""));
		js_array.call<void>("push", obj);
	}

	SRAL_free(voice_ptr);
	return js_array;
}

static emscripten::val SpeakToMemoryJS(const std::string& text) {
	uint64_t size = 0;
	int chan = 0, rate = 0, bits = 0;
	void* ptr = SRAL_SpeakToMemory(text.c_str(), &size, &chan, &rate, &bits);
	if (!ptr)
		return emscripten::val::null();

	emscripten::val result = emscripten::val::object();

#ifdef __EMSCRIPTEN__
	emscripten::val view = emscripten::val(emscripten::typed_memory_view(size, static_cast<uint8_t*>(ptr)));
	emscripten::val copied_buffer = emscripten::val::global("Uint8Array").new_(view).call<emscripten::val>("slice");
	result.set("buffer", copied_buffer);
#else
	result.set("buffer", emscripten::val::null());
#endif

	result.set("channels", chan);
	result.set("sampleRate", rate);
	result.set("bitsPerSample", bits);

	SRAL_free(ptr);
	return result;
}

static emscripten::val SpeakToMemoryExJS(int engine, const std::string& text) {
	uint64_t size = 0;
	int chan = 0, rate = 0, bits = 0;
	void* ptr = SRAL_SpeakToMemoryEx(engine, text.c_str(), &size, &chan, &rate, &bits);
	if (!ptr)
		return emscripten::val::null();

	emscripten::val result = emscripten::val::object();

#ifdef __EMSCRIPTEN__
	emscripten::val view = emscripten::val(emscripten::typed_memory_view(size, static_cast<uint8_t*>(ptr)));
	emscripten::val copied_buffer = emscripten::val::global("Uint8Array").new_(view).call<emscripten::val>("slice");
	result.set("buffer", copied_buffer);
#else
	result.set("buffer", emscripten::val::null());
#endif

	result.set("channels", chan);
	result.set("sampleRate", rate);
	result.set("bitsPerSample", bits);

	SRAL_free(ptr);
	return result;
}

#if defined(__INTELLISENSE__) || !defined(__EMSCRIPTEN__)
void dummy_bindings_completion() {
#else
EMSCRIPTEN_BINDINGS(sral_wasm_module) {
#endif

	emscripten::function("initialize", &SRAL_Initialize);
	emscripten::function("uninitialize", &SRAL_Uninitialize);
	emscripten::function("isInitialized", &SRAL_IsInitialized);

#ifdef __EMSCRIPTEN__
	emscripten::function(
		"speak", emscripten::optional_override([](const std::string& t, bool i) { return SRAL_Speak(t.c_str(), i); }));
	emscripten::function("speakSsml",
		emscripten::optional_override([](const std::string& s, bool i) { return SRAL_SpeakSsml(s.c_str(), i); }));
	emscripten::function(
		"braille", emscripten::optional_override([](const std::string& t) { return SRAL_Braille(t.c_str()); }));
	emscripten::function("output",
		emscripten::optional_override([](const std::string& t, bool i) { return SRAL_Output(t.c_str(), i); }));
#endif

	emscripten::function("stopSpeech", &SRAL_StopSpeech);
	emscripten::function("pauseSpeech", &SRAL_PauseSpeech);
	emscripten::function("resumeSpeech", &SRAL_ResumeSpeech);
	emscripten::function("isSpeaking", &SRAL_IsSpeaking);
	emscripten::function("delay", &SRAL_Delay);

#ifdef __EMSCRIPTEN__
	emscripten::function("setEngineParameter", emscripten::optional_override([](int engine, int param, int value) {
		return SRAL_SetEngineParameter(engine, param, &value);
	}));
	emscripten::function("getEngineParameter", emscripten::optional_override([](int engine, int param) {
		int value = 0;
		if (SRAL_GetEngineParameter(engine, param, &value)) {
			return value;
		}
		return -1;
	}));
#endif

	emscripten::function("getCurrentEngine", &SRAL_GetCurrentEngine);
	emscripten::function("getEngineFeatures", &SRAL_GetEngineFeatures);
	emscripten::function("getAvailableEngines", &SRAL_GetAvailableEngines);
	emscripten::function("getActiveEngines", &SRAL_GetActiveEngines);
	emscripten::function("getEnginesExclude", &SRAL_GetEnginesExclude);
	emscripten::function("setEnginesExclude", &SRAL_SetEnginesExclude);

#ifdef __EMSCRIPTEN__
	emscripten::function("getEngineCategory",
		emscripten::optional_override([](int engine) { return static_cast<int>(SRAL_GetEngineCategory(engine)); }));
#endif

	emscripten::function("getTTSEngines", &SRAL_GetTTSEngines);
	emscripten::function("getAssistiveTechEngines", &SRAL_GetAssistiveTechEngines);

#ifdef __EMSCRIPTEN__
	emscripten::function("getEngineName", emscripten::optional_override([](int e) {
		const char* name = SRAL_GetEngineName(e);
		return std::string(name ? name : "Unknown Engine");
	}));
	emscripten::function("speakEx", emscripten::optional_override([](int e, const std::string& t, bool i) {
		return SRAL_SpeakEx(e, t.c_str(), i);
	}));
#endif

	emscripten::function("isSpeakingEx", &SRAL_IsSpeakingEx);

#ifdef __EMSCRIPTEN__
	emscripten::function("speakSsmlEx", emscripten::optional_override([](int e, const std::string& s, bool i) {
		return SRAL_SpeakSsmlEx(e, s.c_str(), i);
	}));
	emscripten::function("brailleEx",
		emscripten::optional_override([](int e, const std::string& t) { return SRAL_BrailleEx(e, t.c_str()); }));
	emscripten::function("outputEx", emscripten::optional_override([](const int e, const std::string& t, bool i) {
		return SRAL_OutputEx(e, t.c_str(), i);
	}));
#endif

	emscripten::function("stopSpeechEx", &SRAL_StopSpeechEx);
	emscripten::function("pauseSpeechEx", &SRAL_PauseSpeechEx);
	emscripten::function("resumeSpeechEx", &SRAL_ResumeSpeechEx);

	emscripten::function("registerKeyboardHooks", &SRAL_RegisterKeyboardHooks);
	emscripten::function("unregisterKeyboardHooks", &SRAL_UnregisterKeyboardHooks);

#ifdef __EMSCRIPTEN__
	emscripten::function("delayOutput", emscripten::optional_override([](int d, const std::string& t, bool i) {
		return SRAL_DelayOutput(d, t.c_str(), i);
	}));
	emscripten::function("delayOutputEx", emscripten::optional_override([](int e, int d, const std::string& t, bool i) {
		return SRAL_DelayOutputEx(e, d, t.c_str(), i);
	}));
#endif

	emscripten::function("getVoices", &GetVoicesJS);
	emscripten::function("speakToMemory", &SpeakToMemoryJS);
	emscripten::function("speakToMemoryEx", &SpeakToMemoryExJS);
}
