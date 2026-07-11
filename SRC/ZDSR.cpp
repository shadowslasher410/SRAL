#include "ZDSR.h"
#include <concepts>
#include <chrono>
#include "Encoding.h"

namespace Sral {

void Zdsr::LibraryDeleter::operator()(HMODULE handle) const noexcept {
#if defined(_WIN32) || defined(_WIN64)
	if (handle) ::FreeLibrary(handle);
#endif
}

template <typename T>
concept FunctionPointer = std::is_pointer_v<T> && std::is_function_v<std::remove_pointer_t<T>>;

template <typename DestType>
    requires FunctionPointer<DestType>
[[nodiscard]] constexpr DestType SafeProcCast(FARPROC src) noexcept {
    return reinterpret_cast<DestType>(src);
}

[[nodiscard]] static std::mutex& GetLoaderMutex() noexcept {
	static std::mutex mutex;
	return mutex;
}

Zdsr::~Zdsr() noexcept {
#if defined(_WIN32) || defined(_WIN64)
	HMODULE dummy = nullptr;
	if (::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                         reinterpret_cast<LPCWSTR>(&GetLoaderMutex), &dummy) == 0) {
		(void)lib.release(); 
	} else {
		(void)Uninitialize();
	}
#else
	(void)Uninitialize();
#endif
}

bool Zdsr::Initialize() {
	if (m_running.load(std::memory_order_acquire)) return true;
	std::lock_guard<std::mutex> lock(GetLoaderMutex());
	if (m_running.load(std::memory_order_relaxed)) return true;

	m_running.store(true, std::memory_order_release);
	m_workerThread = std::thread(&Zdsr::BackgroundWorkerLoop, this);
	return true;
}

bool Zdsr::Uninitialize() {
	if (!m_running.load(std::memory_order_acquire)) return true;
	m_running.store(false, std::memory_order_release);
	m_cv.notify_all();

	if (m_workerThread.joinable()) {
		m_workerThread.join();
	}
	return true;
}

void Zdsr::CleanUpMembers() noexcept {
	isInitialized.store(false, std::memory_order_release);
	m_isSpeakingCache.store(false, std::memory_order_release);
	std::atomic_thread_fence(std::memory_order_seq_cst);
	lib.reset();
	fInitTTS = nullptr; fSpeak = nullptr; fStopSpeak = nullptr; fGetSpeakState = nullptr;
}

bool Zdsr::GetActive() { return isInitialized.load(std::memory_order_acquire); }
bool Zdsr::IsSpeaking() { return m_isSpeakingCache.load(std::memory_order_acquire); }

bool Zdsr::Speak(const char* text, bool interrupt) {
	if (!text || !m_running.load(std::memory_order_acquire)) [[unlikely]] return false;
	std::string_view textStr(text);
	if (textStr.empty()) return false;

	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		if (interrupt) {
			std::queue<ThreadCommand> empty;
			std::swap(m_commandQueue, empty);
			m_commandQueue.push(ThreadCommand{ CommandType::Stop, "", true });
		}
		m_commandQueue.push(ThreadCommand{ CommandType::Speak, std::string(textStr), interrupt });
	}
	m_cv.notify_one();
	return true;
}

bool Zdsr::StopSpeech() {
	if (!m_running.load(std::memory_order_acquire)) return false;
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		std::queue<ThreadCommand> empty;
		std::swap(m_commandQueue, empty);
		m_commandQueue.push(ThreadCommand{ CommandType::Stop, "", true });
	}
	m_cv.notify_one();
	return true;
}

void Zdsr::BackgroundWorkerLoop() noexcept {
#if defined(_WIN32) || defined(_WIN64)
	{
		std::lock_guard<std::mutex> instanceLock(instanceMutex);
		lib.reset(::LoadLibraryW(L"ZDSRAPI.dll"));
		if (lib) {
			fInitTTS       = SafeProcCast<InitTTS_t>(::GetProcAddress(lib.get(), "InitTTS"));
			fSpeak         = SafeProcCast<Speak_t>(::GetProcAddress(lib.get(), "Speak"));
			fStopSpeak     = SafeProcCast<StopSpeak_t>(::GetProcAddress(lib.get(), "StopSpeak"));
			fGetSpeakState = SafeProcCast<GetSpeakState_t>(::GetProcAddress(lib.get(), "GetSpeakState"));

			if (fInitTTS && fSpeak && fStopSpeak && fGetSpeakState) {
				wchar_t emptyBuffer = { L'\0' };
				if (fInitTTS(0, &emptyBuffer) == 0) {
					isInitialized.store(true, std::memory_order_release);
				}
			}
		}
		if (!isInitialized.load(std::memory_order_relaxed)) {
			CleanUpMembers();
			m_running.store(false, std::memory_order_release);
			return;
		}
	}

	while (m_running.load(std::memory_order_acquire)) {
		ThreadCommand cmd;
		bool hasCommand = false;
		{
			std::unique_lock<std::mutex> lock(m_queueMutex);
			if (!m_commandQueue.empty()) {
				cmd = std::move(m_commandQueue.front());
				m_commandQueue.pop();
				hasCommand = true;
			}
		}

		if (hasCommand) {
			std::lock_guard<std::mutex> instanceLock(instanceMutex);
			if (isInitialized.load(std::memory_order_relaxed)) {
				if (cmd.type == CommandType::Stop) {
					if (fStopSpeak) (void)fStopSpeak();
				} 
				else if (cmd.type == CommandType::Speak && fSpeak) {
					std::wstring broadString;
					if (UnicodeConvert(cmd.payload, broadString) && !broadString.empty()) {
						(void)fSpeak(broadString.c_str(), cmd.interrupt ? TRUE : FALSE);
					}
				}
			}
		}

		{
			std::lock_guard<std::mutex> instanceLock(instanceMutex);
			if (isInitialized.load(std::memory_order_relaxed) && fGetSpeakState) {
				m_isSpeakingCache.store(fGetSpeakState() == 3, std::memory_order_release);
			} else {
				m_isSpeakingCache.store(false, std::memory_order_release);
			}
		}

		if (!hasCommand) {
			std::unique_lock<std::mutex> lock(m_queueMutex);
			if (m_commandQueue.empty() && m_running.load(std::memory_order_acquire)) {
				m_cv.wait_for(lock, std::chrono::milliseconds(15));
			}
		}
	}

	{
		std::lock_guard<std::mutex> instanceLock(instanceMutex);
		if (fStopSpeak && isInitialized.load(std::memory_order_relaxed)) (void)fStopSpeak();
		std::lock_guard<std::mutex> loaderLock(GetLoaderMutex());
		CleanUpMembers();
	}
#else
	isInitialized.store(false, std::memory_order_release);
	m_isSpeakingCache.store(false, std::memory_order_release);
	
	while (m_running.load(std::memory_order_acquire)) {
		std::unique_lock<std::mutex> lock(m_queueMutex);
		m_cv.wait_for(lock, std::chrono::milliseconds(100));
		while(!m_commandQueue.empty()) m_commandQueue.pop(); 
	}
#endif
}

} // namespace Sral
