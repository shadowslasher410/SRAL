#define SRAL_EXPORT
#include "../Include/SRAL.h"
#include "Engine.h"

#if defined(_WIN32)
#define UNICODE
#include "Jaws.h"
#include "NVDA.h"
#include "SAPI.h"
#include "ZDSR.h"
#ifndef SRAL_NO_UIA
#include "UIA.h"
#endif
#include <windows.h>
#include <tlhelp32.h>
#elif defined(__APPLE__)
#include "AVSpeech.h"
#ifndef SRAL_NO_NSSPEECH
#include "NSSpeech.h"
#endif
#include "VoiceOver.h"
#elif defined(__ANDROID__)
#include <jni.h>
#include "../Dep/AndroidContext.h"
#include "AndroidAccessibilityManager.h"
#include "AndroidTextToSpeech.h"
#else
#include <dbus/dbus.h>
#include "ChromeVox.h"
#include "Orca.h"
#include "SpeechDispatcher.h"
#endif

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class Timer final {
public:
	Timer() noexcept { restart(); }
	[[nodiscard]] uint64_t elapsed() const noexcept {
		const auto now = std::chrono::high_resolution_clock::now();
		return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count());
	}
	void restart() noexcept { start_time = std::chrono::high_resolution_clock::now(); }

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
};

static std::atomic<std::shared_ptr<Sral::Engine>> g_currentEngine{ nullptr };
static std::map<SRAL_Engines, std::shared_ptr<Sral::Engine>> g_engines;

static std::mutex g_sralEngineMutex;
static int g_excludes{SRAL_ENGINE_NONE};
static int g_enginesFailedToInitialize{SRAL_ENGINE_NONE};
static bool g_initialized{false};

struct QueuedOutput final {
	std::string text;
	bool interrupt{false};
	bool braille{false};
	bool speak{false};
	bool ssml{false};
	int time{0};
	std::shared_ptr<Sral::Engine> engine{nullptr};
};

static std::vector<QueuedOutput> g_delayedOutputs;
static std::mutex g_delayedOutputsMutex;
static std::atomic<bool> g_delayOperation{false};
static std::atomic<bool> g_outputThreadRunning{false};
static std::thread g_outputThread;
static std::atomic<uint64_t> g_lastDelayTime{0};

static std::thread g_hookThread;
static std::atomic<bool> g_keyboardHookThread{false};
static std::atomic<bool> g_shiftPressed{false};

static void output_thread();
static void trigger_output_thread_safely();
static void speech_engine_update() noexcept;

extern "C" static bool PlatformRegisterKeyboardHooks(void);
extern "C" static void PlatformUnregisterKeyboardHooks(void);

#if defined(_WIN32)
static HHOOK g_keyboardHook = nullptr;

static inline bool is_kernel_handle_valid(HANDLE h) noexcept {
	return (h != INVALID_HANDLE_VALUE && h != nullptr);
}

static BOOL IsNarratorRunningFast(void) noexcept {
	const HWND hwndUwp = FindWindowW(L"ApplicationFrameWindow", L"Narrator");
	if (hwndUwp != nullptr) {
		return TRUE;
	}

	const HWND hwndClassic = FindWindowW(L"StandardWindow", L"Microsoft Narrator");
	return (hwndClassic != nullptr);
}

static BOOL FindProcess(const wchar_t* name) {
	if (!name) return FALSE;
	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (!is_kernel_handle_valid(hProcessSnap)) return FALSE;

	PROCESSENTRY32 pe32;
	std::memset(&pe32, 0, sizeof(PROCESSENTRY32));
	pe32.dwSize = sizeof(PROCESSENTRY32);

	if (!Process32First(hProcessSnap, &pe32)) {
		CloseHandle(hProcessSnap);
		return FALSE;
	}

	do {
		if (_wcsicmp(pe32.szExeFile, name) == 0) {
			CloseHandle(hProcessSnap);
			return TRUE;
		}
	} while (Process32Next(hProcessSnap, &pe32));

	CloseHandle(hProcessSnap);
	return FALSE;
}
#endif

static std::shared_ptr<Sral::Engine> get_engine_internal(int engine) noexcept {
	auto it = g_engines.find(static_cast<SRAL_Engines>(engine));
	return (it != g_engines.end()) ? it->second : nullptr;
}

