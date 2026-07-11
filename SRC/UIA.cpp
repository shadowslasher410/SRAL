#include "UIA.h"
#include <chrono>
#include <concepts>
#include <algorithm>
#include <cstring>
#include "Encoding.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <uiautomation.h>
#include <comdef.h>
#include "../Dep/UIAProvider.h"

_COM_SMARTPTR_TYPEDEF(IUIAutomation, __uuidof(IUIAutomation));
_COM_SMARTPTR_TYPEDEF(IUIAutomationCondition, __uuidof(IUIAutomationCondition));
_COM_SMARTPTR_TYPEDEF(IUIAutomationElement, __uuidof(IUIAutomationElement));
#define WINDOWS_UIA_SUPPORTED 1
#else
#define WINDOWS_UIA_SUPPORTED 0
#endif

namespace Sral {

Uia::~Uia() {
	static_cast<void>(Uia::Uninitialize());
}

bool Uia::Initialize() {
	std::lock_guard<std::mutex> lock(instanceMutex);
	if (isInitialized.load(std::memory_order_acquire)) {
		return true;
	}

	for (size_t i = 0; i < RING_BUFFER_SIZE; ++i) {
		m_ring_queue[i].sequence.store(i, std::memory_order_relaxed);
	}

	m_head.store(0, std::memory_order_relaxed);
	m_tail.store(0, std::memory_order_relaxed);
	m_ring_bell.store(false, std::memory_order_relaxed);

	m_workerThread = std::jthread([this](std::stop_token st) noexcept {
		this->BackgroundWorkerLoop(st);
	});

	isInitialized.store(true, std::memory_order_release);
	return true;
}

bool Uia::Uninitialize() {
	std::jthread thread_to_join;
	{
		std::lock_guard<std::mutex> lock(instanceMutex);
		if (!isInitialized.load(std::memory_order_acquire)) {
			return true;
		}

		m_workerThread.request_stop();
		m_tail.store(m_head.load(std::memory_order_relaxed), std::memory_order_release);
		
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

void Uia::CleanUpMembers() noexcept {
#if WINDOWS_UIA_SUPPORTED
    if (pProvider) {
        auto* rawProvider = static_cast<Provider*>(pProvider);
        rawProvider->Release();
        pProvider = nullptr;
    }
    if (pCondition) {
        auto* cond = static_cast<IUIAutomationCondition*>(pCondition);
        cond->Release();
        pCondition = nullptr;
    }
    if (pElement) {
        auto* elem = static_cast<IUIAutomationElement*>(pElement);
        elem->Release();
        pElement = nullptr;
    }
    if (pAutomation) {
        auto* autoInst = static_cast<IUIAutomation*>(pAutomation);
        autoInst->Release();
        pAutomation = nullptr;
    }
#endif
    m_isSpeakingCache.store(false, std::memory_order_release);
}

bool Uia::GetActive() {
#if WINDOWS_UIA_SUPPORTED
	if (!::UiaClientsAreListening()) {
		return false;
	}

	BOOL screenReaderRunning = FALSE;
	BOOL result = ::SystemParametersInfoW(SPI_GETSCREENREADER, 0, &screenReaderRunning, 0);
	return (result && screenReaderRunning);
#else
	return false;
#endif
}

bool Uia::IsSpeaking() {
	return m_isSpeakingCache.load(std::memory_order_acquire);
}

bool Uia::Speak(const char* text, bool interrupt) {
	std::string_view text_view(text ? text : "");
	if (text_view.empty()) return false;

	if (!isInitialized.load(std::memory_order_acquire)) return false;

	if (interrupt) {
		size_t head_snap = m_head.load(std::memory_order_relaxed);
		m_tail.store(head_snap, std::memory_order_release);
	}

	ThreadCommand* task = nullptr;
	size_t ticket = m_head.load(std::memory_order_relaxed);

	while (true) {
		task = &m_ring_queue[ticket & RING_MASK];
		size_t seq = task->sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

		if (difference == 0) {
			if (m_head.compare_exchange_weak(ticket, ticket + 1, std::memory_order_relaxed)) {
				break;
			}
		} else if (difference < 0) {
			size_t current_tail = m_tail.load(std::memory_order_relaxed);
			m_tail.compare_exchange_weak(current_tail, current_tail + 1, std::memory_order_relaxed);
			ticket = m_head.load(std::memory_order_relaxed);
		} else {
			ticket = m_head.load(std::memory_order_relaxed);
		}
	}

	size_t max_copy = (std::min)(static_cast<size_t>(text_view.size()), static_cast<size_t>(task->payload.size() - 1));
	std::memcpy(task->payload.data(), text_view.data(), max_copy);
	task->payload[max_copy] = '\0';
	task->type = CommandType::Speak;
	task->interrupt = interrupt;

	task->sequence.store(ticket + 1, std::memory_order_release);
	
	if (!m_ring_bell.exchange(true, std::memory_order_release)) {
		m_ring_bell.notify_one();
	}
	return true;
}

bool Uia::StopSpeech() {
	if (!isInitialized.load(std::memory_order_acquire)) return false;

	size_t head_snap = m_head.load(std::memory_order_relaxed);
	m_tail.store(head_snap, std::memory_order_release);

	ThreadCommand* task = nullptr;
	size_t ticket = m_head.load(std::memory_order_relaxed);

	while (true) {
		task = &m_ring_queue[ticket & RING_MASK];
		size_t seq = task->sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

		if (difference == 0) {
			if (m_head.compare_exchange_weak(ticket, ticket + 1, std::memory_order_relaxed)) {
				break;
			}
		} else if (difference < 0) {
			size_t current_tail = m_tail.load(std::memory_order_relaxed);
			m_tail.compare_exchange_weak(current_tail, current_tail + 1, std::memory_order_relaxed);
			ticket = m_head.load(std::memory_order_relaxed);
		} else {
			ticket = m_head.load(std::memory_order_relaxed);
		}
	}

	task->payload[0] = '\0';
	task->type = CommandType::Stop;
	task->interrupt = true;

	task->sequence.store(ticket + 1, std::memory_order_release);
	
	if (!m_ring_bell.exchange(true, std::memory_order_release)) {
		m_ring_bell.notify_one();
	}
	return true;
}

void Uia::BackgroundWorkerLoop(std::stop_token stop_token) noexcept {
#if WINDOWS_UIA_SUPPORTED
	HRESULT hr = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
		return;
	}

	{
		std::lock_guard<std::mutex> instanceLock(instanceMutex);
		IUIAutomation* pAutoInstance = nullptr;
		hr = ::CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_IUIAutomation, reinterpret_cast<void**>(&pAutoInstance));
		if (SUCCEEDED(hr) && pAutoInstance) {
			pAutomation = pAutoInstance;
			
			IUIAutomationConditionPtr pCondInstance;
			_variant_t varNameLocal(L"");
			hr = pAutoInstance->CreatePropertyConditionEx(UIA_NamePropertyId, varNameLocal, PropertyConditionFlags_None, &pCondInstance);
			if (SUCCEEDED(hr) && pCondInstance) {
				pCondInstance->AddRef();
				pCondition = pCondInstance.GetInterfacePtr();
			}
		}
		
		if (!pAutomation || !pCondition) {
			CleanUpMembers();
			::CoUninitialize();
			return;
		}
	}

	while (!stop_token.stop_requested()) {
		size_t current_tail = m_tail.load(std::memory_order_relaxed);
		ThreadCommand& task = m_ring_queue[current_tail & RING_MASK];
		
		size_t seq = task.sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1);

		if (difference != 0) {
			m_ring_bell.store(false, std::memory_order_release);
			
			seq = task.sequence.load(std::memory_order_acquire);
			if (static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1) != 0) {
				m_ring_bell.wait(false, std::memory_order_acquire);
			} else {
				m_ring_bell.store(true, std::memory_order_release);
			}
			if (stop_token.stop_requested()) [[unlikely]] return;
			continue;
		}

