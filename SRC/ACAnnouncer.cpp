#include "ACAnnouncer.h"

#include <iostream>

template <typename T, void (*Deleter)(T*)> struct AccessKitUniquePtr {
	struct CustomDeleter {
		void operator()(T* ptr) const {
			if (ptr)
				Deleter(ptr);
		}
	};
	using Type = std::unique_ptr<T, CustomDeleter>;
};

ACAnnouncer::ACAnnouncer() = default;

ACAnnouncer::~ACAnnouncer() {
	Uninitialize();
}

ACAnnouncer::ACAnnouncer(ACAnnouncer&& other) noexcept {
	std::lock_guard lock(other.m_mutex);

	if (other.m_adapter) {
		std::cerr << "[ACAnnouncer] Critical: Refusing move-construction of an active announcer instance.\n";
		return;
	}

	m_bound_window = other.m_bound_window;
	m_queue = std::move(other.m_queue);
	m_use_id_b = other.m_use_id_b;
	other.m_bound_window = nullptr;
}

ACAnnouncer& ACAnnouncer::operator=(ACAnnouncer&& other) noexcept {
	if (this != &other) {
		Uninitialize();
		std::scoped_lock lock(m_mutex, other.m_mutex);

		if (other.m_adapter) {
			std::cerr << "[ACAnnouncer] Critical: Refusing move-assignment of an active announcer instance.\n";
			return *this;
		}

		m_bound_window = other.m_bound_window;
		m_queue = std::move(other.m_queue);
		m_use_id_b = other.m_use_id_b;
		other.m_bound_window = nullptr;
	}
	return *this;
}

void ACAnnouncer::OnActionRequestCallback(struct accesskit_action_request* request, void* userdata) {
	if (userdata) {
		static_cast<ACAnnouncer*>(userdata)->HandleActionRequest(request);
	}
	else if (request) {
		accesskit_action_request_free(request);
	}
}

struct accesskit_tree_update* ACAnnouncer::ProvideUpdateCallback(void* userdata) {
	if (userdata) {
		return static_cast<ACAnnouncer*>(userdata)->InterceptUpdatePayload();
	}
	return nullptr;
}

void ACAnnouncer::HandleActionRequest(struct accesskit_action_request* request) noexcept {
	if (request) {
		accesskit_action_request_free(request);
	}
}

struct accesskit_tree_update* ACAnnouncer::InterceptUpdatePayload() noexcept {
	return m_active_update_packet;
}

bool ACAnnouncer::IsScreenReaderActive() noexcept {
	BOOL screenReaderRunning = FALSE;
	if (::SystemParametersInfoW(SPI_GETSCREENREADER, 0, &screenReaderRunning, 0)) {
		return screenReaderRunning == TRUE;
	}
	return false;
}

bool ACAnnouncer::Initialize() {
	std::lock_guard lock(m_mutex);
	if (m_adapter)
		return true;

	HWND target_window = m_bound_window;
	if (!target_window) {
		target_window = ::GetForegroundWindow();
		if (!target_window)
			return false;

		DWORD current_process_id = ::GetCurrentProcessId();
		DWORD window_process_id = 0;
		::GetWindowThreadProcessId(target_window, &window_process_id);

		if (current_process_id != window_process_id) {
			std::cerr << "[ACAnnouncer] Guard Blocked: Refusing cross-process engine assignment.\n";
			return false;
		}
		m_bound_window = target_window;
	}

	m_adapter = accesskit_windows_adapter_new(target_window, true, OnActionRequestCallback, static_cast<void*>(this));
	if (!m_adapter)
		return false;

	accesskit_tree* tree_raw = accesskit_tree_new(WINDOW_ID);
	if (tree_raw) {
		typename AccessKitUniquePtr<accesskit_tree, accesskit_tree_free>::Type tree(tree_raw);
		accesskit_tree_set_app_name(tree.get(), "ACAnnouncer");

		accesskit_node* window_raw = accesskit_node_new(ACCESSKIT_ROLE_WINDOW);
		if (window_raw) {
			typename AccessKitUniquePtr<accesskit_node, accesskit_node_free>::Type window_node(window_raw);
			accesskit_node_set_role(window_node.get(), ACCESSKIT_ROLE_WINDOW);
			accesskit_node_push_child(window_node.get(), ANNOUNCEMENT_ID_A);
			accesskit_node_push_child(window_node.get(), ANNOUNCEMENT_ID_B);

			accesskit_tree_update* init_update_raw = accesskit_tree_update_with_capacity_and_focus(1, WINDOW_ID);
			if (init_update_raw) {
				typename AccessKitUniquePtr<accesskit_tree_update, accesskit_tree_update_free>::Type init_update(
					init_update_raw);
				accesskit_tree_update_set_tree(init_update.get(), tree.release());
				accesskit_tree_update_push_node(init_update.get(), WINDOW_ID, window_node.release());

				m_active_update_packet = init_update.get();
				bool status = accesskit_windows_adapter_update_if_active(
					m_adapter, ProvideUpdateCallback, static_cast<void*>(this));
				m_active_update_packet = nullptr;

				if (status) {
					init_update.release();
				}
			}
		}
	}

	m_worker_thread = std::jthread(&ACAnnouncer::BackgroundWorkerLoop, this);
	return true;
}

