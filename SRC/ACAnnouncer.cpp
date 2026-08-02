#include "ACAnnouncer.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <span>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Sral {

#if defined(SRAL_WITH_ACCESSKIT)
namespace {
struct NodeGuard {
	struct accesskit_node* ptr = nullptr;
	NodeGuard() = default;
	explicit NodeGuard(struct accesskit_node* p) noexcept : ptr(p) {}
	~NodeGuard() {
		if (ptr)
			accesskit_node_free(ptr);
	}
	struct accesskit_node* release() noexcept {
		auto* p = ptr;
		ptr = nullptr;
		return p;
	}
	explicit operator bool() const noexcept { return ptr != nullptr; }
};

struct UpdateGuard {
	struct accesskit_tree_update* ptr = nullptr;
	UpdateGuard() = default;
	explicit UpdateGuard(struct accesskit_tree_update* p) noexcept : ptr(p) {}
	~UpdateGuard() {
		if (ptr)
			accesskit_tree_update_free(ptr);
	}
	struct accesskit_tree_update* release() noexcept {
		auto* p = ptr;
		ptr = nullptr;
		return p;
	}
	explicit operator bool() const noexcept { return ptr != nullptr; }
};
} // namespace

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

ACAnnouncer::~ACAnnouncer() noexcept {
	(void)Uninitialize();
}

bool ACAnnouncer::InitializeWithContext(void* window_handle) {
	m_context_handle = window_handle;
	return Initialize();
}

