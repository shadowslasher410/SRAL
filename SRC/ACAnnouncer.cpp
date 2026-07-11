#include "ACAnnouncer.h"

#include <algorithm>
#include <concepts>
#include <cstring>
#include <iostream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
namespace Sral {

ACAnnouncer::ACAnnouncer() : m_ring_queue{} {
	for (size_t i = 0; i < RING_BUFFER_SIZE; ++i) {
		m_ring_queue[i].sequence.store(i, std::memory_order_relaxed);
	}
}

ACAnnouncer::~ACAnnouncer() {
	ACAnnouncer::Uninitialize();
}

ACAnnouncer::ACAnnouncer(ACAnnouncer&& other) noexcept {
	*this = std::move(other);
}

ACAnnouncer& ACAnnouncer::operator=(ACAnnouncer&& other) noexcept {
	if (this != &other) {
		ACAnnouncer::Uninitialize();
		std::lock_guard lock(other.m_init_mutex);
		m_context_handle = other.m_context_handle;
		m_adapter.store(other.m_adapter.load(), std::memory_order_relaxed);
		m_worker_thread = std::move(other.m_worker_thread);

		m_head.store(other.m_head.load(), std::memory_order_relaxed);
		m_tail.store(other.m_tail.load(), std::memory_order_relaxed);

		other.m_context_handle = nullptr;
		other.m_adapter.store(nullptr, std::memory_order_relaxed);
	}
	return *this;
}

bool ACAnnouncer::InitializeWithContext(void* platform_window_or_context) {
	{
		std::lock_guard lock(m_init_mutex);
		if (m_adapter.load(std::memory_order_relaxed))
			return false;
		m_context_handle = platform_window_or_context;
	}
	return Initialize();
}

bool ACAnnouncer::Uninitialize() {
	std::jthread thread_to_join;
	void* adapter_to_free = nullptr;

	{
		std::lock_guard lock(m_init_mutex);
		adapter_to_free = m_adapter.load(std::memory_order_relaxed);
		if (!adapter_to_free)
			return true;

		m_worker_thread.request_stop();

		size_t head_snap = m_head.load(std::memory_order_relaxed);
		m_tail.store(head_snap, std::memory_order_release);
		m_adapter.store(nullptr, std::memory_order_release);

		m_ring_bell.store(true, std::memory_order_release);
		m_ring_bell.notify_one();

		thread_to_join = std::move(m_worker_thread);
	}

	if (thread_to_join.joinable()) {
		thread_to_join.join();
	}

	if (adapter_to_free) {
#if defined(_WIN32)
		accesskit_windows_adapter_free(static_cast<accesskit_windows_adapter*>(adapter_to_free));
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
		accesskit_ios_adapter_free(static_cast<accesskit_ios_adapter*>(adapter_to_free));
#else
		accesskit_macos_adapter_free(static_cast<accesskit_macos_adapter*>(adapter_to_free));
#endif
#elif defined(__ANDROID__)
		accesskit_android_adapter_free(static_cast<accesskit_android_adapter*>(adapter_to_free));
#else
		accesskit_unix_adapter_free(static_cast<accesskit_unix_adapter*>(adapter_to_free));
#endif
	}

	m_context_handle = nullptr;
	return true;
}

bool ACAnnouncer::Speak(const char* speech_text, bool interrupt) {
	std::string_view text_view(speech_text ? speech_text : "");

	if (!m_adapter.load(std::memory_order_acquire)) {
#if defined(_WIN32)
		if (!IsScreenReaderActive())
			return false;
#endif
		std::lock_guard lock(m_init_mutex);
		if (!m_adapter.load(std::memory_order_acquire) && !Initialize())
			return false;
	}

	if (m_worker_thread.get_stop_token().stop_requested())
		return false;

	if (interrupt) {
		size_t head_snap = m_head.load(std::memory_order_relaxed);
		m_tail.store(head_snap, std::memory_order_release);
	}

	SpeechTask* task_ptr = nullptr;
	size_t ticket = m_head.load(std::memory_order_relaxed);

	while (true) {
		task_ptr = &m_ring_queue[ticket & RING_MASK];
		size_t seq = task_ptr->sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

		if (difference == 0) {
			if (m_head.compare_exchange_weak(ticket, ticket + 1, std::memory_order_relaxed)) {
				break;
			}
		}
		else if (difference < 0) {
			return false;
		}
		else {
			ticket = m_head.load(std::memory_order_relaxed);
		}
	}

	size_t max_copy = (std::min)(static_cast<size_t>(text_view.size()), static_cast<size_t>(task_ptr->text.size() - 1));
	std::memcpy(task_ptr->text.data(), text_view.data(), max_copy);
	task_ptr->text[max_copy] = '\0';
	task_ptr->interrupt = interrupt;

	task_ptr->sequence.store(ticket + 1, std::memory_order_release);

	if (!m_ring_bell.exchange(true, std::memory_order_release)) {
		m_ring_bell.notify_one();
	}

	return true;
}

bool ACAnnouncer::StopSpeech() {
	return Speak("", true);
}

bool ACAnnouncer::GetActive() {
	return m_adapter.load(std::memory_order_acquire) != nullptr && !m_worker_thread.get_stop_token().stop_requested();
}

void ACAnnouncer::BackgroundWorkerLoop(std::stop_token stop_token) {
	while (!stop_token.stop_requested()) {
		size_t current_tail = m_tail.load(std::memory_order_relaxed);
		SpeechTask& task = m_ring_queue[current_tail & RING_MASK];

		size_t seq = task.sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1);

		if (difference != 0) {
			m_ring_bell.store(false, std::memory_order_release);

			seq = task.sequence.load(std::memory_order_acquire);
			if (static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1) != 0) {
				m_ring_bell.wait(false, std::memory_order_acquire);
			}
			else {
				m_ring_bell.store(true, std::memory_order_release);
			}
			if (stop_token.stop_requested()) [[unlikely]]
				return;
			continue;
		}

