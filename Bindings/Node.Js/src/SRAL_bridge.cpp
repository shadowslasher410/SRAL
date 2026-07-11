#include <napi.h>
#include <string>
#include <vector>
#include "../../../Include/SRAL.h"

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
	std::string text = info[0].As<Napi::String>().Utf8Value();
	return Napi::Boolean::New(info.Env(), SRAL_Speak(text.c_str(), info[1].As<Napi::Boolean>().Value()));
}

Napi::Value SpeakSsml(const Napi::CallbackInfo& info) {
	std::string ssml = info[0].As<Napi::String>().Utf8Value();
	return Napi::Boolean::New(info.Env(), SRAL_SpeakSsml(ssml.c_str(), info[1].As<Napi::Boolean>().Value()));
}

Napi::Value Braille(const Napi::CallbackInfo& info) {
	std::string text = info[0].As<Napi::String>().Utf8Value();
	return Napi::Boolean::New(info.Env(), SRAL_Braille(text.c_str()));
}

Napi::Value Output(const Napi::CallbackInfo& info) {
	std::string text = info[0].As<Napi::String>().Utf8Value();
	return Napi::Boolean::New(info.Env(), SRAL_Output(text.c_str(), info[1].As<Napi::Boolean>().Value()));
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

Napi::Value GetEngineCategoryWrap(const Napi::CallbackInfo& info) {
	int engine = info[0].As<Napi::Number>().Int32Value();
	int category = static_cast<int>(SRAL_GetEngineCategory(engine));
	return Napi::Number::New(info.Env(), category);
}

Napi::Value GetTTSEngines(const Napi::CallbackInfo& info) {
	return Napi::Number::New(info.Env(), SRAL_GetTTSEngines());
}

Napi::Value GetAssistiveTechEngines(const Napi::CallbackInfo& info) {
	return Napi::Number::New(info.Env(), SRAL_GetAssistiveTechEngines());
}

Napi::Value DelayOutput(const Napi::CallbackInfo& info) {
	int delayTime = info[0].As<Napi::Number>().Int32Value();
	std::string text = info[1].As<Napi::String>().Utf8Value();
	bool interrupt = info[2].As<Napi::Boolean>().Value();
	return Napi::Boolean::New(info.Env(), SRAL_DelayOutput(delayTime, text.c_str(), interrupt));
}

Napi::Value DelayOutputEx(const Napi::CallbackInfo& info) {
	int engine = info[0].As<Napi::Number>().Int32Value();
	int delayTime = info[1].As<Napi::Number>().Int32Value();
	std::string text = info[2].As<Napi::String>().Utf8Value();
	bool interrupt = info[3].As<Napi::Boolean>().Value();
	return Napi::Boolean::New(info.Env(), SRAL_DelayOutputEx(engine, delayTime, text.c_str(), interrupt));
}

Napi::Value RegisterKeyboardHooks(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(), SRAL_RegisterKeyboardHooks());
}

Napi::Value UnregisterKeyboardHooks(const Napi::CallbackInfo& info) {
	SRAL_UnregisterKeyboardHooks();
	return info.Env().Undefined();
}

Napi::Value SpeakEx(const Napi::CallbackInfo& info) {
	int engine = info[0].As<Napi::Number>().Int32Value();
	std::string text = info[1].As<Napi::String>().Utf8Value();
	bool interrupt = info[2].As<Napi::Boolean>().Value();
	return Napi::Boolean::New(info.Env(), SRAL_SpeakEx(engine, text.c_str(), interrupt));
}

Napi::Value IsSpeakingEx(const Napi::CallbackInfo& info) {
	return Napi::Boolean::New(info.Env(), SRAL_IsSpeakingEx(info[0].As<Napi::Number>().Int32Value()));
}

Napi::Value SpeakSsmlEx(const Napi::CallbackInfo& info) {
	int engine = info[0].As<Napi::Number>().Int32Value();
	std::string ssml = info[1].As<Napi::String>().Utf8Value();
	bool interrupt = info[2].As<Napi::Boolean>().Value();
	return Napi::Boolean::New(info.Env(), SRAL_SpeakSsmlEx(engine, ssml.c_str(), interrupt));
}

Napi::Value BrailleEx(const Napi::CallbackInfo& info) {
	int engine = info[0].As<Napi::Number>().Int32Value();
	std::string text = info[1].As<Napi::String>().Utf8Value();
	return Napi::Boolean::New(info.Env(), SRAL_BrailleEx(engine, text.c_str()));
}