bool ACAnnouncer::Initialize() {
	std::lock_guard<std::mutex> lock(m_init_mutex);
#if !defined(SRAL_WITH_ACCESSKIT)
	return false;
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
#endif

	struct accesskit_tree* tree_raw = accesskit_tree_new(WINDOW_ID);
	if (!tree_raw)
		return false;

	NodeGuard window_node(accesskit_node_new(ACCESSKIT_ROLE_WINDOW));
	if (!window_node) {
		accesskit_tree_free(tree_raw);
		return false;
	}
	accesskit_node_push_child(window_node.ptr, ANNOUNCEMENT_ID_A);
	accesskit_node_push_child(window_node.ptr, ANNOUNCEMENT_ID_B);

	UpdateGuard init_update(accesskit_tree_update_new());
	if (!init_update) {
		accesskit_tree_free(tree_raw);
		return false;
	}

	accesskit_tree_update_set_tree(init_update.ptr, tree_raw);
	accesskit_tree_update_set_focus(init_update.ptr, WINDOW_ID);
	accesskit_tree_update_append_node(init_update.ptr, WINDOW_ID, window_node.release());

	bool loops_succeeded = true;
	for (accesskit_node_id id : {ANNOUNCEMENT_ID_A, ANNOUNCEMENT_ID_B}) {
		NodeGuard node_raw(accesskit_node_new(ACCESSKIT_ROLE_STATUS_BAR));
		if (!node_raw) {
			loops_succeeded = false;
			break;
		}
		accesskit_node_set_live_status(node_raw.ptr, ACCESSKIT_LIVE_STATUS_POLITE);
		accesskit_tree_update_append_node(init_update.ptr, id, node_raw.release());
	}

	if (!loops_succeeded) [[unlikely]] {
		return false;
	}

	m_ring_bell.store(false, std::memory_order_relaxed);
	struct accesskit_tree_update* init_payload = init_update.release();

#if defined(_WIN32)
	struct accesskit_windows_subclassing_options subclass_options;
	subclass_options.size = sizeof(struct accesskit_windows_subclassing_options);
	current_adapter =
		accesskit_windows_adapter_new(target_window, init_payload, &subclass_options, OnActionRequestCallback, this);
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
	current_adapter = accesskit_ios_adapter_new(m_context_handle, init_payload, OnActionRequestCallback, this);
#else
	current_adapter = accesskit_macos_adapter_new(m_context_handle, init_payload, OnActionRequestCallback, this);
#endif
#elif defined(__ANDROID__)
	current_adapter = accesskit_android_adapter_new(m_context_handle, init_payload, OnActionRequestCallback, this);
#else
	current_adapter = accesskit_unix_adapter_new(init_payload, OnActionRequestCallback, this);
#endif

	if (!current_adapter) [[unlikely]] {
		accesskit_tree_update_free(init_payload);
		return false;
	}

	m_adapter.store(current_adapter, std::memory_order_release);
	m_worker_thread = std::jthread([this](std::stop_token st) noexcept { this->BackgroundWorkerLoop(st); });
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
	std::lock_guard<std::mutex> lock(m_init_mutex);
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
	(void)text;
	(void)interrupt;
	return false;
#else
	if (text == nullptr || m_adapter.load(std::memory_order_relaxed) == nullptr) [[unlikely]] {
		return false;
	}

	size_t current_head = m_head.load(std::memory_order_relaxed);
	size_t current_tail = m_tail.load(std::memory_order_acquire);

	if ((current_head - current_tail) >= RING_BUFFER_SIZE) [[unlikely]] {
		return false;
	}

	size_t index = current_head & RING_MASK;
	SpeechTask* const task = &m_ring_queue[index];

	std::span<char> target_span(task->text.data(), task->text.size());
	const std::size_t text_length = std::strlen(text);
	const std::size_t copy_length = (std::min)(text_length, target_span.size() - 1);

	std::memcpy(target_span.data(), text, copy_length);
	target_span[copy_length] = '\0';

	task->interrupt = interrupt;
	task->sequence.store(current_head + 1, std::memory_order_release);
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

	UpdateGuard empty_clear(accesskit_tree_update_new());
	if (!empty_clear) [[unlikely]]
		return false;

	NodeGuard root(accesskit_node_new(ACCESSKIT_ROLE_WINDOW));
	if (root) {
		accesskit_node_push_child(root.ptr, ANNOUNCEMENT_ID_A);
		accesskit_node_push_child(root.ptr, ANNOUNCEMENT_ID_B);
		accesskit_tree_update_append_node(empty_clear.ptr, WINDOW_ID, root.release());
	}

	for (accesskit_node_id id : {ANNOUNCEMENT_ID_A, ANNOUNCEMENT_ID_B}) {
		NodeGuard node_raw(accesskit_node_new(ACCESSKIT_ROLE_STATUS_BAR));
		if (node_raw) {
			accesskit_node_set_name(node_raw.ptr, "");
			accesskit_tree_update_append_node(empty_clear.ptr, id, node_raw.release());
		}
	}

	void* const adapter = m_adapter.load(std::memory_order_acquire);
	if (adapter) {
		struct accesskit_tree_update* const payload = empty_clear.release();
#if defined(_WIN32)
		accesskit_windows_adapter_update(static_cast<struct accesskit_windows_adapter*>(adapter), payload);
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
		accesskit_ios_adapter_update(static_cast<struct accesskit_ios_adapter*>(adapter), payload);
#else
		accesskit_macos_adapter_update(static_cast<struct accesskit_macos_adapter*>(adapter), payload);
#endif
#elif defined(__ANDROID__)
		accesskit_android_adapter_update(static_cast<struct accesskit_android_adapter*>(adapter), payload);
#elif defined(__linux__) || defined(__unix__)
		accesskit_unix_adapter_update(static_cast<struct accesskit_unix_adapter*>(adapter), payload);
#else
		accesskit_tree_update_free(payload);
#endif
	}
	return true;
#endif
}