static void output_thread() {
	g_outputThreadRunning.store(true, std::memory_order_release);

#if defined(__ANDROID__)
	JNIEnv* local_env = nullptr;
	JavaVM* jvm = GetAndroidJavaVM();
	bool attached_here = false;
	if (jvm) {
		if (jvm->GetEnv(reinterpret_cast<void**>(&local_env), JNI_VERSION_1_6) == JNI_EDETACHED) {
			if (jvm->AttachCurrentThread(&local_env, nullptr) == JNI_OK) {
				attached_here = true;
			}
		}
	}
#endif

	Timer s_timer;
	s_timer.restart();

	while (g_delayOperation.load(std::memory_order_acquire)) {
		QueuedOutput current_output;
		bool has_item = false;

		{
			std::unique_lock<std::mutex> lock(g_delayedOutputsMutex);
			if (!g_delayOperation.load(std::memory_order_relaxed) || g_delayedOutputs.empty()) {
				break;
			}
			current_output = std::move(g_delayedOutputs.front());
			g_delayedOutputs.erase(g_delayedOutputs.begin());
			has_item = true;
		}

		if (!has_item) break;

		s_timer.restart();
		while (s_timer.elapsed() < static_cast<uint64_t>(current_output.time) &&
			g_delayOperation.load(std::memory_order_relaxed)) {
			bool speaking = false;
			if (current_output.engine) {
				speaking = current_output.engine->IsSpeaking();
			}
			if (speaking) {
				s_timer.restart();
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		if (!g_delayOperation.load(std::memory_order_relaxed)) {
			break;
		}

		if (current_output.speak && current_output.engine) {
			if (current_output.ssml) {
				(void)current_output.engine->SpeakSsml(current_output.text.c_str(), current_output.interrupt);
			}
			else {
				(void)current_output.engine->Speak(current_output.text.c_str(), current_output.interrupt);
			}
		}
		else if (current_output.braille && current_output.engine) {
			(void)current_output.engine->Braille(current_output.text.c_str());
		}
	}

#if defined(__ANDROID__)
	if (jvm && attached_here) {
		jvm->DetachCurrentThread();
	}
#endif

	g_delayOperation.store(false, std::memory_order_release);
	g_outputThreadRunning.store(false, std::memory_order_release);
}

static void trigger_output_thread_safely() {
	if (g_outputThread.joinable()) {
		g_outputThread.join();
	}
	g_delayOperation.store(true, std::memory_order_release);
	g_outputThread = std::thread(output_thread);
}

static void speech_engine_update() noexcept {
	if (!g_initialized) return;

	std::shared_ptr<Sral::Engine> current = g_currentEngine.load(std::memory_order_acquire);
	const int category = current ? current->GetCategory() : static_cast<int>(SRAL_ENGINE_CATEGORY_UNKNOWN);
	
	if (!current || !current->GetActive() || category == SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE ||
		category == SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER) {

#if defined(_WIN32) && !defined(SRAL_NO_UIA)
		bool narratorActive = (IsNarratorRunningFast() == TRUE);
		
		if (!narratorActive) {
			static std::atomic<ULONGLONG> s_lastSnapTime{0};
			const ULONGLONG now = ::GetTickCount64();
			const ULONGLONG last = s_lastSnapTime.load(std::memory_order_relaxed);
			
			if (now - last >= 500) {
				s_lastSnapTime.store(now, std::memory_order_relaxed);
				if (FindProcess(L"narrator.exe") == TRUE) {
					narratorActive = true;
				}
			} else {
				if (current && current->GetNumber() == SRAL_ENGINE_UIA) {
					narratorActive = true;
				}
			}
		}

		if (narratorActive) {
			g_currentEngine.store(get_engine_internal(SRAL_ENGINE_UIA), std::memory_order_release);
			return;
		}
		else {
#endif
			std::shared_ptr<Sral::Engine> nextEngine = nullptr;
			for (const auto& [value, ptr] : g_engines) {
				if (ptr && ptr->GetActive() && !(g_excludes & static_cast<int>(value))) {
					nextEngine = ptr;
					break;
				}
			}
			g_currentEngine.store(nextEngine, std::memory_order_release);
#if defined(_WIN32) && !defined(SRAL_NO_UIA)
		}
#endif
	}
}

extern "C" {

SRAL_API void* SRAL_malloc(size_t size) {
	if (size == 0) [[unlikely]] {
		return nullptr;
	}
	return ::malloc(size);
}

SRAL_API void SRAL_free(void* memory) {
	if (memory) [[likely]] {
		::free(memory);
	}
}

SRAL_API bool SRAL_IsInitialized(void) {
	return g_initialized && !g_engines.empty();
}

SRAL_API bool SRAL_Initialize(int engines_exclude) {
	std::lock_guard<std::mutex> lock(g_sralEngineMutex);
	if (g_initialized) {
		return true;
	}

#if defined(_WIN32)
	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
		return false;
	}
	g_engines[SRAL_ENGINE_NVDA] = std::make_shared<Sral::Nvda>();
	g_engines[SRAL_ENGINE_JAWS] = std::make_shared<Sral::Jaws>();
	g_engines[SRAL_ENGINE_ZDSR] = std::make_shared<Sral::Zdsr>();
#ifndef SRAL_NO_UIA
	g_engines[SRAL_ENGINE_UIA] = std::make_shared<Sral::Uia>();
#endif
	g_engines[SRAL_ENGINE_SAPI] = std::make_shared<Sral::Sapi>();
#elif defined(__APPLE__)
	g_engines[SRAL_ENGINE_VOICE_OVER] = std::make_shared<Sral::VoiceOver>();
	g_engines[SRAL_ENGINE_AV_SPEECH] = std::make_shared<Sral::AvSpeech>();
#ifndef SRAL_NO_NSSPEECH
	g_engines[SRAL_ENGINE_NS_SPEECH] = std::make_shared<Sral::NsSpeech>();
#endif
#elif defined(__ANDROID__)
	g_engines[static_cast<SRAL_Engines>(SRAL_ENGINE_ANDROID_ACCESSIBILITY_MANAGER)] =
		std::make_shared<Sral::AndroidAccessibilityManager>();
	g_engines[static_cast<SRAL_Engines>(SRAL_ENGINE_ANDROID_TEXT_TO_SPEECH)] =
		std::make_shared<Sral::AndroidTextToSpeech>();
#else
	g_engines[SRAL_ENGINE_SPEECH_DISPATCHER] = std::make_shared<Sral::SpeechDispatcher>();
	g_engines[SRAL_ENGINE_ORCA] = std::make_shared<Sral::Orca>();
	g_engines[SRAL_ENGINE_CHROMEVOX] = std::make_shared<Sral::ChromeVox>();
#endif

	bool success = false;
	for (const auto& [value, ptr] : g_engines) {
		if (!ptr) [[unlikely]] continue;
		if (!ptr->Initialize()) {
			g_enginesFailedToInitialize |= static_cast<int>(value);
		}
		else {
			success = true;
		}
	}

	g_initialized = success;
	if (!g_initialized) {
		g_engines.clear();
#if defined(_WIN32)
		CoUninitialize();
#endif
		return false;
	}
	g_excludes = engines_exclude;
	return g_initialized;
}

SRAL_API void SRAL_Uninitialize(void) {
	if (g_keyboardHookThread.load(std::memory_order_acquire)) {
		PlatformUnregisterKeyboardHooks();
	}

	g_delayOperation.store(false, std::memory_order_release);
	if (g_outputThread.joinable()) {
		g_outputThread.join();
	}

	std::lock_guard<std::mutex> lock(g_sralEngineMutex);
	if (!g_initialized) {
		return;
	}

	for (const auto& [value, ptr] : g_engines) {
		if (ptr) {
			(void)ptr->Uninitialize();
		}
	}

#ifdef _WIN32
	CoUninitialize();
#endif
#ifdef __ANDROID__
	Sral::ClearAndroidContext();
#endif

	g_currentEngine.store(nullptr, std::memory_order_release);
	g_engines.clear();
	g_excludes = SRAL_ENGINE_NONE;
	g_enginesFailedToInitialize = SRAL_ENGINE_NONE;
	g_initialized = false;
}


SRAL_API bool SRAL_Speak(const char* text, bool interrupt) {
	speech_engine_update();
	std::shared_ptr<Sral::Engine> active = g_currentEngine.load(std::memory_order_acquire);
	if (!active) {
		return false;
	}
	return SRAL_SpeakEx(active->GetNumber(), text, interrupt);
}

SRAL_API void* SRAL_SpeakToMemory(
	const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample) {
	speech_engine_update();
	std::shared_ptr<Sral::Engine> active = g_currentEngine.load(std::memory_order_acquire);
	if (!active) {
		return nullptr;
	}
	return active->SpeakToMemory(text, buffer_size, channels, sample_rate, bits_per_sample);
}

SRAL_API bool SRAL_SpeakSsml(const char* ssml, bool interrupt) {
	speech_engine_update();
	std::shared_ptr<Sral::Engine> active = g_currentEngine.load(std::memory_order_acquire);
	if (!active) {
		return false;
	}
	return SRAL_SpeakSsmlEx(active->GetNumber(), ssml, interrupt);
}

SRAL_API bool SRAL_Braille(const char* text) {
	speech_engine_update();
	std::shared_ptr<Sral::Engine> active = g_currentEngine.load(std::memory_order_acquire);
	if (!active) {
		return false;
	}
	return SRAL_BrailleEx(active->GetNumber(), text);
}

SRAL_API bool SRAL_Output(const char* text, bool interrupt) {
	speech_engine_update();
	std::shared_ptr<Sral::Engine> active = g_currentEngine.load(std::memory_order_acquire);
	if (!active) {
		return false;
	}
	return SRAL_OutputEx(active->GetNumber(), text, interrupt);
}

SRAL_API bool SRAL_StopSpeech(void) {
	speech_engine_update();
	std::shared_ptr<Sral::Engine> active = g_currentEngine.load(std::memory_order_acquire);
	if (!active) {
		return false;
	}
	return SRAL_StopSpeechEx(active->GetNumber());
}

SRAL_API bool SRAL_PauseSpeech(void) {
	speech_engine_update();
	std::shared_ptr<Sral::Engine> active = g_currentEngine.load(std::memory_order_acquire);
	if (!active) {
		return false;
	}
	return SRAL_PauseSpeechEx(active->GetNumber());
}

SRAL_API bool SRAL_ResumeSpeech(void) {
	speech_engine_update();
	std::shared_ptr<Sral::Engine> active = g_currentEngine.load(std::memory_order_acquire);
	if (!active) {
		return false;
	}
	return SRAL_ResumeSpeechEx(active->GetNumber());
}

SRAL_API bool SRAL_IsSpeaking(void) {
	speech_engine_update();
	std::shared_ptr<Sral::Engine> active = g_currentEngine.load(std::memory_order_acquire);
	if (!active) {
		return false;
	}
	return active->IsSpeaking();
}

SRAL_API int SRAL_GetCurrentEngine(void) {
	speech_engine_update();
	std::shared_ptr<Sral::Engine> active = g_currentEngine.load(std::memory_order_acquire);
	return active ? active->GetNumber() : SRAL_ENGINE_NONE;
}

SRAL_API int SRAL_GetEngineFeatures(int engine) {
	if (engine == 0) {
		std::shared_ptr<Sral::Engine> active = g_currentEngine.load(std::memory_order_acquire);
		return active ? active->GetFeatures() : -1;
	}
	std::shared_ptr<Sral::Engine> e = get_engine_internal(engine);
	return e ? e->GetFeatures() : -1;
}

SRAL_API bool SRAL_SetEngineParameter(int engine, int param, const void* value) {
#ifdef __ANDROID__
	if (param == SRAL_PARAM_ANDROID_JNI_ENV) {
		return Sral::SetAndroidJNIEnv(static_cast<JNIEnv*>(const_cast<void*>(value)));
	}
	if (param == SRAL_PARAM_ANDROID_ACTIVITY) {
		return Sral::SetAndroidActivity(static_cast<jobject>(const_cast<void*>(value)));
	}
#endif
	if (engine == 0) {
		std::shared_ptr<Sral::Engine> active = g_currentEngine.load(std::memory_order_acquire);
		return active ? active->SetParameter(param, value) : false;
	}
	std::shared_ptr<Sral::Engine> e = get_engine_internal(engine);
	return e ? e->SetParameter(param, value) : false;
}

SRAL_API bool SRAL_GetEngineParameter(int engine, int param, void* value) {
	if (engine == 0) {
		std::shared_ptr<Sral::Engine> active = g_currentEngine.load(std::memory_order_acquire);
		return active ? active->GetParameter(param, value) : false;
	}
	std::shared_ptr<Sral::Engine> e = get_engine_internal(engine);
	return e ? e->GetParameter(param, value) : false;
}

SRAL_API bool SRAL_SpeakEx(int engine, const char* text, bool interrupt) {
	std::shared_ptr<Sral::Engine> e = get_engine_internal(engine);
	if (!e) {
		return false;
	}

	if (!g_delayOperation.load(std::memory_order_acquire)) {
		return e->Speak(text, interrupt);
	}

	QueuedOutput qout{
		.text = std::string(text ? text : ""),
		.interrupt = interrupt,
		.braille = false,
		.speak = true,
		.ssml = false,
		.time = static_cast<int>(g_lastDelayTime.load(std::memory_order_relaxed)),
		.engine = e
	};

	{
		std::unique_lock<std::mutex> lock(g_delayedOutputsMutex);
		g_delayedOutputs.push_back(std::move(qout));
	}

	if (!g_outputThreadRunning.load(std::memory_order_acquire)) {
		trigger_output_thread_safely();
	}
	return true;
}

SRAL_API void* SRAL_SpeakToMemoryEx(
	int engine, const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample) {
	std::shared_ptr<Sral::Engine> e = get_engine_internal(engine);
	return e ? e->SpeakToMemory(text, buffer_size, channels, sample_rate, bits_per_sample) : nullptr;
}

SRAL_API bool SRAL_SpeakSsmlEx(int engine, const char* ssml, bool interrupt) {
	std::shared_ptr<Sral::Engine> e = get_engine_internal(engine);
	if (!e) [[unlikely]] {
		return false;
	}

	if (!g_delayOperation.load(std::memory_order_acquire)) {
		return e->SpeakSsml(ssml, interrupt);
	}

	QueuedOutput qout{
		.text = std::string(ssml ? ssml : ""),
		.interrupt = interrupt,
		.braille = false,
		.speak = true,
		.ssml = true,
		.time = static_cast<int>(g_lastDelayTime.load(std::memory_order_relaxed)),
		.engine = e
	};

	{
		std::unique_lock<std::mutex> lock(g_delayedOutputsMutex);
		g_delayedOutputs.push_back(std::move(qout));
	}

	if (!g_outputThreadRunning.load(std::memory_order_acquire)) {
		trigger_output_thread_safely();
	}
	return true;
}

SRAL_API bool SRAL_BrailleEx(int engine, const char* text) {
	std::shared_ptr<Sral::Engine> e = get_engine_internal(engine);
	return e ? e->Braille(text) : false;
}

SRAL_API bool SRAL_OutputEx(int engine, const char* text, bool interrupt) {
	std::shared_ptr<Sral::Engine> e = get_engine_internal(engine);
	if (!e) [[unlikely]] {
		return false;
	}
	const bool speech = e->Speak(text, interrupt);
	const bool braille = e->Braille(text);
	return speech || braille;
}

SRAL_API bool SRAL_StopSpeechEx(int engine) {
	std::shared_ptr<Sral::Engine> e = get_engine_internal(engine);
	if (!e) [[unlikely]] {
		return false;
	}

	if (g_delayOperation.load(std::memory_order_acquire)) {
		{
			std::unique_lock<std::mutex> lock(g_delayedOutputsMutex);
			g_delayedOutputs.clear();
		}
		g_delayOperation.store(false, std::memory_order_release);
		if (g_outputThread.joinable()) {
			g_outputThread.join();
		}
	}
	return e->StopSpeech();
}

SRAL_API bool SRAL_PauseSpeechEx(int engine) {
	std::shared_ptr<Sral::Engine> e = get_engine_internal(engine);
	if (!e) [[unlikely]] {
		return false;
	}

	if (g_delayOperation.load(std::memory_order_acquire)) {
		g_delayOperation.store(false, std::memory_order_release);
		if (g_outputThread.joinable()) {
			g_outputThread.join();
		}
	}
	return e->PauseSpeech();
}

SRAL_API bool SRAL_ResumeSpeechEx(int engine) {
	std::shared_ptr<Sral::Engine> e = get_engine_internal(engine);
	if (!e) [[unlikely]] {
		return false;
	}

	{
		std::unique_lock<std::mutex> lock(g_delayedOutputsMutex);
		if (!g_delayedOutputs.empty()) {
			g_delayOperation.store(true, std::memory_order_release);
			if (!g_outputThreadRunning.load(std::memory_order_acquire)) {
				trigger_output_thread_safely();
			}
		}
	}
	return e->ResumeSpeech();
}

SRAL_API bool SRAL_IsSpeakingEx(int engine) {
	std::shared_ptr<Sral::Engine> e = get_engine_internal(engine);
	return e ? e->IsSpeaking() : false;
}

SRAL_API void SRAL_Delay(int time) {
	if (!SRAL_IsInitialized()) {
		return;
	}
	g_lastDelayTime.store(static_cast<uint64_t>(time), std::memory_order_relaxed);
	g_delayOperation.store(true, std::memory_order_release);
}

SRAL_API bool SRAL_DelayOutput(int time, const char* text, bool interrupt) {
	speech_engine_update();
	std::shared_ptr<Sral::Engine> active = g_currentEngine.load(std::memory_order_acquire);
	if (!active) {
		return false;
	}
	return SRAL_DelayOutputEx(active->GetNumber(), time, text, interrupt);
}

SRAL_API bool SRAL_DelayOutputEx(int engine, int time, const char* text, bool interrupt) {
	if (time < 0 || !SRAL_IsInitialized()) {
		return false;
	}

	std::shared_ptr<Sral::Engine> e = get_engine_internal(engine);
	if (!e) [[unlikely]] {
		return false;
	}

	QueuedOutput qout{
		.text = std::string(text ? text : ""),
		.interrupt = interrupt,
		.braille = false,
		.speak = true,
		.ssml = false,
		.time = time,
		.engine = e
	};

	{
		std::unique_lock<std::mutex> lock(g_delayedOutputsMutex);
		g_delayedOutputs.push_back(std::move(qout));
	}

	if (!g_outputThreadRunning.load(std::memory_order_acquire)) {
		trigger_output_thread_safely();
	}
	return true;
}

SRAL_API int SRAL_GetAvailableEngines(void) {
	if (g_engines.empty()) {
		return 0;
	}
	int mask = 0;
	for (const auto& [value, ptr] : g_engines) {
		if (ptr) {
			mask |= static_cast<int>(value);
		}
	}
	return mask;
}

SRAL_API int SRAL_GetActiveEngines(void) {
	if (g_engines.empty()) {
		return 0;
	}
	int mask = 0;
	for (const auto& [value, ptr] : g_engines) {
		if (ptr && ptr->GetActive()) {
			mask |= static_cast<int>(value);
		}
	}
	return mask;
}

SRAL_API SRAL_EngineCategory SRAL_GetEngineCategory(int engine) {
	if (!SRAL_IsInitialized()) {
		return SRAL_ENGINE_CATEGORY_UNKNOWN;
	}
	std::shared_ptr<Sral::Engine> e = get_engine_internal(engine);
	return e ? static_cast<SRAL_EngineCategory>(e->GetCategory()) : SRAL_ENGINE_CATEGORY_UNKNOWN;
}

SRAL_API int SRAL_GetTTSEngines(void) {
	if (g_engines.empty()) {
		return 0;
	}
	int mask = 0;
	for (const auto& [value, ptr] : g_engines) {
		if (ptr && ptr->GetCategory() == SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE) {
			mask |= static_cast<int>(value);
		}
	}
	return mask;
}

SRAL_API int SRAL_GetAssistiveTechEngines(void) {
	if (g_engines.empty()) {
		return 0;
	}
	int mask = 0;
	for (const auto& [value, ptr] : g_engines) {
		if (!ptr) continue;
		const int category = ptr->GetCategory();
		if (category == SRAL_ENGINE_CATEGORY_SCREEN_READER || category == SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER) {
			mask |= static_cast<int>(value);
		}
	}
	return mask;
}

SRAL_API const char* SRAL_GetEngineName(int engine) {
	switch (static_cast<SRAL_Engines>(engine)) {
	case SRAL_ENGINE_NONE: return "None";
	case SRAL_ENGINE_NVDA: return "NVDA";
	case SRAL_ENGINE_SAPI: return "SAPI";
	case SRAL_ENGINE_JAWS: return "JAWS";
	case SRAL_ENGINE_SPEECH_DISPATCHER: return "Speech Dispatcher";
	case SRAL_ENGINE_UIA: return "UIA";
	case SRAL_ENGINE_AV_SPEECH: return "AV Speech";
	case SRAL_ENGINE_NS_SPEECH: return "NS Speech";
	case SRAL_ENGINE_NARRATOR: return "Narrator";
	case SRAL_ENGINE_VOICE_OVER: return "Voice Over";
	case SRAL_ENGINE_ZDSR: return "ZDSR";
	case SRAL_ENGINE_ANDROID_TEXT_TO_SPEECH: return "Android TTS";
	case SRAL_ENGINE_ANDROID_ACCESSIBILITY_MANAGER: return "Android AccessibilityManager";
	case SRAL_ENGINE_CHROMEVOX: return "ChromeVox";
	case SRAL_ENGINE_ORCA: return "Orca";
	default: return "Unknown";
	}
}

SRAL_API bool SRAL_SetEnginesExclude(int engines_exclude) {
	if (!SRAL_IsInitialized()) {
		return false;
	}
	g_excludes = engines_exclude;
	speech_engine_update();
	return true;
}

SRAL_API int SRAL_GetEnginesExclude(void) {
	return SRAL_IsInitialized() ? g_excludes : -1;
}

} // extern "C"

#if defined(_WIN32)
static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode >= 0) {
		const KBDLLHOOKSTRUCT* const pKeyInfo = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
		if (pKeyInfo) [[likely]] {
			const bool is_control = (pKeyInfo->vkCode == VK_LCONTROL || pKeyInfo->vkCode == VK_RCONTROL);
			const bool is_shift   = (pKeyInfo->vkCode == VK_LSHIFT   || pKeyInfo->vkCode == VK_RSHIFT);

			if (wParam == WM_KEYDOWN) {
				if (is_control) {
					for (const auto& [value, ptr] : g_engines) {
						if (ptr && ptr->GetActive() && (ptr->GetKeyFlags() & Sral::HANDLE_INTERRUPT)) {
							(void)ptr->StopSpeech();
						}
					}
				}
				else if (is_shift && !g_shiftPressed.load(std::memory_order_acquire)) {
					g_shiftPressed.store(true, std::memory_order_release);
					for (const auto& [value, ptr] : g_engines) {
						if (ptr && ptr->GetActive() && (ptr->GetKeyFlags() & Sral::HANDLE_PAUSE_RESUME)) {
							if (ptr->paused) {
								(void)ptr->ResumeSpeech();
							} else {
								(void)ptr->PauseSpeech();
							}
						}
					}
				}
			}
			else if (wParam == WM_KEYUP) {
				if (is_shift) {
					g_shiftPressed.store(false, std::memory_order_release);
				}
			}
		}
	}
	return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}

