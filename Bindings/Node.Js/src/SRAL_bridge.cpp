#include <napi.h>
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
int SRAL_GetFailedEngines(void);
bool SRAL_IsEngineActive(int engine);
int SRAL_GetEngineCategory(int engine);
bool SRAL_DelayOutput(const char* text, int time, bool interrupt, bool speak, bool braille, bool ssml);
bool SRAL_DelayOutputEx(int engine, const char* text, int time, bool interrupt, bool speak, bool braille, bool ssml);
void* SRAL_SpeakToMemory(
	const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample);
void* SRAL_SpeakToMemoryEx(
	int engine, const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample);
void SRAL_free(void* memory);
}

Napi::Value Initialize(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(), SRAL_Initialize(info[0].As<Napi::Number>().Int32Value()));
}
Napi::Value Uninitialize(const Napi::CallbackInfo& info) {
	SRAL_Uninitialize();
	return info.Env().Undefined();
}
Napi::Value IsInitialized(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(), SRAL_IsInitialized());
}
Napi::Value Speak(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(
		info.Env(), SRAL_Speak(info[0].As<Napi::String>().Utf8Value().c_str(), info[1].As<Napi::Boolean>().Value()));
}
Napi::Value SpeakSsml(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(),
		SRAL_SpeakSsml(info[0].As<Napi::String>().Utf8Value().c_str(), info[1].As<Napi::Boolean>().Value()));
}
Napi::Value Braille(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(), SRAL_Braille(info[0].As<Napi::String>().Utf8Value().c_str()));
}
Napi::Value Output(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(
		info.Env(), SRAL_Output(info[0].As<Napi::String>().Utf8Value().c_str(), info[1].As<Napi::Boolean>().Value()));
}
Napi::Value StopSpeech(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(), SRAL_StopSpeech());
}
Napi::Value PauseSpeech(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(), SRAL_PauseSpeech());
}
Napi::Value ResumeSpeech(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(), SRAL_ResumeSpeech());
}
Napi::Value IsSpeaking(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(), SRAL_IsSpeaking());
}
Napi::Value Delay(const Napi::CallbackInfo& info) {
	SRAL_Delay(info[0].As<Napi::Number>().Int32Value());
	return info.Env().Undefined();
}
Napi::Value GetCurrentEngine(const Napi::CallbackInfo& info) {
	return Napi::Number::New(info.Env(), SRAL_GetCurrentEngine());
}
Napi::Value GetEngineFeatures(const Napi::CallbackInfo& info) {
	return Napi::Number::New(info.Env(), SRAL_GetEngineFeatures(info[0].As<Napi::Number>().Int32Value()));
}
Napi::Value GetAvailableEngines(const Napi::CallbackInfo& info) {
	return Napi::Number::New(info.Env(), SRAL_GetAvailableEngines());
}
Napi::Value GetActiveEngines(const Napi::CallbackInfo& info) {
	return Napi::Number::New(info.Env(), SRAL_GetActiveEngines());
}
Napi::Value GetEnginesExclude(const Napi::CallbackInfo& info) {
	return Napi::Number::New(info.Env(), SRAL_GetEnginesExclude());
}
Napi::Value SetEnginesExclude(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(), SRAL_SetEnginesExclude(info[0].As<Napi::Number>().Int32Value()));
}
Napi::Value GetEngineName(const Napi::CallbackInfo& info) {
	const char* n = SRAL_GetEngineName(info[0].As<Napi::Number>().Int32Value());
	return Napi::String::New(info.Env(), n ? n : "Unknown Engine");
}
Napi::Value SpeakEx(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(),
		SRAL_SpeakEx(info[0].As<Napi::Number>().Int32Value(),
			info[1].As<Napi::String>().Utf8Value().c_str(),
			info[2].As<Napi::Boolean>().Value()));
}
Napi::Value IsSpeakingEx(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(), SRAL_IsSpeakingEx(info[0].As<Napi::Number>().Int32Value()));
}
Napi::Value RegisterKeyboardHooks(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(), SRAL_RegisterKeyboardHooks());
}
Napi::Value UnregisterKeyboardHooks(const Napi::CallbackInfo& info) {
	SRAL_UnregisterKeyboardHooks();
	return info.Env().Undefined();
}
Napi::Value GetEngineCategory(const Napi::CallbackInfo& info) {
	return Napi::Number::New(info.Env(), SRAL_GetEngineCategory(info[0].As<Napi::Number>().Int32Value()));
}
Napi::Value DelayOutput(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(),
		SRAL_DelayOutput(info[0].As<Napi::String>().Utf8Value().c_str(),
			info[1].As<Napi::Number>().Int32Value(),
			info[2].As<Napi::Boolean>().Value(),
			info[3].As<Napi::Boolean>().Value(),
			info[4].As<Napi::Boolean>().Value(),
			info[5].As<Napi::Boolean>().Value()));
}
Napi::Value DelayOutputEx(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(),
		SRAL_DelayOutputEx(info[0].As<Napi::Number>().Int32Value(),
			info[1].As<Napi::String>().Utf8Value().c_str(),
			info[2].As<Napi::Number>().Int32Value(),
			info[3].As<Napi::Boolean>().Value(),
			info[4].As<Napi::Boolean>().Value(),
			info[5].As<Napi::Boolean>().Value(),
			info[6].As<Napi::Boolean>().Value()));
}

