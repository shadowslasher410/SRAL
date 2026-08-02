#ifndef SRAL_BUILD_DLL
#define SRAL_BUILD_DLL 1
#endif

#include "SRAL.h"

#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "Engine.h"

#if defined(_WIN32)
#include <windows.h>

#include <tlhelp32.h>
#elif defined(__linux__) && !defined(__ANDROID__)
#include <dbus/dbus.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define BS_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define BS_LIKELY(x) __builtin_expect(!!(x), 1)
#else
#define BS_UNLIKELY(x) (x)
#define BS_LIKELY(x) (x)
#endif

namespace Sral {
class Nvda;
class Jaws;
class Zdsr;
class Uia;
class Sapi;
class VoiceOver;
class AvSpeech;
class NSSpeech;
class AndroidAccessibilityManager;
class AndroidTextToSpeech;
class ChromeVox;
class Orca;
class ACAnnouncer;

static void output_thread_loop();
static inline size_t GetEngineLookupIndex(int engineBitmask) noexcept;
std::shared_ptr<Sral::Engine> get_engine_internal(int engine) noexcept;
void PlatformUnregisterKeyboardHooks(void);
bool PlatformRegisterKeyboardHooks(void);

static constexpr size_t MAX_ENGINE_BIT_INDEX = 32;

alignas(128) std::atomic<std::shared_ptr<Sral::Engine>> g_currentEngine{nullptr};
alignas(128) std::array<std::atomic<std::shared_ptr<Sral::Engine>>, MAX_ENGINE_BIT_INDEX> g_enginesLookup{};
alignas(128) std::map<int, std::shared_ptr<Sral::Engine>> g_engines;

alignas(128) std::atomic<bool> g_initialized{false};
alignas(128) std::atomic<int> g_excludes{0};
alignas(128) std::atomic<int> g_enginesFailedToInitialize{0};
alignas(128) std::atomic<bool> g_keyboardHookThread{false};
alignas(128) std::atomic<bool> g_shiftPressed{false};

#if defined(_WIN32)
alignas(128) std::atomic<HHOOK> g_keyboardHook{nullptr};
alignas(128) std::atomic<DWORD> g_hookThreadId{0};
#endif

alignas(128) std::thread g_hookThread;
alignas(128) std::thread g_outputThread;
alignas(128) std::atomic<bool> g_outputThreadRunning{false};
alignas(128) std::atomic<bool> g_delayOperation{false};
alignas(128) std::atomic<uint64_t> g_lastDelayTime{0};

struct QueuedOutput {
	std::string text;
	bool interrupt = false;
	bool braille = false;
	bool speak = false;
	bool ssml = false;
	int time = 0;
	std::shared_ptr<Sral::Engine> engine;
};

alignas(128) std::vector<QueuedOutput> g_delayedOutputs;
alignas(128) std::mutex g_delayedOutputsMutex;
alignas(128) std::condition_variable g_delayedOutputsCV;
alignas(128) std::mutex g_sralEngineMutex;
alignas(128) std::mutex g_lifecycle_mutex;

#if defined(_WIN32)
static inline bool is_kernel_handle_valid(HANDLE handle) noexcept {
	return (handle != INVALID_HANDLE_VALUE && handle != nullptr);
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
	if (!name) [[unlikely]]
		return FALSE;
	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (!is_kernel_handle_valid(hProcessSnap)) [[unlikely]]
		return FALSE;

	PROCESSENTRY32W pe32;
	std::memset(&pe32, 0, sizeof(PROCESSENTRY32W));
	pe32.dwSize = sizeof(PROCESSENTRY32W);

	if (!Process32FirstW(hProcessSnap, &pe32)) {
		CloseHandle(hProcessSnap);
		return FALSE;
	}

	do {
		if (_wcsicmp(pe32.szExeFile, name) == 0) {
			CloseHandle(hProcessSnap);
			return TRUE;
		}
	} while (Process32NextW(hProcessSnap, &pe32));

	CloseHandle(hProcessSnap);
	return FALSE;
}
#endif

static inline size_t GetEngineLookupIndex(int engineBitmask) noexcept {
	if (engineBitmask <= 0) [[unlikely]]
		return 0;
	return static_cast<size_t>(std::countr_zero(static_cast<unsigned int>(engineBitmask)));
}

std::shared_ptr<Sral::Engine> get_engine_internal(int engine) noexcept {
	const size_t index = GetEngineLookupIndex(engine);
	if (index < MAX_ENGINE_BIT_INDEX) [[likely]] {
		return g_enginesLookup[index].load(std::memory_order_acquire);
	}
	return nullptr;
}

static void output_thread_loop() {
	g_outputThreadRunning.store(true, std::memory_order_release);

#if defined(__ANDROID__)
	JNIEnv* local_env = nullptr;
	JavaVM* jvm = Sral::GetAndroidJavaVM();
	bool attached_here = false;
	if (jvm) {
		if (jvm->GetEnv(reinterpret_cast<void**>(&local_env), JNI_VERSION_1_6) == JNI_EDETACHED) {
			if (jvm->AttachCurrentThread(&local_env, nullptr) == JNI_OK) {
				attached_here = true;
			}
		}
	}
#endif

	size_t consumed_index = 0;

	while (g_delayOperation.load(std::memory_order_acquire)) {
		QueuedOutput current_output;
		{
			std::unique_lock<std::mutex> lock(g_delayedOutputsMutex);
			g_delayedOutputsCV.wait(lock, [&consumed_index] {
				return !g_delayOperation.load(std::memory_order_relaxed) || (g_delayedOutputs.size() > consumed_index);
			});

			if (!g_delayOperation.load(std::memory_order_relaxed) || (g_delayedOutputs.size() <= consumed_index))
				[[unlikely]] {
				break;
			}

			current_output = std::move(g_delayedOutputs[consumed_index]);
			consumed_index++;

			if (consumed_index == g_delayedOutputs.size()) {
				g_delayedOutputs.clear();
				consumed_index = 0;
			}
		}

		if (current_output.time > 0) {
			std::unique_lock<std::mutex> lock(g_delayedOutputsMutex);
			const auto target_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(current_output.time);

			g_delayedOutputsCV.wait_until(
				lock, target_time, [] { return !g_delayOperation.load(std::memory_order_relaxed); });
		}

		if (!g_delayOperation.load(std::memory_order_relaxed)) [[unlikely]] {
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

	{
		std::lock_guard<std::mutex> lock(g_delayedOutputsMutex);
		g_delayedOutputs.clear();
	}

#if defined(__ANDROID__)
	if (jvm && attached_here) {
		jvm->DetachCurrentThread();
	}
#endif
	g_outputThreadRunning.store(false, std::memory_order_release);
}

void trigger_output_thread_safely() {
	std::lock_guard<std::mutex> lock(g_delayedOutputsMutex);
	g_delayOperation.store(true, std::memory_order_release);

	if (!g_outputThreadRunning.load(std::memory_order_acquire)) {
		if (g_outputThread.joinable()) {
			g_outputThread.detach();
		}
		g_outputThread = std::thread(output_thread_loop);
	}
	g_delayedOutputsCV.notify_one();
}

void speech_engine_update() noexcept {
	if (!g_initialized.load(std::memory_order_acquire)) [[unlikely]]
		return;

	std::shared_ptr<Sral::Engine> current = g_currentEngine.load(std::memory_order_acquire);
	const int category = current ? current->GetCategory() : static_cast<int>(SRAL_ENGINE_CATEGORY_UNKNOWN);

	if (!current || !current->GetActive() || category == SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE ||
		category == SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER) {

		bool narratorActive = false;

#if defined(_WIN32) && !defined(SRAL_NO_UIA)
		narratorActive = (IsNarratorRunningFast() == TRUE);

		if (!narratorActive) {
			static std::atomic<ULONGLONG> s_lastSnapTime{0};
			const ULONGLONG now = ::GetTickCount64();
			const ULONGLONG last = s_lastSnapTime.load(std::memory_order_relaxed);

			if (now - last >= 500) {
				s_lastSnapTime.store(now, std::memory_order_relaxed);
				if (FindProcess(L"narrator.exe") == TRUE) {
					narratorActive = true;
				}
			}
			else {
				if (current && current->GetNumber() == SRAL_ENGINE_UIA) {
					narratorActive = true;
				}
			}
		}
#endif

		if (narratorActive) {
#if defined(_WIN32) && !defined(SRAL_NO_UIA)
			std::shared_ptr<Sral::Engine> targetUia = get_engine_internal(SRAL_ENGINE_UIA);
			if (current.get() != targetUia.get()) {
				g_currentEngine.store(targetUia, std::memory_order_release);
			}
			return;
#endif
		}
		else {
			std::shared_ptr<Sral::Engine> nextEngine = nullptr;
			const int currentExcludes = g_excludes.load(std::memory_order_relaxed);

			std::lock_guard<std::mutex> lock(g_sralEngineMutex);
			for (const auto& [value, ptr] : g_engines) {
				if (ptr && ptr->GetActive() && !(currentExcludes & static_cast<int>(value))) {
					nextEngine = ptr;
					break;
				}
			}

			if (current.get() != nextEngine.get()) {
				g_currentEngine.store(nextEngine, std::memory_order_release);
			}
		}
	}
}

#if defined(_WIN32)
static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode >= 0) {
		const KBDLLHOOKSTRUCT* const pKeyInfo = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
		if (pKeyInfo) [[likely]] {
			const bool is_control = (pKeyInfo->vkCode == VK_LCONTROL || pKeyInfo->vkCode == VK_RCONTROL);
			const bool is_shift = (pKeyInfo->vkCode == VK_LSHIFT || pKeyInfo->vkCode == VK_RSHIFT);

			if (wParam == WM_KEYDOWN) {
				if (is_control) {
					for (size_t i = 0; i < MAX_ENGINE_BIT_INDEX; ++i) {
						std::shared_ptr<Sral::Engine> ptr = g_enginesLookup[i].load(std::memory_order_acquire);
						if (ptr && ptr->GetActive() && (ptr->GetKeyFlags() & Sral::HANDLE_INTERRUPT)) {
							(void)ptr->StopSpeech();
						}
					}
				}
				else if (is_shift && !g_shiftPressed.load(std::memory_order_acquire)) {
					g_shiftPressed.store(true, std::memory_order_release);
					for (size_t i = 0; i < MAX_ENGINE_BIT_INDEX; ++i) {
						std::shared_ptr<Sral::Engine> ptr = g_enginesLookup[i].load(std::memory_order_acquire);
						if (ptr && ptr->GetActive() && (ptr->GetKeyFlags() & Sral::HANDLE_PAUSE_RESUME)) {
							int is_paused = 0;
							if (ptr->GetParameter(SRAL_PARAM_ENGINE_IS_PAUSED, &is_paused) && is_paused) {
								(void)ptr->ResumeSpeech();
							}
							else {
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
	return CallNextHookEx(g_keyboardHook.load(std::memory_order_relaxed), nCode, wParam, lParam);
}
#endif

#if defined(__linux__) && !defined(__ANDROID__)
static DBusHandlerResult ProcessAtSpiKeyEvent(DBusConnection* conn, DBusMessage* msg, void* user_data) {
	(void)conn;
	(void)user_data;
	if (dbus_message_is_signal(msg, "org.a11y.atspi.DeviceEventController", "DeviceEvent")) {
		DBusMessageIter iter;
		if (dbus_message_iter_init(msg, &iter)) {
			dbus_uint32_t type = 0;
			if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_UINT32) {
				dbus_message_iter_get_basic(&iter, &type);
			}
		}
	}
	return DBUS_HANDLER_RESULT_HANDLED;
}
#endif

void PlatformUnregisterKeyboardHooks(void) {
#if defined(_WIN32)
	const DWORD threadId = g_hookThreadId.exchange(0, std::memory_order_acq_rel);
	if (threadId != 0) {
		if (::GetCurrentThreadId() != threadId) {
			::PostThreadMessageW(threadId, WM_QUIT, 0, 0);
			if (g_hookThread.joinable()) {
				g_hookThread.join();
			}
		}
		else {
			g_hookThread.detach();
		}
	}
#elif defined(__linux__) && !defined(__ANDROID__)
	if (g_hookThread.joinable()) {
		g_hookThread.join();
	}
#endif
}

bool PlatformRegisterKeyboardHooks(void) {
	bool expected = false;
	if (!g_keyboardHookThread.compare_exchange_strong(
			expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
		return true;
	}

#if defined(_WIN32)
	g_hookThread = std::thread([]() {
		g_hookThreadId.store(::GetCurrentThreadId(), std::memory_order_release);
		HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, GetModuleHandleW(nullptr), 0);
		g_keyboardHook.store(hook, std::memory_order_release);
		if (!hook) {
			g_keyboardHookThread.store(false, std::memory_order_release);
			return;
		}

		MSG msg;
		while (GetMessageW(&msg, nullptr, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		HHOOK active_hook = g_keyboardHook.exchange(nullptr, std::memory_order_acq_rel);
		if (active_hook)
			(void)UnhookWindowsHookEx(active_hook);
	});

	uint64_t attempts = 0;
	while (g_keyboardHook.load(std::memory_order_acquire) == nullptr && attempts++ < 1500) {
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	return (g_keyboardHook.load(std::memory_order_acquire) != nullptr);

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
		const char* match_rule = "type='signal',interface='org.a11y.atspi.DeviceEventController',member='DeviceEvent'";
		dbus_bus_add_match(conn, match_rule, &err);
		dbus_connection_flush(conn);
		if (!dbus_connection_add_filter(conn, ProcessAtSpiKeyEvent, nullptr, nullptr)) {
			g_keyboardHookThread.store(false, std::memory_order_release);
			return;
		}
		while (g_keyboardHookThread.load(std::memory_order_acquire)) {
			(void)dbus_connection_read_write_dispatch(conn, 50);
		}
	});
	return true;
#else
	return true;
#endif
}

static inline char* allocate_and_null_terminate(
	std::string_view view, char* stack_buffer, size_t stack_capacity, bool& out_is_heap_allocated) noexcept {
	const size_t required_size = view.size();
	if (required_size < stack_capacity) [[likely]] {
		out_is_heap_allocated = false;
		std::memcpy(stack_buffer, view.data(), required_size);
		stack_buffer[required_size] = '\0';
		return stack_buffer;
	}

	out_is_heap_allocated = true;
	char* heap_buffer = static_cast<char*>(::SRAL_malloc(required_size + 1));
	if (!heap_buffer) [[unlikely]] {
		return nullptr;
	}

	std::memcpy(heap_buffer, view.data(), required_size);
	heap_buffer[required_size] = '\0';
	return heap_buffer;
}

template <typename F> static inline bool ExecuteWithSafeAllocation(std::string_view text, F&& func) noexcept {
	std::array<char, 256> stack_cache;
	bool is_heap = false;
	char* c_str = allocate_and_null_terminate(text, stack_cache.data(), stack_cache.size(), is_heap);
	if (!c_str) [[unlikely]] {
		return false;
	}

	const bool result = func(c_str);
	if (is_heap) [[unlikely]] {
		::SRAL_free(c_str);
	}
	return result;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
bool SafeSpeakAllocationBridge(std::string_view text, bool interrupt) noexcept {
	return ExecuteWithSafeAllocation(text, [interrupt](const char* s) noexcept { return ::SRAL_Speak(s, interrupt); });
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
bool SafeSpeakSsmlAllocationBridge(std::string_view ssml, bool interrupt) noexcept {
	return ExecuteWithSafeAllocation(
		ssml, [interrupt](const char* s) noexcept { return ::SRAL_SpeakSsml(s, interrupt); });
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
bool SafeBrailleAllocationBridge(std::string_view text) noexcept {
	return ExecuteWithSafeAllocation(text, [](const char* s) noexcept { return ::SRAL_Braille(s); });
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
bool SafeOutputAllocationBridge(std::string_view text, bool interrupt) noexcept {
	return ExecuteWithSafeAllocation(text, [interrupt](const char* s) noexcept { return ::SRAL_Output(s, interrupt); });
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
bool SafeSpeakExAllocationBridge(SRAL_Engines engine, std::string_view text, bool interrupt) noexcept {
	return ExecuteWithSafeAllocation(text,
		[engine, interrupt](const char* s) noexcept { return ::SRAL_SpeakEx(static_cast<int>(engine), s, interrupt); });
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
bool SafeSpeakSsmlExAllocationBridge(SRAL_Engines engine, std::string_view ssml, bool interrupt) noexcept {
	return ExecuteWithSafeAllocation(ssml, [engine, interrupt](const char* s) noexcept {
		return ::SRAL_SpeakSsmlEx(static_cast<int>(engine), s, interrupt);
	});
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
bool SafeBrailleExAllocationBridge(SRAL_Engines engine, std::string_view text) noexcept {
	return ExecuteWithSafeAllocation(
		text, [engine](const char* s) noexcept { return ::SRAL_BrailleEx(static_cast<int>(engine), s); });
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
bool SafeOutputExAllocationBridge(SRAL_Engines engine, std::string_view text, bool interrupt) noexcept {
	return ExecuteWithSafeAllocation(text, [engine, interrupt](const char* s) noexcept {
		return ::SRAL_OutputEx(static_cast<int>(engine), s, interrupt);
	});
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
bool SafeDelayOutputAllocationBridge(int time, std::string_view text, bool interrupt) noexcept {
	return ExecuteWithSafeAllocation(
		text, [time, interrupt](const char* s) noexcept { return ::SRAL_DelayOutput(time, s, interrupt); });
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
bool SafeDelayOutputExAllocationBridge(SRAL_Engines engine, int time, std::string_view text, bool interrupt) noexcept {
	return ExecuteWithSafeAllocation(text, [engine, time, interrupt](const char* s) noexcept {
		return ::SRAL_DelayOutputEx(static_cast<int>(engine), time, s, interrupt);
	});
}

PCMBuffer DirectMemoryBridge(const char* text) noexcept {
	uint64_t buffer_size = 0;
	int channels = 0;
	int sample_rate = 0;
	int bits_per_sample = 0;

	void* raw_buffer = ::SRAL_SpeakToMemory(text, &buffer_size, &channels, &sample_rate, &bits_per_sample);
	if (!raw_buffer) [[unlikely]] {
		return PCMBuffer{};
	}

	PCMBuffer buffer;
	buffer.data = std::span<uint8_t>(static_cast<uint8_t*>(raw_buffer), static_cast<size_t>(buffer_size));
	buffer.channels = channels;
	buffer.sample_rate = sample_rate;
	buffer.bits_per_sample = bits_per_sample;
	return buffer;
}

PCMBuffer DirectMemoryExBridge(SRAL_Engines engine, const char* text) noexcept {
	uint64_t buffer_size = 0;
	int channels = 0;
	int sample_rate = 0;
	int bits_per_sample = 0;

	void* raw_buffer =
		::SRAL_SpeakToMemoryEx(static_cast<int>(engine), text, &buffer_size, &channels, &sample_rate, &bits_per_sample);
	if (!raw_buffer) [[unlikely]] {
		return PCMBuffer{};
	}

	PCMBuffer buffer;
	buffer.data = std::span<uint8_t>(static_cast<uint8_t*>(raw_buffer), static_cast<size_t>(buffer_size));
	buffer.channels = channels;
	buffer.sample_rate = sample_rate;
	buffer.bits_per_sample = bits_per_sample;
	return buffer;
}

std::string_view GetEngineNameFastBridge(SRAL_Engines engine) noexcept {
	const char* name = ::SRAL_GetEngineName(static_cast<int>(engine));
	return name ? std::string_view(name) : std::string_view("");
}

} // namespace Sral

extern "C" {

void* SRAL_malloc(size_t size) noexcept {
	if (size == 0) [[unlikely]]
		return nullptr;
	return ::malloc(size);
}

void SRAL_free(void* memory) noexcept {
	if (memory) [[likely]]
		::free(memory);
}

bool SRAL_IsInitialized(void) noexcept {
	return Sral::g_initialized.load(std::memory_order_acquire) && !Sral::g_engines.empty();
}

bool SRAL_Initialize(int engines_exclude) noexcept {
	std::lock_guard<std::mutex> lock(Sral::g_sralEngineMutex);
	if (Sral::g_initialized.load(std::memory_order_acquire))
		return true;

#if defined(SRAL_WITH_ACCESSKIT)
	Sral::g_engines[SRAL_ENGINE_ACCESSKIT] = std::make_shared<Sral::ACAnnouncer>();
#endif

#if defined(_WIN32)
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
		return false;
#ifndef SRAL_NO_NVDA
	Sral::g_engines[SRAL_ENGINE_NVDA] == std::make_shared<Sral::Nvda>();
#endif
#ifndef SRAL_NO_JAWS
	Sral::g_engines[SRAL_ENGINE_JAWS] == std::make_shared<Sral::Jaws>();
#endif
#ifndef SRAL_NO_ZDSR
	Sral::g_engines[SRAL_ENGINE_ZDSR] == std::make_shared<Sral::Zdsr>();
#endif
#ifndef SRAL_NO_UIA
	Sral::g_engines[SRAL_ENGINE_UIA] == std::make_shared<Sral::Uia>();
#endif
#ifndef SRAL_NO_SAPI
	Sral::g_engines[SRAL_ENGINE_SAPI] == std::make_shared<Sral::Sapi>();
#endif

#elif defined(__APPLE__)
	Sral::g_engines[SRAL_ENGINE_VOICE_OVER] = std::make_shared<Sral::VoiceOver>();
	Sral::g_engines[SRAL_ENGINE_AV_SPEECH] = std::make_shared<Sral::AvSpeech>();
#ifndef SRAL_NO_NSSPEECH
	Sral::g_engines[SRAL_ENGINE_NS_SPEECH] = std::make_shared<Sral::NSSpeech>();
#endif

#elif defined(__ANDROID__)
#ifndef SRAL_NO_ANDROID_ACCESSIBILITY
	Sral::g_engines[SRAL_ENGINE_ANDROID_ACCESSIBILITY_MANAGER] = std::make_shared<Sral::AndroidAccessibilityManager>();
#endif
#ifndef SRAL_NO_ANDROID_TTS
	Sral::g_engines[SRAL_ENGINE_ANDROID_TEXT_TO_SPEECH] = std::make_shared<Sral::AndroidTextToSpeech>();
#endif

#elif defined(__EMSCRIPTEN__)
#ifndef SRAL_NO_CHROMEVOX
	Sral::g_engines[SRAL_ENGINE_CHROMEVOX] = std::make_shared<Sral::ChromeVox>();
#endif

#else
#ifndef SRAL_NO_SPEECH_DISPATCHER
	Sral::g_engines[SRAL_ENGINE_SPEECH_DISPATCHER] = std::make_shared<Sral::SpeechDispatcher>();
#endif
#ifndef SRAL_NO_ORCA
	Sral::g_engines[SRAL_ENGINE_ORCA] = std::make_shared<Sral::Orca>();
#endif
#ifndef SRAL_NO_CHROMEVOX
	Sral::g_engines[SRAL_ENGINE_CHROMEVOX] = std::make_shared<Sral::ChromeVox>();
#endif
#endif

	bool success = false;
	for (const auto& [value, ptr] : Sral::g_engines) {
		if (!ptr) [[unlikely]]
			continue;
		if (!ptr->Initialize()) {
			Sral::g_enginesFailedToInitialize.fetch_or(static_cast<int>(value));
		}
		else {
			success = true;
			const size_t lookIndex = Sral::GetEngineLookupIndex(static_cast<int>(value));
			if (lookIndex < Sral::MAX_ENGINE_BIT_INDEX) {
				Sral::g_enginesLookup[lookIndex].store(ptr, std::memory_order_release);
			}
		}
	}
	Sral::g_initialized.store(success, std::memory_order_release);
	if (!Sral::g_initialized.load(std::memory_order_acquire)) {
		for (const auto& [value, ptr] : Sral::g_engines) {
			if (ptr)
				(void)ptr->Uninitialize();
		}
		Sral::g_engines.clear();

		for (size_t i = 0; i < Sral::MAX_ENGINE_BIT_INDEX; ++i) {
			Sral::g_enginesLookup[i].store(nullptr, std::memory_order_relaxed);
		}

#if defined(_WIN32)
		CoUninitialize();
#endif
		return false;
	}
	Sral::g_excludes.store(engines_exclude, std::memory_order_release);
	return true;
}

void SRAL_Uninitialize(void) noexcept {
	std::lock_guard<std::mutex> lock(Sral::g_sralEngineMutex);
	if (!Sral::g_initialized.load(std::memory_order_acquire))
		return;

#if defined(_WIN32) || (defined(__linux__) && !defined(__ANDROID__)) || (defined(__APPLE__) && !TARGET_OS_IPHONE)
	if (Sral::g_keyboardHookThread.load(std::memory_order_acquire)) {
		Sral::PlatformUnregisterKeyboardHooks();
	}
#endif

	Sral::g_delayOperation.store(false, std::memory_order_release);
	Sral::g_delayedOutputsCV.notify_all();
	if (Sral::g_outputThread.joinable()) {
		Sral::g_outputThread.join();
	}

	for (const auto& [value, ptr] : Sral::g_engines) {
		if (ptr)
			(void)ptr->Uninitialize();
	}

#if defined(_WIN32)
	CoUninitialize();
#endif
#if defined(__ANDROID__)
	Sral::ClearAndroidContext();
#endif

	Sral::g_currentEngine.store(nullptr, std::memory_order_release);
	for (size_t i = 0; i < Sral::MAX_ENGINE_BIT_INDEX; ++i) {
		Sral::g_enginesLookup[i].store(nullptr, std::memory_order_release);
	}
	Sral::g_engines.clear();
	Sral::g_excludes.store(SRAL_ENGINE_NONE, std::memory_order_release);
	Sral::g_enginesFailedToInitialize.store(SRAL_ENGINE_NONE, std::memory_order_release);
	Sral::g_initialized.store(false, std::memory_order_release);
}

bool SRAL_Speak(const char* text, bool interrupt) noexcept {
	Sral::speech_engine_update();
	if (std::shared_ptr<Sral::Engine> active = Sral::g_currentEngine.load(std::memory_order_acquire)) [[likely]] {
		return active->Speak(text ? text : "", interrupt);
	}
	return false;
}

bool SRAL_DelayOutput(int time, const char* text, bool interrupt) noexcept {
	Sral::speech_engine_update();
	if (std::shared_ptr<Sral::Engine> active = Sral::g_currentEngine.load(std::memory_order_acquire)) [[likely]] {
		if (time < 0 || !SRAL_IsInitialized()) [[unlikely]]
			return false;

		Sral::QueuedOutput qout{.text = std::string(text ? text : ""),
			.interrupt = interrupt,
			.braille = false,
			.speak = true,
			.ssml = false,
			.time = time,
			.engine = active};

		{
			std::lock_guard<std::mutex> lock(Sral::g_delayedOutputsMutex);
			Sral::g_delayedOutputs.push_back(std::move(qout));
		}
		Sral::trigger_output_thread_safely();
		return true;
	}
	return false;
}

bool SRAL_DelayOutputEx(int engine, int time, const char* text, bool interrupt) noexcept {
	if (time < 0 || !SRAL_IsInitialized()) [[unlikely]]
		return false;
	std::shared_ptr<Sral::Engine> e = Sral::get_engine_internal(engine);
	if (!e) [[unlikely]]
		return false;

	Sral::QueuedOutput qout{.text = std::string(text ? text : ""),
		.interrupt = interrupt,
		.braille = false,
		.speak = true,
		.ssml = false,
		.time = time,
		.engine = e};

	{
		std::lock_guard<std::mutex> lock(Sral::g_delayedOutputsMutex);
		Sral::g_delayedOutputs.push_back(std::move(qout));
	}
	Sral::trigger_output_thread_safely();
	return true;
}

void* SRAL_SpeakToMemory(
	const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample) noexcept {

	Sral::speech_engine_update();
	if (buffer_size != nullptr) {
		*buffer_size = 0;
	}
	if (channels != nullptr) {
		*channels = 0;
	}
	if (sample_rate != nullptr) {
		*sample_rate = 0;
	}
	if (bits_per_sample != nullptr) {
		*bits_per_sample = 0;
	}

	if (std::shared_ptr<Sral::Engine> active = Sral::g_currentEngine.load(std::memory_order_acquire)) [[likely]] {
		return active->SpeakToMemory(text ? text : "", buffer_size, channels, sample_rate, bits_per_sample);
	}
	return nullptr;
}

bool SRAL_SpeakSsml(const char* ssml, bool interrupt) noexcept {
	Sral::speech_engine_update();
	if (std::shared_ptr<Sral::Engine> active = Sral::g_currentEngine.load(std::memory_order_acquire)) [[likely]] {
		return active->SpeakSsml(ssml ? ssml : "", interrupt);
	}
	return false;
}

bool SRAL_Braille(const char* text) noexcept {
	Sral::speech_engine_update();
	if (std::shared_ptr<Sral::Engine> active = Sral::g_currentEngine.load(std::memory_order_acquire)) [[likely]] {
		return active->Braille(text ? text : "");
	}
	return false;
}

bool SRAL_Output(const char* text, bool interrupt) noexcept {
	Sral::speech_engine_update();
	if (std::shared_ptr<Sral::Engine> active = Sral::g_currentEngine.load(std::memory_order_acquire)) [[likely]] {
		const bool speech = active->Speak(text ? text : "", interrupt);
		const bool braille = active->Braille(text ? text : "");
		return speech || braille;
	}
	return false;
}

bool SRAL_StopSpeech(void) noexcept {
	Sral::speech_engine_update();
	if (std::shared_ptr<Sral::Engine> active = Sral::g_currentEngine.load(std::memory_order_acquire)) [[likely]] {
		if (Sral::g_delayOperation.exchange(false, std::memory_order_acq_rel)) {
			{
				std::lock_guard<std::mutex> lock(Sral::g_delayedOutputsMutex);
				Sral::g_delayedOutputs.clear();
			}
			Sral::g_delayedOutputsCV.notify_all();
		}
		return active->StopSpeech();
	}
	return false;
}

bool SRAL_PauseSpeech(void) noexcept {
	Sral::speech_engine_update();
	if (std::shared_ptr<Sral::Engine> active = Sral::g_currentEngine.load(std::memory_order_acquire)) [[likely]] {
		if (Sral::g_delayOperation.exchange(false, std::memory_order_acq_rel)) {
			Sral::g_delayedOutputsCV.notify_all();
		}
		return active->PauseSpeech();
	}
	return false;
}

bool SRAL_ResumeSpeech(void) noexcept {
	Sral::speech_engine_update();
	if (std::shared_ptr<Sral::Engine> active = Sral::g_currentEngine.load(std::memory_order_acquire)) [[likely]] {
		std::lock_guard<std::mutex> lock(Sral::g_delayedOutputsMutex);
		if (!Sral::g_delayedOutputs.empty()) {
			Sral::g_delayOperation.store(true, std::memory_order_release);
			Sral::trigger_output_thread_safely();
		}
		return active->ResumeSpeech();
	}
	return false;
}

bool SRAL_IsSpeaking(void) noexcept {
	Sral::speech_engine_update();
	if (std::shared_ptr<Sral::Engine> active = Sral::g_currentEngine.load(std::memory_order_acquire)) [[likely]] {
		return active->IsSpeaking();
	}
	return false;
}

int SRAL_GetCurrentEngine(void) noexcept {
	Sral::speech_engine_update();
	if (std::shared_ptr<Sral::Engine> active = Sral::g_currentEngine.load(std::memory_order_acquire)) [[likely]] {
		return active->GetNumber();
	}
	return SRAL_ENGINE_NONE;
}

int SRAL_GetEngineFeatures(int engine) noexcept {
	if (engine == 0) {
		Sral::speech_engine_update();
		if (std::shared_ptr<Sral::Engine> active = Sral::g_currentEngine.load(std::memory_order_acquire)) [[likely]] {
			return active->GetFeatures();
		}
		return -1;
	}
	std::shared_ptr<Sral::Engine> e = Sral::get_engine_internal(engine);
	return e ? e->GetFeatures() : -1;
}

bool SRAL_SetEngineParameter(int engine, int param, const void* value) noexcept {
#if defined(__ANDROID__)
	if (param == SRAL_PARAM_ANDROID_JNI_ENV) {
		void* non_const_val = const_cast<void*>(value);
		return Sral::SetAndroidJNIEnv(static_cast<JNIEnv*>(non_const_val));
	}
	if (param == SRAL_PARAM_ANDROID_ACTIVITY) {
		void* non_const_val = const_cast<void*>(value);
		return Sral::SetAndroidActivity(static_cast<jobject>(non_const_val));
	}
#endif
	if (engine == 0) {
		std::shared_ptr<Sral::Engine> active = Sral::g_currentEngine.load(std::memory_order_acquire);
		return active ? active->SetParameter(param, value) : false;
	}
	std::shared_ptr<Sral::Engine> e = Sral::get_engine_internal(engine);
	return e ? e->SetParameter(param, value) : false;
}

bool SRAL_GetEngineParameter(int engine, int param, void* value) noexcept {
	if (engine == 0) {
		std::shared_ptr<Sral::Engine> active = Sral::g_currentEngine.load(std::memory_order_acquire);
		return active ? active->GetParameter(param, value) : false;
	}
	std::shared_ptr<Sral::Engine> e = Sral::get_engine_internal(engine);
	return e ? e->GetParameter(param, value) : false;
}

bool SRAL_SpeakEx(int engine, const char* text, bool interrupt) noexcept {
	std::shared_ptr<Sral::Engine> e = Sral::get_engine_internal(engine);
	if (BS_UNLIKELY(!e))
		return false;

	if (!Sral::g_delayOperation.load(std::memory_order_acquire)) {
		return e->Speak(text, interrupt);
	}

	Sral::QueuedOutput qout{.text = std::string(text ? text : ""),
		.interrupt = interrupt,
		.braille = false,
		.speak = true,
		.ssml = false,
		.time = static_cast<int>(Sral::g_lastDelayTime.load(std::memory_order_relaxed)),
		.engine = e};

	{
		std::lock_guard<std::mutex> lock(Sral::g_delayedOutputsMutex);
		Sral::g_delayedOutputs.push_back(std::move(qout));
	}
	Sral::trigger_output_thread_safely();
	return true;
}

void* SRAL_SpeakToMemoryEx(int engine,
	const char* text,
	uint64_t* buffer_size,
	int* channels,
	int* sample_rate,
	int* bits_per_sample) noexcept {
	std::shared_ptr<Sral::Engine> e = Sral::get_engine_internal(engine);
	return e ? e->SpeakToMemory(text, buffer_size, channels, sample_rate, bits_per_sample) : nullptr;
}

bool SRAL_SpeakSsmlEx(int engine, const char* ssml, bool interrupt) noexcept {
	std::shared_ptr<Sral::Engine> e = Sral::get_engine_internal(engine);
	if (BS_UNLIKELY(!e))
		return false;

	if (!Sral::g_delayOperation.load(std::memory_order_acquire)) {
		return e->SpeakSsml(ssml, interrupt);
	}

	Sral::QueuedOutput qout{.text = std::string(ssml ? ssml : ""),
		.interrupt = interrupt,
		.braille = false,
		.speak = true,
		.ssml = true,
		.time = static_cast<int>(Sral::g_lastDelayTime.load(std::memory_order_relaxed)),
		.engine = e};

	{
		std::lock_guard<std::mutex> lock(Sral::g_delayedOutputsMutex);
		Sral::g_delayedOutputs.push_back(std::move(qout));
	}
	Sral::trigger_output_thread_safely();
	return true;
}

bool SRAL_BrailleEx(int engine, const char* text) noexcept {
	std::shared_ptr<Sral::Engine> e = Sral::get_engine_internal(engine);
	if (BS_UNLIKELY(!e))
		return false;

	if (!Sral::g_delayOperation.load(std::memory_order_acquire)) {
		return e->Braille(text);
	}

	Sral::QueuedOutput qout{.text = std::string(text ? text : ""),
		.interrupt = false,
		.braille = true,
		.speak = false,
		.ssml = false,
		.time = static_cast<int>(Sral::g_lastDelayTime.load(std::memory_order_relaxed)),
		.engine = e};

	{
		std::lock_guard<std::mutex> lock(Sral::g_delayedOutputsMutex);
		Sral::g_delayedOutputs.push_back(std::move(qout));
	}
	Sral::trigger_output_thread_safely();
	return true;
}

bool SRAL_OutputEx(int engine, const char* text, bool interrupt) noexcept {
	std::shared_ptr<Sral::Engine> e = Sral::get_engine_internal(engine);
	if (BS_UNLIKELY(!e))
		return false;
	const bool speech = e->Speak(text, interrupt);
	const bool braille = e->Braille(text);
	return speech || braille;
}

bool SRAL_StopSpeechEx(int engine) noexcept {
	std::shared_ptr<Sral::Engine> e = Sral::get_engine_internal(engine);
	if (BS_UNLIKELY(!e))
		return false;

	if (Sral::g_delayOperation.exchange(false, std::memory_order_acq_rel)) {
		{
			std::lock_guard<std::mutex> lock(Sral::g_delayedOutputsMutex);
			Sral::g_delayedOutputs.clear();
		}
		Sral::g_delayedOutputsCV.notify_all();
	}
	return e->StopSpeech();
}

bool SRAL_PauseSpeechEx(int engine) noexcept {
	std::shared_ptr<Sral::Engine> e = Sral::get_engine_internal(engine);
	if (BS_UNLIKELY(!e))
		return false;

	if (Sral::g_delayOperation.exchange(false, std::memory_order_acq_rel)) {
		Sral::g_delayedOutputsCV.notify_all();
	}
	return e->PauseSpeech();
}

bool SRAL_ResumeSpeechEx(int engine) noexcept {
	std::shared_ptr<Sral::Engine> e = Sral::get_engine_internal(engine);
	if (BS_UNLIKELY(!e))
		return false;

	{
		std::lock_guard<std::mutex> lock(Sral::g_delayedOutputsMutex);
		if (!Sral::g_delayedOutputs.empty()) {
			Sral::g_delayOperation.store(true, std::memory_order_release);
			Sral::trigger_output_thread_safely();
		}
	}
	return e->ResumeSpeech();
}

bool SRAL_IsSpeakingEx(int engine) noexcept {
	std::shared_ptr<Sral::Engine> e = Sral::get_engine_internal(engine);
	return e ? e->IsSpeaking() : false;
}

void SRAL_Delay(int time) noexcept {
	if (!SRAL_IsInitialized() || time < 0) [[unlikely]]
		return;
	Sral::g_lastDelayTime.store(static_cast<uint64_t>(time), std::memory_order_relaxed);
	Sral::g_delayOperation.store(true, std::memory_order_release);
}

int SRAL_GetAvailableEngines(void) noexcept {
	if (!SRAL_IsInitialized()) [[unlikely]]
		return 0;
	int mask = 0;
	for (const auto& [value, ptr] : Sral::g_engines) {
		if (ptr) [[likely]]
			mask |= static_cast<int>(value);
	}
	return mask;
}

int SRAL_GetActiveEngines(void) noexcept {
	if (!SRAL_IsInitialized()) [[unlikely]]
		return 0;
	int mask = 0;
	for (const auto& [value, ptr] : Sral::g_engines) {
		if (ptr && ptr->GetActive())
			mask |= static_cast<int>(value);
	}
	return mask;
}

SRAL_EngineCategory SRAL_GetEngineCategory(int engine) noexcept {
	if (!SRAL_IsInitialized()) [[unlikely]]
		return SRAL_ENGINE_CATEGORY_UNKNOWN;
	std::shared_ptr<Sral::Engine> e = Sral::get_engine_internal(engine);
	return e ? static_cast<SRAL_EngineCategory>(e->GetCategory()) : SRAL_ENGINE_CATEGORY_UNKNOWN;
}

int SRAL_GetTTSEngines(void) noexcept {
	if (!SRAL_IsInitialized()) [[unlikely]]
		return 0;
	int mask = 0;
	for (const auto& [value, ptr] : Sral::g_engines) {
		if (ptr && ptr->GetCategory() == SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE) {
			mask |= static_cast<int>(value);
		}
	}
	return mask;
}

int SRAL_GetAssistiveTechEngines(void) noexcept {
	if (!SRAL_IsInitialized()) [[unlikely]]
		return 0;
	int mask = 0;
	for (const auto& [value, ptr] : Sral::g_engines) {
		if (!ptr) [[unlikely]]
			continue;
		const int category = ptr->GetCategory();
		if (category == SRAL_ENGINE_CATEGORY_SCREEN_READER || category == SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER) {
			mask |= static_cast<int>(value);
		}
	}
	return mask;
}

const char* SRAL_GetEngineName(int engine) noexcept {
	switch (static_cast<SRAL_Engines>(engine)) {
	case SRAL_ENGINE_NONE:
		return "None";
	case SRAL_ENGINE_NVDA:
		return "NVDA";
	case SRAL_ENGINE_SAPI:
		return "SAPI";
	case SRAL_ENGINE_JAWS:
		return "JAWS";
	case SRAL_ENGINE_SPEECH_DISPATCHER:
		return "Speech Dispatcher";
	case SRAL_ENGINE_UIA:
		return "UIA";
	case SRAL_ENGINE_AV_SPEECH:
		return "AV Speech";
	case SRAL_ENGINE_NS_SPEECH:
		return "NS Speech";
	case SRAL_ENGINE_NARRATOR:
		return "Narrator";
	case SRAL_ENGINE_VOICE_OVER:
		return "Voice Over";
	case SRAL_ENGINE_ZDSR:
		return "ZDSR";
	case SRAL_ENGINE_ANDROID_TEXT_TO_SPEECH:
		return "Android TTS";
	case SRAL_ENGINE_ANDROID_ACCESSIBILITY_MANAGER:
		return "Android AccessibilityManager";
	case SRAL_ENGINE_CHROMEVOX:
		return "ChromeVox";
	case SRAL_ENGINE_ORCA:
		return "Orca";
	case SRAL_ENGINE_ACCESSKIT:
		return "AccessKit";
	default:
		return "Unknown";
	}
}

bool SRAL_SetEnginesExclude(int engines_exclude) noexcept {
	if (!SRAL_IsInitialized()) [[unlikely]]
		return false;
	Sral::g_excludes.store(engines_exclude, std::memory_order_relaxed);
	Sral::speech_engine_update();
	return true;
}

int SRAL_GetEnginesExclude(void) noexcept {
	return SRAL_IsInitialized() ? Sral::g_excludes.load(std::memory_order_relaxed) : -1;
}

} // extern "C"