#elif defined(__linux__) && !defined(__ANDROID__)
static DBusHandlerResult ProcessAtSpiKeyEvent(DBusConnection* conn, DBusMessage* msg, void* user_data) {
	(void)conn;
	(void)user_data;

	if (dbus_message_is_signal(msg, "org.a11y.atspi.DeviceEventController", "DeviceEvent")) {
		DBusMessageIter iter;
		if (dbus_message_iter_init(msg, &iter)) {
			dbus_uint32_t type = 0;
			dbus_int32_t id = 0, hw_code = 0, modifiers = 0, timestamp = 0;
			const char* event_string = nullptr;

			if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_UINT32) {
				dbus_message_iter_get_basic(&iter, &type);
				
				if (dbus_message_iter_next(&iter) && dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INT32) {
					dbus_message_iter_get_basic(&iter, &id);
					
					if (dbus_message_iter_next(&iter) && dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INT32) {
						dbus_message_iter_get_basic(&iter, &hw_code);
						
						if (dbus_message_iter_next(&iter) && dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INT32) {
							dbus_message_iter_get_basic(&iter, &modifiers);
							
							if (dbus_message_iter_next(&iter) && dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INT32) {
								dbus_message_iter_get_basic(&iter, &timestamp);
								
								if (dbus_message_iter_next(&iter) && dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
									dbus_message_iter_get_basic(&iter, &event_string);
								}
							}
						}
					}
				}
			}

			if (event_string) [[likely]] {
				const std::string_view key_name(event_string);
				const bool is_control = (key_name == "Control_L" || key_name == "Control_R");
				const bool is_shift   = (key_name == "Shift_L"   || key_name == "Shift_R");

				if (type == 0) { // Key Down
					if (is_control) {
						for (const auto& [value, ptr] : g_engines) {
							if (ptr && ptr->GetActive() && (ptr->GetKeyFlags() & Sral::HANDLE_INTERRUPT)) {
								(void)ptr->StopSpeech();
							}
						}
					}
					else if (is_shift && !g_shiftPressed.load(std::memory_order_acquire)) {
						g_shiftPressed.store(true, std::memory_order_release);
						for (const auto& [value, ptr] : g_engines) {
							if (ptr && ptr->GetActive() && (ptr->GetKeyFlags() & Sral::HANDLE_PAUSE_RESUME)) {
								if (ptr->paused) {
									(void)ptr->ResumeSpeech();
								} else {
									(void)ptr->PauseSpeech();
								}
							}
						}
					}
				}
				else if (type == 1) { // Key Up
					if (is_shift) {
						g_shiftPressed.store(false, std::memory_order_release);
					}
				}
			}
		}
	}
	return DBUS_HANDLER_RESULT_HANDLED;
}
#endif