Napi::Value SetIntParameter(const Napi::CallbackInfo& info) {
	int e = info[0].As<Napi::Number>().Int32Value();
	int p = info[1].As<Napi::Number>().Int32Value();
	int v = info[2].As<Napi::Number>().Int32Value();
	return Napi::Boolean::New(info.Env(), SRAL_SetEngineParameter(e, p, &v));
}
Napi::Value GetIntParameter(const Napi::CallbackInfo& info) {
	int e = info[0].As<Napi::Number>().Int32Value();
	int p = info[1].As<Napi::Number>().Int32Value();
	int v = -1;
	return SRAL_GetEngineParameter(e, p, &v) ? Napi::Number::New(info.Env(), v) : Napi::Number::New(info.Env(), -1);
}

Napi::Value SpeakToMemoryCommon(Napi::Env env, void* ptr, uint64_t size, int chan, int rate, int bits) {
	if (!ptr)
		return env.Null();
	Napi::Object out = Napi::Object::New(env);
	out.Set("buffer", Napi::Buffer<uint8_t>::Copy(env, static_cast<uint8_t*>(ptr), size));
	out.Set("channels", Napi::Number::New(env, chan));
	out.Set("sampleRate", Napi::Number::New(env, rate));
	out.Set("bitsPerSample", Napi::Number::New(env, bits));
	SRAL_free(ptr);
	return out;
}
Napi::Value SpeakToMemory(const Napi::CallbackInfo& info) {
	uint64_t s;
	int c, r, b;
	void* p = SRAL_SpeakToMemory(info[0].As<Napi::String>().Utf8Value().c_str(), &s, &c, &r, &b);
	return SpeakToMemoryCommon(info.Env(), p, s, c, r, b);
}
Napi::Value SpeakToMemoryEx(const Napi::CallbackInfo& info) {
	int e = info[0].As<Napi::Number>().Int32Value();
	uint64_t s;
	int c, r, b;
	void* p = SRAL_SpeakToMemoryEx(e, info[1].As<Napi::String>().Utf8Value().c_str(), &s, &c, &r, &b);
	return SpeakToMemoryCommon(info.Env(), p, s, c, r, b);
}