Napi::Value OutputEx(const Napi::CallbackInfo& info) {
	int engine = info[0].As<Napi::Number>().Int32Value();
	std::string text = info[1].As<Napi::String>().Utf8Value();
	bool interrupt = info[2].As<Napi::Boolean>().Value();
	return Napi::Boolean::New(info.Env(), SRAL_OutputEx(engine, text.c_str(), interrupt));
}

Napi::Value StopSpeechEx(const Napi::CallbackInfo& info) {
	int engine = info[0].As<Napi::Number>().Int32Value();
	return Napi::Boolean::New(info.Env(), SRAL_StopSpeechEx(engine));
}

Napi::Value PauseSpeechEx(const Napi::CallbackInfo& info) {
	int engine = info[0].As<Napi::Number>().Int32Value();
	return Napi::Boolean::New(info.Env(), SRAL_PauseSpeechEx(engine));
}

Napi::Value ResumeSpeechEx(const Napi::CallbackInfo& info) {
	int engine = info[0].As<Napi::Number>().Int32Value();
	return Napi::Boolean::New(info.Env(), SRAL_ResumeSpeechEx(engine));
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
Napi::Value GetVoices(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	int e = info[0].As<Napi::Number>().Int32Value();
	int cnt = 0;
	
	if (!SRAL_GetEngineParameter(e, SRAL_PARAM_VOICE_COUNT, &cnt) || cnt <= 0) 
		return Napi::Array::New(env, 0);
		
	void* v_ptr = nullptr;
	if (!SRAL_GetEngineParameter(e, SRAL_PARAM_VOICE_PROPERTIES, &v_ptr) || !v_ptr) 
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
		js_arr.Set(i, o); 
	}
	SRAL_free(v_ptr);
	return js_arr;
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
	std::string text = info[0].As<Napi::String>().Utf8Value();
	uint64_t s; int c, r, b;
	void* p = SRAL_SpeakToMemory(text.c_str(), &s, &c, &r, &b);
	return SpeakToMemoryCommon(info.Env(), p, s, c, r, b);
}

Napi::Value SpeakToMemoryEx(const Napi::CallbackInfo& info) {
	int engine = info[0].As<Napi::Number>().Int32Value();
	std::string text = info[1].As<Napi::String>().Utf8Value();
	uint64_t s; int c, r, b;
	void* p = SRAL_SpeakToMemoryEx(engine, text.c_str(), &s, &c, &r, &b);
	return SpeakToMemoryCommon(info.Env(), p, s, c, r, b);
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
	
	exports.Set("registerKeyboardHooks", Napi::Function::New(env, RegisterKeyboardHooks));
	exports.Set("unregisterKeyboardHooks", Napi::Function::New(env, UnregisterKeyboardHooks));
	exports.Set("getEngineCategory", Napi::Function::New(env, GetEngineCategoryWrap));
	exports.Set("getTTSEngines", Napi::Function::New(env, GetTTSEngines));
	exports.Set("getAssistiveTechEngines", Napi::Function::New(env, GetAssistiveTechEngines));
	exports.Set("delayOutput", Napi::Function::New(env, DelayOutput));
	exports.Set("delayOutputEx", Napi::Function::New(env, DelayOutputEx));
	
	exports.Set("setEngineParameter", Napi::Function::New(env, SetIntParameter));
	exports.Set("getEngineParameter", Napi::Function::New(env, GetIntParameter));
	exports.Set("getVoices", Napi::Function::New(env, GetVoices));
	
	exports.Set("speakToMemory", Napi::Function::New(env, SpeakToMemory));
	exports.Set("speakToMemoryEx", Napi::Function::New(env, SpeakToMemoryEx));

	exports.Set("speakEx", Napi::Function::New(env, SpeakEx));
	exports.Set("isSpeakingEx", Napi::Function::New(env, IsSpeakingEx));
	exports.Set("speakSsmlEx", Napi::Function::New(env, SpeakSsmlEx));
	exports.Set("brailleEx", Napi::Function::New(env, BrailleEx));
	exports.Set("outputEx", Napi::Function::New(env, OutputEx));
	exports.Set("stopSpeechEx", Napi::Function::New(env, StopSpeechEx));
	exports.Set("pauseSpeechEx", Napi::Function::New(env, PauseSpeechEx));
	exports.Set("resumeSpeechEx", Napi::Function::New(env, ResumeSpeechEx));
	
	return exports;
}

NODE_API_MODULE(sral_bridge, Init)