static bool PlatformRegisterKeyboardHooks(void) {
	if (g_keyboardHookThread.load(std::memory_order_acquire)) {
		return true;
	}
	g_keyboardHookThread.store(true, std::memory_order_release);

#if defined(_WIN32)
	g_hookThread = std::thread([]() {
		g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, GetModuleHandleW(nullptr), 0);
		if (!g_keyboardHook) {
			g_keyboardHookThread.store(false, std::memory_order_release);
			return;
		}

		MSG msg;
		while (g_keyboardHookThread.load(std::memory_order_acquire)) {
			if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
			else {
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
			}
		}
		(void)UnhookWindowsHookEx(g_keyboardHook);
		g_keyboardHook = nullptr;
	});

	uint64_t attempts = 0;
	while (g_keyboardHook == nullptr && attempts++ < 1500) {
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	return (g_keyboardHook != nullptr);

#elif defined(__linux__) && !defined(__ANDROID__)
	g_hookThread = std::thread([]() {
		DBusError err;
		dbus_error_init(&err);

		DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
		if (dbus_error_is_set(&err) || !conn) {
			dbus_error_free(&err);
			g_keyboardHookThread.store(false, std::memory_order_release);
			return;
		}

		dbus_bus_add_match(
			conn, "type='signal',interface='org.a11y.atspi.DeviceEventController',member='DeviceEvent'", &err);
		dbus_connection_flush(conn);
		if (dbus_error_is_set(&err)) {
			dbus_error_free(&err);
			dbus_connection_unref(conn);
			g_keyboardHookThread.store(false, std::memory_order_release);
			return;
		}

		if (!dbus_connection_add_filter(conn, ProcessAtSpiKeyEvent, nullptr, nullptr)) {
			dbus_connection_unref(conn);
			g_keyboardHookThread.store(false, std::memory_order_release);
			return;
		}

		while (g_keyboardHookThread.load(std::memory_order_acquire)) {
			(void)dbus_connection_read_write_dispatch(conn, 5);
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}

		dbus_connection_remove_filter(conn, ProcessAtSpiKeyEvent, nullptr);
		dbus_connection_unref(conn);
	});
	return true;
#else
	return true;
#endif
}

static void PlatformUnregisterKeyboardHooks(void) {
	if (!g_keyboardHookThread.load(std::memory_order_acquire)) {
		return;
	}
	g_keyboardHookThread.store(false, std::memory_order_release);
	if (g_hookThread.joinable()) {
		g_hookThread.join();
	}
}

extern "C" {

SRAL_API bool SRAL_RegisterKeyboardHooks(void) {
	if (!SRAL_IsInitialized()) {
		return false;
	}
	return PlatformRegisterKeyboardHooks();
}

SRAL_API void SRAL_UnregisterKeyboardHooks(void) {
	if (!SRAL_IsInitialized()) {
		return;
	}
	PlatformUnregisterKeyboardHooks();
}

} // extern "C"
