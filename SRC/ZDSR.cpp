#include "ZDSR.h"

#include <chrono>
#include <concepts>

#include "Encoding.h"

namespace Sral {

void Zdsr::LibraryDeleter::operator()(HMODULE handle) const noexcept {
#if defined(_WIN32) || defined(_WIN64)
	if (handle) [[likely]]
		::FreeLibrary(handle);
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
			reinterpret_cast<LPCWSTR>(&GetLoaderMutex),
			&dummy) == 0) {
		(void)lib.release();
	}
	else {
		(void)Uninitialize();
	}
#else
	(void)Uninitialize();
#endif
}

bool Zdsr::Initialize() {
	if (isInitialized.load(std::memory_order_acquire))
		return true;

	std::lock_guard<std::mutex> lock(GetLoaderMutex());
	if (isInitialized.load(std::memory_order_relaxed))
		return true;

	m_workerThread = std::jthread([this](std::stop_token st) { BackgroundWorkerLoop(st); });
	return true;
}

bool Zdsr::Uninitialize() {
	m_workerThread.request_stop();
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		m_cv.notify_all();
	}

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
	fInitTTS = nullptr;
	fSpeak = nullptr;
	fStopSpeak = nullptr;
	fGetSpeakState = nullptr;
}

bool Zdsr::GetActive() {
	return isInitialized.load(std::memory_order_acquire);
}
bool Zdsr::IsSpeaking() {
	return m_isSpeakingCache.load(std::memory_order_acquire);
}

bool Zdsr::Speak(const char* text, bool interrupt) {
	if (!text || !isInitialized.load(std::memory_order_acquire)) [[unlikely]]
		return false;
	std::string_view textStr(text);
	if (textStr.empty())
		return false;

	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		if (interrupt) {
			std::queue<ThreadCommand> empty;
			std::swap(m_commandQueue, empty);
			m_commandQueue.push(ThreadCommand{CommandType::Stop, "", true});
		}
		m_commandQueue.push(ThreadCommand{CommandType::Speak, std::string(textStr), interrupt});
	}
	m_cv.notify_one();
	return true;
}

bool Zdsr::StopSpeech() {
	if (!isInitialized.load(std::memory_order_acquire))
		return false;
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		std::queue<ThreadCommand> empty;
		std::swap(m_commandQueue, empty);
		m_commandQueue.push(ThreadCommand{CommandType::Stop, "", true});
	}
	m_cv.notify_one();
	return true;
}

void Zdsr::BackgroundWorkerLoop(std::stop_token stop_token) noexcept {
#if defined(_WIN32) || defined(_WIN64)
	{
		std::lock_guard<std::mutex> instanceLock(instanceMutex);
		lib.reset(::LoadLibraryW(L"ZDSRAPI.dll"));
		if (lib) {
			HMODULE const moduleHandle = reinterpret_cast<HMODULE>(lib.get());

			fInitTTS = SafeProcCast<InitTTS_t>(::GetProcAddress(moduleHandle, "InitTTS"));
			fSpeak = SafeProcCast<Speak_t>(::GetProcAddress(moduleHandle, "Speak"));
			fStopSpeak = SafeProcCast<StopSpeak_t>(::GetProcAddress(moduleHandle, "StopSpeak"));
			fGetSpeakState = SafeProcCast<GetSpeakState_t>(::GetProcAddress(moduleHandle, "GetSpeakState"));

			if (fInitTTS && fSpeak && fStopSpeak && fGetSpeakState) {
				wchar_t emptyBuffer = {L'\0'};
				if (fInitTTS(0, &emptyBuffer) == 0) {
					isInitialized.store(true, std::memory_order_release);
				}
			}
		}
		if (!isInitialized.load(std::memory_order_relaxed)) {
			CleanUpMembers();
			return;
		}
	}

	while (!stop_token.stop_requested()) [[likely]] {
		ThreadCommand cmd;
		bool hasCommand = false;
		{
			std::unique_lock<std::mutex> lock(m_queueMutex);

			m_cv.wait(lock, [this, &stop_token] { return !m_commandQueue.empty() || stop_token.stop_requested(); });

			if (stop_token.stop_requested() && m_commandQueue.empty()) [[unlikely]] {
				break;
			}

			cmd = std::move(m_commandQueue.front());
			m_commandQueue.pop();
			hasCommand = true;
		}

		if (hasCommand) {
			std::lock_guard<std::mutex> instanceLock(instanceMutex);
			if (isInitialized.load(std::memory_order_relaxed)) {
				if (cmd.type == CommandType::Stop) {
					if (fStopSpeak)
						(void)fStopSpeak();
				}
				else if (cmd.type == CommandType::Speak && fSpeak) {
					std::wstring broadString;
					if (UnicodeConvert(cmd.payload, broadString) && !broadString.empty()) {
						(void)fSpeak(broadString.c_str(), cmd.interrupt ? TRUE : FALSE);
					}
				}
			}
		}

		if (isInitialized.load(std::memory_order_relaxed) && fGetSpeakState) {
			m_isSpeakingCache.store(fGetSpeakState() == 3, std::memory_order_release);
		}
		else {
			m_isSpeakingCache.store(false, std::memory_order_release);
		}
	}

	{
		std::lock_guard<std::mutex> instanceLock(instanceMutex);
		if (fStopSpeak && isInitialized.load(std::memory_order_relaxed))
			(void)fStopSpeak();
		std::lock_guard<std::mutex> loaderLock(GetLoaderMutex());
		CleanUpMembers();
	}
#else
	isInitialized.store(false, std::memory_order_release);
	m_isSpeakingCache.store(false, std::memory_order_release);

	while (!stop_token.stop_requested()) [[likely]] {
		std::unique_lock<std::mutex> lock(m_queueMutex);
		m_cv.wait(lock, [this, &stop_token] { return !m_commandQueue.empty() || stop_token.stop_requested(); });

		while (!m_commandQueue.empty()) {
			m_commandQueue.pop();
		}
	}
#endif
}

} // namespace Sral
