#include "Jaws.h"

#include <algorithm>
#include <cstring>
#include <string_view>

#include "Encoding.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

#include <comdef.h>

namespace Sral {

Jaws::~Jaws() noexcept {
	static_cast<void>(Jaws::Uninitialize());
}

bool Jaws::Initialize() noexcept {
	std::lock_guard<std::mutex> lock(instanceMutex);

	if (isInitialized.load(std::memory_order_acquire)) [[unlikely]] {
		return true;
	}

	for (size_t i = 0; i < RING_BUFFER_SIZE; ++i) {
		m_ring_queue[i].sequence.store(i, std::memory_order_relaxed);
	}

	m_head.store(0, std::memory_order_relaxed);
	m_tail.store(0, std::memory_order_relaxed);
	m_ring_bell.store(false, std::memory_order_relaxed);
	m_fastPathInterrupt.store(false, std::memory_order_relaxed);

	m_workerThread = std::jthread([this](std::stop_token st) noexcept { this->BackgroundWorkerLoop(st); });

	isInitialized.store(true, std::memory_order_release);
	return true;
}

bool Jaws::Uninitialize() noexcept {
	std::jthread thread_to_join;
	{
		std::lock_guard<std::mutex> lock(instanceMutex);
		if (!isInitialized.load(std::memory_order_acquire)) [[unlikely]] {
			return true;
		}

		m_workerThread.request_stop();

		m_ring_bell.store(true, std::memory_order_release);
		m_ring_bell.notify_one();

		thread_to_join = std::move(m_workerThread);
		isInitialized.store(false, std::memory_order_release);
	}

	if (thread_to_join.joinable()) {
		thread_to_join.join();
	}
	return true;
}

bool Jaws::GetActive() noexcept {
	return isInitialized.load(std::memory_order_acquire);
}

bool Jaws::Speak(const char* text, bool interrupt) noexcept {
	if (!text || text == '\0') [[unlikely]]
		return false;
	if (!isInitialized.load(std::memory_order_acquire)) [[unlikely]]
		return false;

	if (interrupt) {
		m_fastPathInterrupt.store(true, std::memory_order_release);
		m_tail.store(m_head.load(std::memory_order_relaxed), std::memory_order_release);
	}

	std::string_view text_view(text);
	ThreadCommand* task = nullptr;
	size_t ticket = m_head.load(std::memory_order_acquire);

	while (true) {
		task = &m_ring_queue[ticket & RING_MASK];
		size_t seq = task->sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

		if (difference == 0) {
			if (m_head.compare_exchange_weak(
					ticket, ticket + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
				break;
			}
		}
		else if (difference < 0) {
			return false;
		}
		else {
			ticket = m_head.load(std::memory_order_acquire);
		}
	}

	size_t max_copy = (std::min)(text_view.size(), task->data.char_payload.size() - 1);
	std::memcpy(task->data.char_payload.data(), text_view.data(), max_copy);
	task->data.char_payload[max_copy] = '\0';

	task->type = CommandType::Speak;
	task->interrupt = interrupt;
	task->payload_length = max_copy;

	task->sequence.store(ticket + 1, std::memory_order_release);

	m_ring_bell.store(true, std::memory_order_release);
	m_ring_bell.notify_one();
	return true;
}

bool Jaws::Braille(const char* text) noexcept {
	if (!text || text == '\0') [[unlikely]]
		return false;
	if (!isInitialized.load(std::memory_order_acquire)) [[unlikely]]
		return false;

	std::wstring wstr;
	if (!UnicodeConvert(text, wstr))
		return false;

	for (auto& ch : wstr) {
		if (ch == L'"')
			ch = L'\'';
	}

	wstr.insert(0, L"BrailleString(\"");
	wstr.append(L"\")");

	ThreadCommand* task = nullptr;
	size_t ticket = m_head.load(std::memory_order_acquire);

	while (true) {
		task = &m_ring_queue[ticket & RING_MASK];
		size_t seq = task->sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

		if (difference == 0) {
			if (m_head.compare_exchange_weak(
					ticket, ticket + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
				break;
			}
		}
		else if (difference < 0) {
			return false;
		}
		else {
			ticket = m_head.load(std::memory_order_acquire);
		}
	}

	size_t max_copy = (std::min)(wstr.size(), task->data.wchar_payload.size() - 1);
	std::wmemcpy(task->data.wchar_payload.data(), wstr.data(), max_copy);
	task->data.wchar_payload[max_copy] = L'\0';

	task->type = CommandType::Braille;
	task->interrupt = false;
	task->payload_length = max_copy;

	task->sequence.store(ticket + 1, std::memory_order_release);
	m_ring_bell.store(true, std::memory_order_release);
	m_ring_bell.notify_one();
	return true;
}

bool Jaws::StopSpeech() noexcept {
	if (!isInitialized.load(std::memory_order_acquire)) [[unlikely]]
		return false;

	m_fastPathInterrupt.store(true, std::memory_order_release);
	m_tail.store(m_head.load(std::memory_order_relaxed), std::memory_order_release);

	m_ring_bell.store(true, std::memory_order_release);
	m_ring_bell.notify_one();
	return true;
}

bool Jaws::IsSpeaking() noexcept {
	return m_isSpeakingCache.load(std::memory_order_acquire);
}

void Jaws::BackgroundWorkerLoop(std::stop_token stopToken) noexcept {
	(void)SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	IJawsApiPtr localJawsApi = nullptr;

	if (SUCCEEDED(hr)) {
		hr = localJawsApi.CreateInstance(CLSID_JawsApi, nullptr, CLSCTX_INPROC_SERVER);
	}

	if (FAILED(hr) || localJawsApi == nullptr) {
		if (SUCCEEDED(hr))
			CoUninitialize();
		return;
	}

	BSTR bstrActive = nullptr;

	while (!stopToken.stop_requested()) {
		m_ring_bell.store(false, std::memory_order_release);

		if (m_fastPathInterrupt.exchange(false, std::memory_order_acq_rel)) {
			(void)localJawsApi->StopSpeech();
		}

		size_t tail = m_tail.load(std::memory_order_relaxed);
		size_t head = m_head.load(std::memory_order_acquire);

		if (tail == head) {
			m_ring_bell.wait(false, std::memory_order_acquire);
			continue;
		}

		while (tail != head) {
			if (stopToken.stop_requested()) [[unlikely]]
				break;

			if (m_fastPathInterrupt.exchange(false, std::memory_order_acq_rel)) {
				(void)localJawsApi->StopSpeech();
				tail = m_head.load(std::memory_order_relaxed);
				m_tail.store(tail, std::memory_order_release);
				break;
			}

			ThreadCommand& cmd = m_ring_queue[tail & RING_MASK];
			size_t seq = cmd.sequence.load(std::memory_order_acquire);

			if (seq != (tail + 1)) {
				break;
			}

			m_isSpeakingCache.store(true, std::memory_order_relaxed);

			switch (cmd.type) {
			case CommandType::Speak: {
				if (cmd.interrupt) {
					(void)localJawsApi->StopSpeech();
				}

				int wlen = MultiByteToWideChar(
					CP_ACP, 0, cmd.data.char_payload.data(), static_cast<int>(cmd.payload_length), nullptr, 0);
				if (wlen > 0) {
					bstrActive = SysAllocStringLen(nullptr, wlen);
					if (bstrActive) {
						MultiByteToWideChar(CP_ACP,
							0,
							cmd.data.char_payload.data(),
							static_cast<int>(cmd.payload_length),
							bstrActive,
							wlen);
						VARIANT_BOOL result = VARIANT_FALSE;
						const VARIANT_BOOL flush = cmd.interrupt ? VARIANT_TRUE : VARIANT_FALSE;

						(void)localJawsApi->SayString(bstrActive, flush, &result);
						SysFreeString(bstrActive);
					}
				}
				break;
			}
			case CommandType::Braille: {
				bstrActive = SysAllocStringLen(cmd.data.wchar_payload.data(), static_cast<UINT>(cmd.payload_length));
				if (bstrActive) {
					VARIANT_BOOL result = VARIANT_FALSE;
					(void)localJawsApi->RunFunction(bstrActive, &result);
					SysFreeString(bstrActive);
				}
				break;
			}
			case CommandType::Stop: {
				(void)localJawsApi->StopSpeech();
				break;
			}
			default:
				break;
			}

			m_isSpeakingCache.store(false, std::memory_order_relaxed);

			cmd.sequence.store(tail + RING_BUFFER_SIZE, std::memory_order_release);
			tail++;
			m_tail.store(tail, std::memory_order_release);

			head = m_head.load(std::memory_order_acquire);
		}
	}

	localJawsApi = nullptr;
	CoUninitialize();
}

} // namespace Sral
#endif