Napi::Value GetVoices(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	int e = info[0].As<Napi::Number>().Int32Value();
	int cnt = 0;
	if (!SRAL_GetEngineParameter(e, 4, &cnt) || cnt <= 0)
		return Napi::Array::New(env, 0);
	void* v_ptr = nullptr;
	if (!SRAL_GetEngineParameter(e, 3, &v_ptr) || !v_ptr)
		return Napi::Array::New(env, 0);
	SRAL_VoiceInfo* arr = static_cast<SRAL_VoiceInfo*>(v_ptr);
	Napi::Array js_arr = Napi::Array::New(env, cnt);
	for (int i = 0; i < cnt; i++) {
		Napi::Object o = Napi::Object::New(env);
		o.Set("index", arr[i].index);
		o.Set("name", arr[i].name ? arr[i].name : "");
		o.Set("language", arr[i].language ? arr[i].language : "");
		o.Set("gender", arr[i].gender ? arr[i].gender : "");
		o.Set("vendor", arr[i].vendor ? arr[i].vendor : "");
		js_arr[i] = o;
	}
	SRAL_free(v_ptr);
	return js_arr;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
	exports.Set("initialize", Napi::Function::New(env, Initialize));
	exports.Set("uninitialize", Napi::Function::New(env, Uninitialize));
	exports.Set("isInitialized", Napi::Function::New(env, IsInitialized));
	exports.Set("speak", Napi::Function::New(env, Speak));
	exports.Set("speakSsml", Napi::Function::New(env, SpeakSsml));
	exports.Set("braille", Napi::Function::New(env, Braille));
	exports.Set("output", Napi::Function::New(env, Output));
	exports.Set("stopSpeech", Napi::Function::New(env, StopSpeech));
	exports.Set("pauseSpeech", Napi::Function::New(env, PauseSpeech));
	exports.Set("resumeSpeech", Napi::Function::New(env, ResumeSpeech));
	exports.Set("isSpeaking", Napi::Function::New(env, IsSpeaking));
	exports.Set("delay", Napi::Function::New(env, Delay));
	exports.Set("getCurrentEngine", Napi::Function::New(env, GetCurrentEngine));
	exports.Set("getEngineFeatures", Napi::Function::New(env, GetEngineFeatures));
	exports.Set("getAvailableEngines", Napi::Function::New(env, GetAvailableEngines));
	exports.Set("getActiveEngines", Napi::Function::New(env, GetActiveEngines));
	exports.Set("getEnginesExclude", Napi::Function::New(env, GetEnginesExclude));
	exports.Set("setEnginesExclude", Napi::Function::New(env, SetEnginesExclude));
	exports.Set("getEngineName", Napi::Function::New(env, GetEngineName));
	exports.Set("speakEx", Napi::Function::New(env, SpeakEx));
	exports.Set("isSpeakingEx", Napi::Function::New(env, IsSpeakingEx));
	exports.Set("registerKeyboardHooks", Napi::Function::New(env, RegisterKeyboardHooks));
	exports.Set("unregisterKeyboardHooks", Napi::Function::New(env, UnregisterKeyboardHooks));
	exports.Set("getFailedEngines", Napi::Function::New(env, GetFailedEngines));
	exports.Set("isEngineActive", Napi::Function::New(env, IsEngineActive));
	exports.Set("getEngineCategory", Napi::Function::New(env, GetEngineCategory));
	exports.Set("delayOutput", Napi::Function::New(env, DelayOutput));
	exports.Set("delayOutputEx", Napi::Function::New(env, DelayOutputEx));
	exports.Set("speakToMemory", Napi::Function::New(env, SpeakToMemory));
	exports.Set("speakToMemoryEx", Napi::Function::New(env, SpeakToMemoryEx));
	exports.Set("setIntParameter", Napi::Function::New(env, SetIntParameter));
	exports.Set("getIntParameter", Napi::Function::New(env, GetIntParameter));
	exports.Set("getVoices", Napi::Function::New(env, GetVoices));
	return exports;
}
NODE_API_MODULE(sral_bridge, Init)
