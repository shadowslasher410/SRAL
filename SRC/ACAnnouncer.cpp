#include "ACAnnouncer.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <span>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if defined(SRAL_WITH_ACCESSKIT)
#ifdef __cplusplus
extern "C" {
#endif
#if TARGET_OS_IPHONE
bool SralCheckIOSVoiceOverActive();
#else
bool SralCheckMacAccessibilityActive();
#endif
#ifdef __cplusplus
}
#endif
#endif
#endif

namespace Sral {

#if defined(SRAL_WITH_ACCESSKIT)
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
#endif // SRAL_WITH_ACCESSKIT

ACAnnouncer::ACAnnouncer() {
	m_head.store(0, std::memory_order_relaxed);
	m_tail.store(0, std::memory_order_relaxed);
	m_ring_bell.store(false, std::memory_order_relaxed);
#if defined(SRAL_WITH_ACCESSKIT)
	m_use_id_b.store(false, std::memory_order_relaxed);
	m_adapter.store(nullptr, std::memory_order_relaxed);
#endif
}

ACAnnouncer::~ACAnnouncer() {
	Uninitialize();
}

ACAnnouncer::ACAnnouncer(ACAnnouncer&& other) noexcept {
	*this = std::move(other);
}

ACAnnouncer& ACAnnouncer::operator=(ACAnnouncer&& other) noexcept {
	if (this != &other) {
		std::lock_guard lock(m_init_mutex);
		m_head.store(other.m_head.load(std::memory_order_relaxed), std::memory_order_relaxed);
		m_tail.store(other.m_tail.load(std::memory_order_relaxed), std::memory_order_relaxed);
#if defined(SRAL_WITH_ACCESSKIT)
		m_adapter.store(other.m_adapter.exchange(nullptr, std::memory_order_relaxed), std::memory_order_relaxed);
#endif
		m_context_handle = other.m_context_handle;
		other.m_context_handle = nullptr;
	}
	return *this;
}

bool ACAnnouncer::InitializeWithContext(void* window_handle) {
	m_context_handle = window_handle;
	return Initialize();
}

bool ACAnnouncer::Initialize() {
	std::lock_guard lock(m_init_mutex);
#if !defined(SRAL_WITH_ACCESSKIT)
	return false; // Safely reject initialization when compiled without AccessKit support
#else
	void* current_adapter = m_adapter.load(std::memory_order_acquire);
	if (current_adapter) {
		return true;
	}

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
#else
	if (!m_context_handle) [[unlikely]] {
#if !defined(__linux__) && !defined(__unix__)
		return false;
#endif
	}
#endif

	struct accesskit_tree* tree_raw = accesskit_tree_new(WINDOW_ID);
	if (!tree_raw)
		return false;

	struct accesskit_node* window_node = accesskit_node_new(ACCESSKIT_ROLE_WINDOW);
	if (!window_node) {
		accesskit_tree_free(tree_raw);
		return false;
	}
	accesskit_node_push_child(window_node, ANNOUNCEMENT_ID_A);
	accesskit_node_push_child(window_node, ANNOUNCEMENT_ID_B);

	struct accesskit_tree_update* init_update_raw = accesskit_tree_update_new();
	if (!init_update_raw) {
		accesskit_node_free(window_node);
		accesskit_tree_free(tree_raw);
		return false;
	}

	accesskit_tree_update_set_tree(init_update_raw, tree_raw);
	accesskit_tree_update_set_focus(init_update_raw, WINDOW_ID);
	accesskit_tree_update_append_node(init_update_raw, WINDOW_ID, window_node);

	bool loops_succeeded = true;
	for (accesskit_node_id id : {ANNOUNCEMENT_ID_A, ANNOUNCEMENT_ID_B}) {
		struct accesskit_node* node_raw = accesskit_node_new(ACCESSKIT_ROLE_STATUS_BAR);
		if (!node_raw) {
			loops_succeeded = false;
			break;
		}
		accesskit_node_set_live_status(node_raw, ACCESSKIT_LIVE_STATUS_POLITE);
		accesskit_tree_update_append_node(init_update_raw, id, node_raw);
	}

	if (!loops_succeeded) [[unlikely]] {
		accesskit_tree_update_free(init_update_raw);
		return false;
	}

	m_ring_bell.store(false, std::memory_order_relaxed);

#if defined(_WIN32)
	struct accesskit_windows_subclassing_options subclass_options;
	subclass_options.size = sizeof(struct accesskit_windows_subclassing_options);
	current_adapter =
		accesskit_windows_adapter_new(target_window, init_update_raw, &subclass_options, OnActionRequestCallback, this);
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
	current_adapter = accesskit_ios_adapter_new(m_context_handle, init_update_raw, OnActionRequestCallback, this);
#else
	current_adapter = accesskit_macos_adapter_new(m_context_handle, init_update_raw, OnActionRequestCallback, this);
#endif
#elif defined(__ANDROID__)
	current_adapter = accesskit_android_adapter_new(m_context_handle, init_update_raw, OnActionRequestCallback, this);
#else
	current_adapter = accesskit_unix_adapter_new(init_update_raw, OnActionRequestCallback, this);
#endif

	if (!current_adapter) [[unlikely]] {
		return false;
	}

	m_adapter.store(current_adapter, std::memory_order_release);
	m_worker_thread = std::jthread([this](std::stop_token st) { BackgroundWorkerLoop(st); });
	return true;
#endif
}

bool ACAnnouncer::Uninitialize() {
	m_worker_thread.request_stop();
	m_ring_bell.store(true, std::memory_order_release);
	m_ring_bell.notify_all();

	if (m_worker_thread.joinable()) {
		m_worker_thread.join();
	}

	std::lock_guard lock(m_init_mutex);
#if defined(SRAL_WITH_ACCESSKIT)
	void* adapter = m_adapter.exchange(nullptr, std::memory_order_acq_rel);
	if (adapter) {
#if defined(_WIN32)
		accesskit_windows_adapter_free(static_cast<accesskit_windows_adapter*>(adapter));
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
		accesskit_ios_adapter_free(static_cast<accesskit_ios_adapter*>(adapter));
#else
		accesskit_macos_adapter_free(static_cast<accesskit_macos_adapter*>(adapter));
#endif
#elif defined(__ANDROID__)
		accesskit_android_adapter_free(static_cast<accesskit_android_adapter*>(adapter));
#elif defined(__linux__) || defined(__unix__)
		accesskit_unix_adapter_free(static_cast<accesskit_unix_adapter*>(adapter));
#endif
	}
#endif
	return true;
}

bool ACAnnouncer::Speak(const char* text, bool interrupt) {
#if !defined(SRAL_WITH_ACCESSKIT)
	return false;
#else
	if (!text || m_adapter.load(std::memory_order_relaxed) == nullptr)
		return false;

	size_t current_head = m_head.load(std::memory_order_relaxed);
	size_t current_tail = m_tail.load(std::memory_order_acquire);

	if ((current_head - current_tail) >= RING_BUFFER_SIZE) {
		return false;
	}

	size_t index = current_head & RING_MASK;
	SpeechTask& task = m_ring_queue[index];

	std::span<char> target_span(task.text.data(), task.text.size());
	std::size_t text_length = std::strlen(text);
	std::size_t copy_length = std::min(text_length, target_span.size() - 1);

	std::memcpy(target_span.data(), text, copy_length);
	target_span[copy_length] = '\0';

	task.interrupt = interrupt;
	task.sequence.fetch_add(1, std::memory_order_release);

	m_head.store(current_head + 1, std::memory_order_release);
	m_ring_bell.store(true, std::memory_order_release);
	m_ring_bell.notify_all();
	return true;
#endif
}

bool ACAnnouncer::StopSpeech() {
#if !defined(SRAL_WITH_ACCESSKIT)
	return true;
#else
	m_tail.store(m_head.load(std::memory_order_relaxed), std::memory_order_release);

	struct accesskit_tree_update* empty_clear = accesskit_tree_update_new();
	if (empty_clear) {
		struct accesskit_node* root = accesskit_node_new(ACCESSKIT_ROLE_WINDOW);
		accesskit_tree_update_append_node(empty_clear, WINDOW_ID, root);
	}

	void* adapter = m_adapter.load(std::memory_order_acquire);
	if (adapter && empty_clear) {
#if defined(_WIN32)
		accesskit_windows_adapter_update(static_cast<accesskit_windows_adapter*>(adapter), empty_clear);
#elif defined(__linux__) || defined(__unix__)
		accesskit_unix_adapter_update(static_cast<accesskit_unix_adapter*>(adapter), empty_clear);
#else
		accesskit_tree_update_free(empty_clear);
#endif
	}
	else if (empty_clear) {
		accesskit_tree_update_free(empty_clear);
	}
	return true;
#endif
}

void ACAnnouncer::BackgroundWorkerLoop(std::stop_token stop_token) {
#if !defined(SRAL_WITH_ACCESSKIT)
	return;
#else
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
		if (!current_adapter) {
			task.sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
			continue;
		}

		bool use_b = m_use_id_b.load(std::memory_order_relaxed);
		accesskit_node_id active_id = use_b ? ANNOUNCEMENT_ID_B : ANNOUNCEMENT_ID_A;
		accesskit_node_id inactive_id = use_b ? ANNOUNCEMENT_ID_A : ANNOUNCEMENT_ID_B;
		m_use_id_b.store(!use_b, std::memory_order_relaxed);

		struct accesskit_node* window_node = accesskit_node_new(ACCESSKIT_ROLE_WINDOW);
		if (!window_node) {
			task.sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
			continue;
		}
		accesskit_node_push_child(window_node, active_id);
		accesskit_node_push_child(window_node, inactive_id);

		struct accesskit_node* announcement_node = accesskit_node_new(ACCESSKIT_ROLE_STATUS_BAR);
		if (!announcement_node) {
			accesskit_node_free(window_node);
			task.sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
			continue;
		}
		accesskit_node_set_name(announcement_node, task_text);
		accesskit_node_set_live_status(
			announcement_node, task_interrupt ? ACCESSKIT_LIVE_STATUS_ASSERTIVE : ACCESSKIT_LIVE_STATUS_POLITE);

		struct accesskit_node* placeholder_node = accesskit_node_new(ACCESSKIT_ROLE_STATUS_BAR);
		if (!placeholder_node) {
			accesskit_node_free(announcement_node);
			accesskit_node_free(window_node);
			task.sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
			continue;
		}
		accesskit_node_set_name(placeholder_node, " ");

		struct accesskit_tree_update* update_raw = accesskit_tree_update_new();
		if (!update_raw) {
			accesskit_node_free(placeholder_node);
			accesskit_node_free(announcement_node);
			accesskit_node_free(window_node);
			task.sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
			continue;
		}

		accesskit_tree_update_set_focus(update_raw, active_id);

		accesskit_tree_update_append_node(update_raw, WINDOW_ID, window_node);
		accesskit_tree_update_append_node(update_raw, active_id, announcement_node);
		accesskit_tree_update_append_node(update_raw, inactive_id, placeholder_node);

#if defined(_WIN32)
		accesskit_windows_adapter_update(static_cast<struct accesskit_windows_adapter*>(current_adapter), update_raw);
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
		accesskit_ios_adapter_update(static_cast<struct accesskit_ios_adapter*>(current_adapter), update_raw);
#else
		accesskit_macos_adapter_update(static_cast<struct accesskit_macos_adapter*>(current_adapter), update_raw);
#endif
#elif defined(__ANDROID__)
		accesskit_android_adapter_update(static_cast<struct accesskit_android_adapter*>(current_adapter), update_raw);
#else
		accesskit_unix_adapter_update(static_cast<struct accesskit_unix_adapter*>(current_adapter), update_raw);
#endif

		task.sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
	}
#endif
}

bool ACAnnouncer::IsScreenReaderActive() noexcept {
#if !defined(SRAL_WITH_ACCESSKIT)
	return false;
#else
#if defined(_WIN32)
	BOOL screen_running = FALSE;
	if (::SystemParametersInfoW(SPI_GETSCREENREADER, 0, &screen_running, 0)) {
		return screen_running == TRUE;
	}
	return false;
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
	return SralCheckIOSVoiceOverActive();
#else
	return SralCheckMacAccessibilityActive();
#endif
#elif defined(__ANDROID__)
	return true;
#else
	const char* env_at_spi = std::getenv("GTK_MODULES");
	if (env_at_spi && std::strstr(env_at_spi, "gail") != nullptr) {
		return true;
	}
	const char* env_a11y = std::getenv("QT_ACCESSIBILITY");
	if (env_a11y && std::strcmp(env_a11y, "1") == 0) {
		return true;
	}
	return true;
#endif
#endif
}

bool ACAnnouncer::GetActive() {
#if !defined(SRAL_WITH_ACCESSKIT)
	return false;
#else
	return m_adapter.load(std::memory_order_relaxed) != nullptr;
#endif
}

} // namespace Sral