		const char* task_text = task.text.data();
		bool task_interrupt = task.interrupt;

		m_tail.store(current_tail + 1, std::memory_order_relaxed);

		void* current_adapter = m_adapter.load(std::memory_order_acquire);
		if (!current_adapter)
			continue;

		bool use_b = m_use_id_b.load(std::memory_order_relaxed);
		accesskit_node_id active_id = use_b ? ANNOUNCEMENT_ID_B : ANNOUNCEMENT_ID_A;
		accesskit_node_id inactive_id = use_b ? ANNOUNCEMENT_ID_A : ANNOUNCEMENT_ID_B;
		m_use_id_b.store(!use_b, std::memory_order_relaxed);

		struct accesskit_node* window_raw = accesskit_node_new(ACCESSKIT_ROLE_WINDOW);
		if (!window_raw)
			continue;
		accesskit_node_set_role(window_raw, ACCESSKIT_ROLE_WINDOW);
		accesskit_node_push_child(window_raw, ANNOUNCEMENT_ID_A);
		accesskit_node_push_child(window_raw, ANNOUNCEMENT_ID_B);

		struct accesskit_node* announcement_raw = accesskit_node_new(ACCESSKIT_ROLE_STATUS);
		if (!announcement_raw) {
			accesskit_node_free(window_raw);
			continue;
		}
		accesskit_node_set_role(announcement_raw, ACCESSKIT_ROLE_STATUS);
		accesskit_node_set_value(announcement_raw, task_text);
		accesskit_node_set_live(announcement_raw, task_interrupt ? ACCESSKIT_LIVE_ASSERTIVE : ACCESSKIT_LIVE_POLITE);