		CommandType localType = task.type;
		bool localInterrupt = task.interrupt;
		
		std::array<char, 512> localPayload = task.payload;
		m_tail.store(current_tail + 1, std::memory_order_relaxed);

		m_isSpeakingCache.store(true, std::memory_order_release);

		{
			std::lock_guard<std::mutex> instanceLock(instanceMutex);
			if (pAutomation) {
				auto* autoInst = static_cast<IUIAutomation*>(pAutomation);

				if (localType == CommandType::Stop) {
					if (pProvider) {
						_bstr_t emptyText(L"");
						_bstr_t emptyActivityId(L"");
						(void)::UiaRaiseNotificationEvent(static_cast<IRawElementProviderSimple*>(pProvider), 
							NotificationKind_ActionCompleted, NotificationProcessing_ImportantMostRecent, 
							emptyText, emptyActivityId);
					}
				} 
				else if (localType == CommandType::Speak) {
					std::wstring broadString;
					if (UnicodeConvert(localPayload.data(), broadString) && !broadString.empty()) {
						HWND foreground = ::GetForegroundWindow();
						if (foreground) {
							if (pProvider) {
                                auto* oldProvider = static_cast<Provider*>(pProvider);
                                oldProvider->Release();
                                pProvider = nullptr;
                            }

							if (pElement) {
								static_cast<IUIAutomationElement*>(pElement)->Release();
								pElement = nullptr;
							}

							Provider* pRawProvider = new (std::nothrow) Provider(foreground);
							if (pRawProvider) {
								pProvider = pRawProvider;
								IUIAutomationElement* elem = nullptr;
								hr = autoInst->ElementFromHandle(foreground, &elem);
								if (SUCCEEDED(hr) && elem) {
									pElement = elem;
									_bstr_t bstrText(broadString.c_str());
									_bstr_t bstrActivityId(L"");
									const NotificationProcessing flags = localInterrupt 
										? NotificationProcessing_ImportantMostRecent 
										: NotificationProcessing_ImportantAll;
									(void)::UiaRaiseNotificationEvent(pRawProvider, 
										NotificationKind_ActionCompleted, flags, bstrText, bstrActivityId);
								}
							}
						}
					}
				}
			}
		}

		m_isSpeakingCache.store(false, std::memory_order_release);
		task.sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
	}

	{
		std::lock_guard<std::mutex> instanceLock(instanceMutex);
		CleanUpMembers();
		::CoUninitialize();
	}
#else
	(void)stop_token;
	CleanUpMembers();
#endif
}

} // namespace Sral