#if defined(_WIN32) || defined(_WIN64)

#include "ZDSR.h"
#include <windows.h>
#include <concepts>
#include <mutex>
#include <string_view>
#include <type_traits>

#include "Encoding.h"

namespace Sral {

template <typename T>
concept FunctionPointer = std::is_pointer_v<T> && std::is_function_v<std::remove_pointer_t<T>>;

template <FunctionPointer DestType> 
[[nodiscard]] constexpr DestType SafeProcCast(FARPROC src) noexcept {
	return reinterpret_cast<DestType>(src);
}

[[nodiscard]] static std::mutex& GetLoaderMutex() noexcept {
	static std::mutex mutex;
	return mutex;
}

Zdsr::~Zdsr() noexcept {
	HMODULE dummy = nullptr;
	(void)dummy;
	
	if (::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                         reinterpret_cast<LPCWSTR>(&GetLoaderMutex), &dummy) == 0) {
		(void)lib.release(); 
	} else {
		(void)Uninitialize();
	}
}

bool Zdsr::Initialize() {
	if (isInitialized.load(std::memory_order_acquire)) {
		return true;
	}

	std::lock_guard<std::mutex> lock(GetLoaderMutex());
	
	if (isInitialized.load(std::memory_order_relaxed)) {
		return true;
	}

	lib.reset(::LoadLibraryW(L"ZDSRAPI.dll"));
	if (!lib) {
		return false;
	}

	fInitTTS       = SafeProcCast<InitTTS_t>(::GetProcAddress(lib.get(), "InitTTS"));
	fSpeak         = SafeProcCast<Speak_t>(::GetProcAddress(lib.get(), "Speak"));
	fStopSpeak     = SafeProcCast<StopSpeak_t>(::GetProcAddress(lib.get(), "StopSpeak"));
	fGetSpeakState = SafeProcCast<GetSpeakState_t>(::GetProcAddress(lib.get(), "GetSpeakState"));

	if (!fInitTTS || !fSpeak || !fStopSpeak || !fGetSpeakState) [[unlikely]] {
		std::lock_guard<std::mutex> instanceLock(instanceMutex);
		CleanUpMembers();
		return false;
	}
	wchar_t emptyBuffer[512] = { L'\0' };
	if (fInitTTS(0, &emptyBuffer[0]) != 0) {
		std::lock_guard<std::mutex> instanceLock(instanceMutex);
		CleanUpMembers();
		return false;
	}

	isInitialized.store(true, std::memory_order_release);
	return true;
}

bool Zdsr::Uninitialize() {
	std::lock_guard<std::mutex> instanceLock(instanceMutex);

	if (fStopSpeak && isInitialized.load(std::memory_order_relaxed)) {
		(void)fStopSpeak();
	}

	std::lock_guard<std::mutex> loaderLock(GetLoaderMutex());
	if (!isInitialized.load(std::memory_order_relaxed)) {
		return true;
	}

	CleanUpMembers();
	return true;
}

void Zdsr::CleanUpMembers() noexcept {
	isInitialized.store(false, std::memory_order_release);

	std::atomic_thread_fence(std::memory_order_seq_cst);

	lib.reset();
	
	fInitTTS       = nullptr;
	fSpeak         = nullptr;
	fStopSpeak     = nullptr;
	fGetSpeakState = nullptr;
}

bool Zdsr::GetActive() {
	return isInitialized.load(std::memory_order_acquire);
}

bool Zdsr::Speak(const char* text, bool interrupt) {
	if (!text) [[unlikely]] {
		return false;
	}

	std::string_view textStr(text);
	if (textStr.empty()) {
		return false;
	}

	std::lock_guard<std::mutex> lock(instanceMutex);
	if (!isInitialized.load(std::memory_order_acquire) || !fSpeak) [[unlikely]] {
		return false;
	}

	std::wstring out;
	if (!UnicodeConvert(textStr, out) || out.empty()) {
		return false;
	}

	return fSpeak(out.c_str(), interrupt ? TRUE : FALSE) == 0;
}

bool Zdsr::StopSpeech() {
	std::lock_guard<std::mutex> lock(instanceMutex);
	if (!isInitialized.load(std::memory_order_acquire) || !fStopSpeak) {
		return false;
	}
	(void)fStopSpeak();
	return true;
}

bool Zdsr::IsSpeaking() {
	std::lock_guard<std::mutex> lock(instanceMutex);
	if (!isInitialized.load(std::memory_order_acquire) || !fGetSpeakState) {
		return false;
	}
	return fGetSpeakState() == 3;
}

} // namespace Sral

#endif /* defined(_WIN32) || defined(_WIN64) */