		struct accesskit_node* placeholder_raw = accesskit_node_new(ACCESSKIT_ROLE_STATUS);
		if (!placeholder_raw) {
			accesskit_node_free(announcement_raw);
			accesskit_node_free(window_raw);
			continue;
		}
		accesskit_node_set_role(placeholder_raw, ACCESSKIT_ROLE_STATUS);
		accesskit_node_set_value(placeholder_raw, "");

		struct accesskit_tree_update* update_raw = accesskit_tree_update_with_capacity_and_focus(3, active_id);
		if (!update_raw) {
			accesskit_node_free(placeholder_raw);
			accesskit_node_free(announcement_raw);
			accesskit_node_free(window_raw);
			continue;
		}

		accesskit_tree_update_push_node(update_raw, WINDOW_ID, window_raw);
		accesskit_tree_update_push_node(update_raw, active_id, announcement_raw);
		accesskit_tree_update_push_node(update_raw, inactive_id, placeholder_raw);

		bool status = false;

#if defined(_WIN32)
		status = accesskit_windows_adapter_update_if_active(
			static_cast<accesskit_windows_adapter*>(current_adapter), ProvideUpdateCallback, this);
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
		status = accesskit_ios_adapter_update_if_active(
			static_cast<accesskit_ios_adapter*>(current_adapter), ProvideUpdateCallback, this);
#else
		status = accesskit_macos_adapter_update_if_active(
			static_cast<accesskit_macos_adapter*>(current_adapter), ProvideUpdateCallback, this);
#endif
#elif defined(__ANDROID__)
		status = accesskit_android_adapter_update_if_active(
			static_cast<accesskit_android_adapter*>(current_adapter), ProvideUpdateCallback, this);
#else
		status = accesskit_unix_adapter_update_if_active(
			static_cast<accesskit_unix_adapter*>(current_adapter), ProvideUpdateCallback, this);
#endif

		if (!status) {
			struct accesskit_tree_update* leaked_packet =
				m_active_update_packet.exchange(nullptr, std::memory_order_acq_rel);
			if (leaked_packet) {
				accesskit_tree_update_free(leaked_packet);
			}
		}

		task.sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
	}
}

void ACAnnouncer::OnActionRequestCallback(struct accesskit_action_request* request, void* userdata) {
	if (userdata) [[likely]] {
		static_cast<ACAnnouncer*>(userdata)->HandleActionRequest(request);
	}
	else if (request) [[unlikely]] {
		accesskit_action_request_free(request);
	}
}

struct accesskit_tree_update* ACAnnouncer::ProvideUpdateCallback(void* userdata) {
	if (userdata) [[likely]] {
		return static_cast<ACAnnouncer*>(userdata)->InterceptUpdatePayload();
	}
	return nullptr;
}

void ACAnnouncer::HandleActionRequest(struct accesskit_action_request* request) noexcept {
	if (request) [[likely]] {
		accesskit_action_request_free(request);
	}
}

struct accesskit_tree_update* ACAnnouncer::InterceptUpdatePayload() noexcept {
	return m_active_update_packet.exchange(nullptr, std::memory_order_acq_rel);
}

bool ACAnnouncer::IsScreenReaderActive() noexcept {
#if defined(_WIN32)
	BOOL screenReaderRunning = FALSE;
	if (::SystemParametersInfoW(SPI_GETSCREENREADER, 0, &screenReaderRunning, 0)) {
		return screenReaderRunning == TRUE;
	}
	return false;
#else
	return true;
#endif
}

