#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <string>
#include <vector>

extern "C" {
typedef struct {
	int index;
	const char* name;
	const char* language;
	const char* gender;
	const char* vendor;
} SRAL_VoiceInfo;

bool SRAL_Initialize(int engines_exclude);
void SRAL_Uninitialize(void);
bool SRAL_IsInitialized(void);
bool SRAL_Speak(const char* text, bool interrupt);
bool SRAL_SpeakSsml(const char* ssml, bool interrupt);
bool SRAL_Braille(const char* text);
bool SRAL_Output(const char* text, bool interrupt);
bool SRAL_StopSpeech(void);
bool SRAL_PauseSpeech(void);
bool SRAL_ResumeSpeech(void);
bool SRAL_IsSpeaking(void);
void SRAL_Delay(int time);
int SRAL_GetCurrentEngine(void);
int SRAL_GetEngineFeatures(int engine);
bool SRAL_SetEngineParameter(int engine, int param, const void* value);
bool SRAL_GetEngineParameter(int engine, int param, void* value);
bool SRAL_SpeakEx(int engine, const char* text, bool interrupt);
bool SRAL_IsSpeakingEx(int engine);
int SRAL_GetAvailableEngines(void);
int SRAL_GetActiveEngines(void);
int SRAL_GetEnginesExclude(void);
bool SRAL_SetEnginesExclude(int engines_exclude);
const char* SRAL_GetEngineName(int engine);
bool SRAL_RegisterKeyboardHooks(void);
void SRAL_UnregisterKeyboardHooks(void);
int SRAL_GetEngineCategory(int engine);
bool SRAL_DelayOutput(const char* text, int time, bool interrupt, bool speak, bool braille, bool ssml);
bool SRAL_DelayOutputEx(int engine, const char* text, int time, bool interrupt, bool speak, bool braille, bool ssml);
void* SRAL_SpeakToMemory(
	const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample);
void* SRAL_SpeakToMemoryEx(
	int engine, const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample);
void SRAL_free(void* memory);
}

// Wrapper to marshal C structures into an emscripten::val object (JavaScript Map)
static emscripten::val GetVoicesJS(int engine) {
	int count = 0;
	if (!SRAL_GetEngineParameter(engine, 4, &count) || count <= 0) { // 4 = VOICE_COUNT
		return emscripten::val::array();
	}

	void* voice_ptr = nullptr;
	if (!SRAL_GetEngineParameter(engine, 3, &voice_ptr) || !voice_ptr) { // 3 = VOICE_PROPERTIES
		return emscripten::val::array();
	}

	SRAL_VoiceInfo* raw_array = static_cast<SRAL_VoiceInfo*>(voice_ptr);
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

// Wrapper for marshalling SpeakToMemory into a JS Object with a Uint8Array view
static emscripten::val SpeakToMemoryJS(const std::string& text) {
	uint64_t size = 0;
	int chan = 0, rate = 0, bits = 0;
	void* ptr = SRAL_SpeakToMemory(text.c_str(), &size, &chan, &rate, &bits);
	if (!ptr)
		return emscripten::val::null();

	emscripten::val result = emscripten::val::object();
	result.set("buffer", emscripten::val(emscripten::typed_memory_view(size, static_cast<uint8_t*>(ptr))));
	result.set("channels", chan);
	result.set("sampleRate", rate);
	result.set("bitsPerSample", bits);

	// Memory remains pinned until read; user must free manually or copy
	return result;
}

// Map parameters into Embind tables
static EMSCRIPTEN_BINDINGS(sral_wasm_module) {
	emscripten::function("initialize", &SRAL_Initialize);
	emscripten::function("uninitialize", &SRAL_Uninitialize);
	emscripten::function("isInitialized", &SRAL_IsInitialized);

	emscripten::function(
		"speak", emscripten::optional_override([](const std::string& t, bool i) { return SRAL_Speak(t.c_str(), i); }));
	emscripten::function("speakSsml",
		emscripten::optional_override([](const std::string& s, bool i) { return SRAL_SpeakSsml(s.c_str(), i); }));
	emscripten::function(
		"braille", emscripten::optional_override([](const std::string& t) { return SRAL_Braille(t.c_str()); }));
	emscripten::function("output",
		emscripten::optional_override([](const std::string& t, bool i) { return SRAL_Output(t.c_str(), i); }));

	emscripten::function("stopSpeech", &SRAL_StopSpeech);
	emscripten::function("pauseSpeech", &SRAL_PauseSpeech);
	emscripten::function("resumeSpeech", &SRAL_ResumeSpeech);
	emscripten::function("isSpeaking", &SRAL_IsSpeaking);
	emscripten::function("delay", &SRAL_Delay);

	emscripten::function("getCurrentEngine", &SRAL_GetCurrentEngine);
	emscripten::function("getEngineFeatures", &SRAL_GetEngineFeatures);
	emscripten::function("getAvailableEngines", &SRAL_GetAvailableEngines);
	emscripten::function("getActiveEngines", &SRAL_GetActiveEngines);
	emscripten::function("getEnginesExclude", &SRAL_GetEnginesExclude);
	emscripten::function("setEnginesExclude", &SRAL_SetEnginesExclude);
	emscripten::function("getEngineCategory", &SRAL_GetEngineCategory);

	emscripten::function("getEngineName", emscripten::optional_override([](int e) {
		const char* name = SRAL_GetEngineName(e);
		return std::string(name ? name : "Unknown Engine");
	}));

	emscripten::function("speakEx", emscripten::optional_override([](int e, const std::string& t, bool i) {
		return SRAL_SpeakEx(e, t.c_str(), i);
	}));
	emscripten::function("isSpeakingEx", &SRAL_IsSpeakingEx);
	emscripten::function("registerKeyboardHooks", &SRAL_RegisterKeyboardHooks);
	emscripten::function("unregisterKeyboardHooks", &SRAL_UnregisterKeyboardHooks);

	emscripten::function(
		"delayOutput", emscripten::optional_override([](const std::string& t, int d, bool i, bool s, bool b, bool ss) {
			return SRAL_DelayOutput(t.c_str(), d, i, s, b, ss);
		}));
	emscripten::function("delayOutputEx",
		emscripten::optional_override([](int e, const std::string& t, int d, bool i, bool s, bool b, bool ss) {
			return SRAL_DelayOutputEx(e, t.c_str(), d, i, s, b, ss);
		}));

	emscripten::function("getVoices", &GetVoicesJS);
	emscripten::function("speakToMemory", &SpeakToMemoryJS);
}