void ACAnnouncer::BackgroundWorkerLoop(std::stop_token stop_token) noexcept {
#if !defined(SRAL_WITH_ACCESSKIT)
	(void)stop_token;
	return;
#else
	while (!stop_token.stop_requested()) [[likely]] {
		size_t current_tail = m_tail.load(std::memory_order_relaxed);
		size_t index = current_tail & RING_MASK;
		SpeechTask* const task = &m_ring_queue[index];

		size_t seq = task->sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1);

		if (difference != 0) {
			m_ring_bell.store(false, std::memory_order_release);

			seq = task->sequence.load(std::memory_order_acquire);
			if (static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1) != 0) {
				m_ring_bell.wait(false, std::memory_order_acquire);
			}
			else {
				m_ring_bell.store(true, std::memory_order_release);
			}
			if (stop_token.stop_requested()) [[unlikely]]
				break;
			continue;
		}

		const char* const task_text = task->text.data();
		const bool task_interrupt = task->interrupt;

		m_tail.store(current_tail + 1, std::memory_order_release);

		void* const current_adapter = m_adapter.load(std::memory_order_acquire);
		if (current_adapter == nullptr) {
			task->sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
			continue;
		}

		const bool use_b = m_use_id_b.load(std::memory_order_relaxed);
		const accesskit_node_id active_id = use_b ? ANNOUNCEMENT_ID_B : ANNOUNCEMENT_ID_A;
		const accesskit_node_id inactive_id = use_b ? ANNOUNCEMENT_ID_A : ANNOUNCEMENT_ID_B;
		m_use_id_b.store(!use_b, std::memory_order_relaxed);

		NodeGuard window_node(accesskit_node_new(ACCESSKIT_ROLE_WINDOW));
		if (!window_node) {
			task->sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
			continue;
		}
		accesskit_node_push_child(window_node.ptr, active_id);
		accesskit_node_push_child(window_node.ptr, inactive_id);

		NodeGuard announcement_node(accesskit_node_new(ACCESSKIT_ROLE_STATUS_BAR));
		if (!announcement_node) {
			task->sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
			continue;
		}
		accesskit_node_set_name(announcement_node.ptr, task_text);
		accesskit_node_set_live_status(
			announcement_node.ptr, task_interrupt ? ACCESSKIT_LIVE_STATUS_ASSERTIVE : ACCESSKIT_LIVE_STATUS_POLITE);

		NodeGuard placeholder_node(accesskit_node_new(ACCESSKIT_ROLE_STATUS_BAR));
		if (!placeholder_node) {
			task->sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
			continue;
		}
		accesskit_node_set_name(placeholder_node.ptr, " ");

		UpdateGuard update_raw(accesskit_tree_update_new());
		if (!update_raw) {
			task->sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
			continue;
		}

		accesskit_tree_update_set_focus(update_raw.ptr, active_id);
		accesskit_tree_update_append_node(update_raw.ptr, WINDOW_ID, window_node.release());
		accesskit_tree_update_append_node(update_raw.ptr, active_id, announcement_node.release());
		accesskit_tree_update_append_node(update_raw.ptr, inactive_id, placeholder_node.release());

		struct accesskit_tree_update* const update_payload = update_raw.release();

#if defined(_WIN32)
		accesskit_windows_adapter_update(
			static_cast<struct accesskit_windows_adapter*>(current_adapter), update_payload);
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
		accesskit_ios_adapter_update(static_cast<struct accesskit_ios_adapter*>(current_adapter), update_payload);
#else
		accesskit_macos_adapter_update(static_cast<struct accesskit_macos_adapter*>(current_adapter), update_payload);
#endif
#elif defined(__ANDROID__)
		accesskit_android_adapter_update(
			static_cast<struct accesskit_android_adapter*>(current_adapter), update_payload);
#else
		accesskit_unix_adapter_update(static_cast<struct accesskit_unix_adapter*>(current_adapter), update_payload);
#endif

		task->sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
	}
#endif
}

bool ACAnnouncer::GetActive() {
#if !defined(SRAL_WITH_ACCESSKIT)
	return false;
#else
	return m_adapter.load(std::memory_order_acquire) != nullptr && IsScreenReaderActive();
#endif
}

bool ACAnnouncer::IsScreenReaderActive() noexcept {
#if !defined(SRAL_WITH_ACCESSKIT)
	return false;
#else
#if defined(_WIN32)
	BOOL screen_running = FALSE;
	if (::SystemParametersInfoW(SPI_GETSCREENREADER, 0, &screen_running, 0)) [[likely]] {
		return screen_running == TRUE;
	}
	return false;
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
	return ::UIAccessibilityIsVoiceOverRunning() == YES;
#else
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101000
	return ::NSAccessibilityIsVoiceOverRunning() == YES;
#else
	return ::AXIsProcessTrusted() == YES;
#endif
#endif
#elif defined(__ANDROID__)
	return Sral::IsAndroidAccessibilityActive();
#else
	static const bool is_a11y_env_active = []() noexcept -> bool {
		const char* const env_at_spi = std::getenv("GTK_MODULES");
		if (env_at_spi && std::strstr(env_at_spi, "gail") != nullptr)
			return true;
		const char* const env_a11y = std::getenv("QT_ACCESSIBILITY");
		if (env_a11y && std::strcmp(env_a11y, "1") == 0)
			return true;
		const char* const env_desktop = std::getenv("ACCESSIBILITY_ENABLED");
		if (env_desktop && std::strcmp(env_desktop, "1") == 0)
			return true;
		return false;
	}();

	return is_a11y_env_active;
#endif
#endif
}

} // namespace Sral