bool ACAnnouncer::Initialize() {
	std::lock_guard lock(m_init_mutex);
	void* current_adapter = m_adapter.load(std::memory_order_acquire);
	if (current_adapter)
		return true;

#if defined(_WIN32)
	HWND target_window = static_cast<HWND>(m_context_handle);
	if (!target_window) {
		target_window = ::GetForegroundWindow();
		if (!target_window)
			return false;

		DWORD current_process_id = ::GetCurrentProcessId();
		DWORD window_process_id = 0;
		::GetWindowThreadProcessId(target_window, &window_process_id);
		if (current_process_id != window_process_id) [[unlikely]] {
			return false;
		}
		m_context_handle = static_cast<void*>(target_window);
	}
	current_adapter = accesskit_windows_adapter_new(target_window, true, OnActionRequestCallback, this);
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
	if (!m_context_handle)
		return false;
	current_adapter = accesskit_ios_adapter_new(m_context_handle, OnActionRequestCallback, this);
#else
	if (!m_context_handle)
		return false;
	current_adapter = accesskit_macos_adapter_new(m_context_handle, OnActionRequestCallback, this);
#endif
#elif defined(__ANDROID__)
	if (!m_context_handle)
		return false;
	current_adapter = accesskit_android_adapter_new(m_context_handle, OnActionRequestCallback, this);
#else
	current_adapter = accesskit_unix_adapter_new(OnActionRequestCallback, this);
#endif

	if (!current_adapter)
		return false;
	m_adapter.store(current_adapter, std::memory_order_release);

	struct accesskit_tree* tree_raw = accesskit_tree_new(WINDOW_ID);
	if (!tree_raw)
		return false;

	struct accesskit_node* window_raw = accesskit_node_new(ACCESSKIT_ROLE_WINDOW);
	if (!window_raw) {
		accesskit_tree_free(tree_raw);
		return false;
	}
	accesskit_node_set_role(window_raw, ACCESSKIT_ROLE_WINDOW);
	accesskit_node_push_child(window_raw, ANNOUNCEMENT_ID_A);
	accesskit_node_push_child(window_raw, ANNOUNCEMENT_ID_B);

	struct accesskit_tree_update* init_update_raw = accesskit_tree_update_with_capacity_and_focus(3, WINDOW_ID);
	if (!init_update_raw) {
		accesskit_node_free(window_raw);
		accesskit_tree_free(tree_raw);
		return false;
	}

	accesskit_tree_update_set_tree(init_update_raw, tree_raw);
	accesskit_tree_update_push_node(init_update_raw, WINDOW_ID, window_raw);

	for (accesskit_node_id id : {ANNOUNCEMENT_ID_A, ANNOUNCEMENT_ID_B}) {
		struct accesskit_node* node_raw = accesskit_node_new(ACCESSKIT_ROLE_STATUS);
		if (node_raw) {
			accesskit_node_set_role(node_raw, ACCESSKIT_ROLE_STATUS);
			accesskit_node_set_live(node_raw, ACCESSKIT_LIVE_POLITE);
			accesskit_tree_update_push_node(init_update_raw, id, node_raw);
		}
	}
	m_active_update_packet.store(init_update_raw, std::memory_order_release);
	bool status = false;

#if defined(_WIN32)
	status = accesskit_windows_adapter_update_if_active(
		static_cast<accesskit_windows_adapter*>(current_adapter), ProvideUpdateCallback, this);
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
	status = accesskit_ios_adapter_update_if_active(
		static_cast<accesskit_ios_adapter*>(current_adapter), ProvideUpdateCallback, this);
#else
	status = accesskit_macos_adapter_update_if_active(
		static_cast<accesskit_macos_adapter*>(current_adapter), ProvideUpdateCallback, this);
#endif
#elif defined(__ANDROID__)
	status = accesskit_android_adapter_update_if_active(
		static_cast<accesskit_android_adapter*>(current_adapter), ProvideUpdateCallback, this);
#else
	status = accesskit_unix_adapter_update_if_active(
		static_cast<accesskit_unix_adapter*>(current_adapter), ProvideUpdateCallback, this);
#endif
	if (!status) {
		struct accesskit_tree_update* leaked_packet =
			m_active_update_packet.exchange(nullptr, std::memory_order_acq_rel);
		if (leaked_packet) {
			accesskit_tree_update_free(leaked_packet);
		}
	}

	m_worker_thread = std::jthread([this](std::stop_token st) { BackgroundWorkerLoop(st); });

	return true;
}

} // namespace Sral
