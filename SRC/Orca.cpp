#include "Orca.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#if defined(__linux__) && !defined(__ANDROID__)
#include <cstdlib>
#include <cstring>
#include <dbus/dbus.h>
#endif

namespace Sral {

std::atomic<bool> Orca::is_active{false};
std::mutex Orca::orca_mutex;

#if defined(__linux__) && !defined(__ANDROID__)
DBusConnection* Orca::_dbus_connection{nullptr};

struct DBusMessageDeleter {
	void operator()(DBusMessage* msg) const noexcept {
		if (msg) {
			dbus_message_unref(msg);
		}
	}
};
using UniqueDBusMessage = std::unique_ptr<DBusMessage, DBusMessageDeleter>;

class DBusConnectionGuard {
public:
	explicit DBusConnectionGuard(DBusConnection* conn) noexcept : _conn(conn) {}
	~DBusConnectionGuard() {
		if (_conn) {
			if (dbus_connection_get_is_connected(_conn) && dbus_connection_has_messages_to_send(_conn)) {
				dbus_connection_flush(_conn);
			}
			dbus_connection_unref(_conn);
		}
	}
	[[nodiscard]] DBusConnection* get() const noexcept { return _conn; }
	DBusConnectionGuard(const DBusConnectionGuard&) = delete;
	DBusConnectionGuard& operator=(const DBusConnectionGuard&) = delete;
private:
	DBusConnection* _conn;
};
#endif

bool Orca::Initialize() {
	std::lock_guard lock(orca_mutex);
	if (is_active.load(std::memory_order_relaxed)) {
		return true;
	}

#if defined(__linux__) && !defined(__ANDROID__)
	if (std::getenv("DISPLAY") == nullptr && std::getenv("WAYLAND_DISPLAY") == nullptr) {
		return false;
	}

	if (!dbus_threads_init_default()) {
		return false;
	}

	DBusError error;
	dbus_error_init(&error);

	DBusConnection* conn = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
	if (dbus_error_is_set(&error)) {
		dbus_error_free(&error);
		return false;
	}

	if (!conn) {
		dbus_error_free(&error);
		return false;
	}

	dbus_error_free(&error);

	struct InitGuard {
		DBusConnection* c;
		bool success = false;
		~InitGuard() {
			if (c && !success) {
				dbus_connection_close(c);
				dbus_connection_unref(c);
			}
		}
	} init_guard{conn};

	dbus_connection_set_exit_on_disconnect(conn, FALSE);
	
	_dbus_connection = conn;
	is_active.store(true, std::memory_order_release);
	init_guard.success = true;
	return true;
#else
	return false;
#endif
}

bool Orca::Uninitialize() noexcept {
	std::lock_guard lock(orca_mutex);
	if (!is_active.load(std::memory_order_relaxed)) {
		return true;
	}

	is_active.store(false, std::memory_order_release);

#if defined(__linux__) && !defined(__ANDROID__)
	if (_dbus_connection) {
		if (dbus_connection_get_is_connected(_dbus_connection) && dbus_connection_has_messages_to_send(_dbus_connection)) {
			dbus_connection_flush(_dbus_connection);
		}
		dbus_connection_close(_dbus_connection);
		dbus_connection_unref(_dbus_connection);
		_dbus_connection = nullptr;
	}
#endif

	return true;
}

bool Orca::GetActive() noexcept {
	return is_active.load(std::memory_order_acquire);
}

bool Orca::Speak(std::string_view text, bool interrupt) {
	if (!is_active.load(std::memory_order_acquire) || text.empty()) [[unlikely]] {
		return false;
	}

#if defined(__linux__) && !defined(__ANDROID__)
	UniqueDBusMessage stop_msg;
	UniqueDBusMessage msg;

	if (interrupt) {
		stop_msg.reset(dbus_message_new_signal("/org/a11y/atspi/registry", "org.a11y.atspi.Event.Document", "Reload"));
		if (stop_msg) {
			DBusMessageIter stop_iter;
			dbus_message_iter_init_append(stop_msg.get(), &stop_iter);

			const char* stop_detail = "";
			dbus_int32_t stop_int1 = 0;
			dbus_int32_t stop_int2 = 0;
			const char* stop_variant_val = "";

			dbus_message_iter_append_basic(&stop_iter, DBUS_TYPE_STRING, &stop_detail);
			dbus_message_iter_append_basic(&stop_iter, DBUS_TYPE_INT32, &stop_int1);
			dbus_message_iter_append_basic(&stop_iter, DBUS_TYPE_INT32, &stop_int2);

			DBusMessageIter stop_var_iter;
			dbus_message_iter_open_container(&stop_iter, DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &stop_var_iter);
			dbus_message_iter_append_basic(&stop_var_iter, DBUS_TYPE_STRING, &stop_variant_val);
			dbus_message_iter_close_container(&stop_iter, &stop_var_iter);
		}
	}

	msg.reset(dbus_message_new_signal("/org/a11y/atspi/registry", "org.a11y.atspi.Event.Document", "PageChanged"));
	if (!msg) {
		return false;
	}

	DBusMessageIter iter;
	dbus_message_iter_init_append(msg.get(), &iter);

	const char* detail = "announcement";
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &detail);