bool ACAnnouncer::Uninitialize() {
	std::jthread thread_to_join;
	accesskit_windows_adapter* adapter_to_free = nullptr;

	{
		std::lock_guard lock(m_mutex);
		if (!m_adapter)
			return true;

		m_worker_thread.request_stop();
		std::queue<SpeechTask>().swap(m_queue);
		adapter_to_free = m_adapter;
		m_adapter = nullptr;
		m_semaphore.release();
		thread_to_join = std::move(m_worker_thread);
	}

	if (thread_to_join.joinable()) {
		thread_to_join.join();
	}

	if (adapter_to_free) {
		accesskit_windows_adapter_free(adapter_to_free);
	}

	std::lock_guard lock(m_mutex);
	m_bound_window = nullptr;
	return true;
}

bool ACAnnouncer::Speak(const char* text, bool interrupt) {
	if (!text)
		text = "";

	{
		std::unique_lock lock(m_mutex);
		if (!m_adapter) {
			if (!IsScreenReaderActive())
				return false;

			lock.unlock();
			if (!Initialize())
				return false;
		}
	}

	std::lock_guard lock(m_mutex);
	if (m_worker_thread.get_stop_token().stop_requested())
		return false;

	if (interrupt) {
		std::queue<SpeechTask>().swap(m_queue);
	}

	m_queue.push(SpeechTask{.text = std::string(text), .interrupt = interrupt});
	m_semaphore.release();
	return true;
}

bool ACAnnouncer::StopSpeech() {
	return Speak("", true);
}

bool ACAnnouncer::GetActive() {
	std::lock_guard lock(m_mutex);
	return m_adapter != nullptr && !m_worker_thread.get_stop_token().stop_requested();
}

void ACAnnouncer::BackgroundWorkerLoop(std::stop_token stop_token) {
	while (!stop_token.stop_requested()) {
		m_semaphore.acquire();

		SpeechTask task;
		accesskit_windows_adapter* current_adapter = nullptr;
		accesskit_node_id active_id = ANNOUNCEMENT_ID_A;
		accesskit_node_id inactive_id = ANNOUNCEMENT_ID_B;

		{
			std::lock_guard lock(m_mutex);
			if (stop_token.stop_requested() && m_queue.empty()) {
				break;
			}

			if (m_queue.empty())
				continue;

			task = std::move(m_queue.front());
			m_queue.pop();
			current_adapter = m_adapter;

			active_id = m_use_id_b ? ANNOUNCEMENT_ID_B : ANNOUNCEMENT_ID_A;
			inactive_id = m_use_id_b ? ANNOUNCEMENT_ID_A : ANNOUNCEMENT_ID_B;
			m_use_id_b = !m_use_id_b;
		}

		if (!current_adapter)
			continue;

		accesskit_node* window_raw = accesskit_node_new(ACCESSKIT_ROLE_WINDOW);
		if (!window_raw)
			continue;
		typename AccessKitUniquePtr<accesskit_node, accesskit_node_free>::Type window_node(window_raw);
		accesskit_node_set_role(window_node.get(), ACCESSKIT_ROLE_WINDOW);
		accesskit_node_push_child(window_node.get(), ANNOUNCEMENT_ID_A);
		accesskit_node_push_child(window_node.get(), ANNOUNCEMENT_ID_B);

		accesskit_node* announcement_raw = accesskit_node_new(ACCESSKIT_ROLE_LABEL);
		if (!announcement_raw)
			continue;
		typename AccessKitUniquePtr<accesskit_node, accesskit_node_free>::Type announcement_node(announcement_raw);
		accesskit_node_set_role(announcement_node.get(), ACCESSKIT_ROLE_LABEL);
		accesskit_node_set_label(announcement_node.get(), task.text.c_str());
		accesskit_node_set_live(
			announcement_node.get(), task.interrupt ? ACCESSKIT_LIVE_ASSERTIVE : ACCESSKIT_LIVE_POLITE);

		accesskit_node* placeholder_raw = accesskit_node_new(ACCESSKIT_ROLE_LABEL);
		if (!placeholder_raw)
			continue;
		typename AccessKitUniquePtr<accesskit_node, accesskit_node_free>::Type placeholder_node(placeholder_raw);
		accesskit_node_set_role(placeholder_node.get(), ACCESSKIT_ROLE_LABEL);
		accesskit_node_set_label(placeholder_node.get(), "");
		accesskit_node_set_live(placeholder_node.get(), ACCESSKIT_LIVE_OFF);

		accesskit_tree_update* update_raw = accesskit_tree_update_with_capacity_and_focus(3, active_id);
		if (!update_raw)
			continue;

		typename AccessKitUniquePtr<accesskit_tree_update, accesskit_tree_update_free>::Type update(update_raw);
		accesskit_tree_update_push_node(update.get(), WINDOW_ID, window_node.release());
		accesskit_tree_update_push_node(update.get(), active_id, announcement_node.release());
		accesskit_tree_update_push_node(update.get(), inactive_id, placeholder_node.release());

		m_active_update_packet = update.get();
		bool status = accesskit_windows_adapter_update_if_active(
			current_adapter, ProvideUpdateCallback, static_cast<void*>(this));
		m_active_update_packet = nullptr;

		if (status) {
			update.release();
		}
	}
}