	dbus_int32_t detail1 = 0;
	dbus_int32_t detail2 = static_cast<dbus_int32_t>(text.size());
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &detail1);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &detail2);

	std::string safe_string_copy(text);
	const char* raw_text_ptr = safe_string_copy.c_str();

	DBusMessageIter variant_iter;
	dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &variant_iter);
	dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &raw_text_ptr);
	dbus_message_iter_close_container(&iter, &variant_iter);

	DBusConnection* raw_conn_ptr = nullptr;

	{
		std::lock_guard lock(orca_mutex);
		if (!_dbus_connection) {
			return false;
		}
		raw_conn_ptr = dbus_connection_ref(_dbus_connection);
	}

	{
		DBusConnectionGuard conn_guard(raw_conn_ptr);
		
		if (interrupt && stop_msg) {
			dbus_connection_send(conn_guard.get(), stop_msg.get(), nullptr);
		}
		dbus_connection_send(conn_guard.get(), msg.get(), nullptr);
	}

	return true;
#else
	(void)interrupt;
	return false;
#endif
}

bool Orca::StopSpeech() {
#if defined(__linux__) && !defined(__ANDROID__)
	if (!is_active.load(std::memory_order_acquire)) {
		return false;
	}

	UniqueDBusMessage msg(
		dbus_message_new_signal("/org/a11y/atspi/registry", "org.a11y.atspi.Event.Document", "Reload"));
	
	if (!msg) [[unlikely]] {
		return false;
	}

	DBusMessageIter iter;
	dbus_message_iter_init_append(msg.get(), &iter);

	const char* detail = "";
	dbus_int32_t dummy_int1 = 0;
	dbus_int32_t dummy_int2 = 0;
	const char* dummy_str = "";

	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &detail);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &dummy_int1);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &dummy_int2);

	DBusMessageIter variant_iter;
	dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &variant_iter);
	dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &dummy_str);
	dbus_message_iter_close_container(&iter, &variant_iter);

	DBusConnection* raw_conn_ptr = nullptr;

	{
		std::lock_guard lock(orca_mutex);
		if (!_dbus_connection) {
			return false; 
		}
		raw_conn_ptr = dbus_connection_ref(_dbus_connection);
	}

	{
		DBusConnectionGuard conn_guard(raw_conn_ptr);
		dbus_connection_send(conn_guard.get(), msg.get(), nullptr);
	}

	return true;
#else
	return false;
#endif
}

bool Orca::Speak(std::nullptr_t, bool) noexcept {
	return false;
}

bool Orca::SpeakSsml(std::string_view ssml, bool interrupt) {
	return Speak(ssml, interrupt);
}

bool Orca::SpeakSsml(std::nullptr_t, bool) noexcept {
	return false;
}

bool Orca::Braille(std::string_view text) {
	return Speak(text, false);
}

bool Orca::Braille(std::nullptr_t) noexcept {
	return false;
}

bool Orca::IsSpeaking() {
	return false;
}

} // namespace Sral
