#include "Orca.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#if defined(__linux__) && !defined(__ANDROID__)
#include <cstdlib>
#include <dbus/dbus.h>
#endif

namespace Sral {

std::atomic<bool> Orca::is_active{false};
std::mutex Orca::orca_mutex;
void* Orca::_dbus_connection{nullptr};

#if defined(__linux__) && !defined(__ANDROID__)
struct DBusMessageDeleter {
	void operator()(DBusMessage* msg) const noexcept {
		if (msg) {
			dbus_message_unref(msg);
		}
	}
};
using UniqueDBusMessage = std::unique_ptr<DBusMessage, DBusMessageDeleter>;
#endif

bool Orca::Initialize() {
	std::lock_guard lock(orca_mutex);
	if (is_active.load(std::memory_order_acquire)) {
		return true;
	}

#if defined(__linux__) && !defined(__ANDROID__)
	if (std::getenv("DISPLAY") == nullptr && std::getenv("WAYLAND_DISPLAY") == nullptr) {
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

	_dbus_connection = static_cast<void*>(conn);
	is_active.store(true, std::memory_order_release);
	return true;
#else
	return false;
#endif
}

bool Orca::Uninitialize() noexcept {
	std::lock_guard lock(orca_mutex);
	if (!is_active.load(std::memory_order_acquire)) {
		return true;
	}

#if defined(__linux__) && !defined(__ANDROID__)
	if (_dbus_connection) {
		DBusConnection* conn = static_cast<DBusConnection*>(_dbus_connection);
		dbus_connection_close(conn);
		dbus_connection_unref(conn);
		_dbus_connection = nullptr;
	}
#endif

	is_active.store(false, std::memory_order_release);
	return true;
}

bool Orca::GetActive() noexcept {
	return is_active.load(std::memory_order_acquire);
}

bool Orca::Speak(std::string_view text, bool interrupt) {
	if (!is_active.load(std::memory_order_acquire) || text.empty()) [[unlikely]] {
		return false;
	}

	std::lock_guard lock(orca_mutex);

#if defined(__linux__) && !defined(__ANDROID__)
	DBusConnection* conn = static_cast<DBusConnection*>(_dbus_connection);
	if (!conn) {
		return false;
	}

	if (interrupt) {
		UniqueDBusMessage stop_msg(
			dbus_message_new_signal("/org/a11y/atspi/registry", "org.a11y.atspi.Event.Document", "Reload"));
		if (stop_msg) {
			dbus_connection_send(conn, stop_msg.get(), nullptr);
		}
	}

	UniqueDBusMessage msg(
		dbus_message_new_signal("/org/a11y/atspi/registry", "org.a11y.atspi.Event.Document", "PageChanged"));
	if (!msg) {
		return false;
	}

	DBusMessageIter iter;
	dbus_message_iter_init_append(msg.get(), &iter);

	const char* detail = "announcement";
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &detail);

	std::string null_terminated_text(text);
	const char* raw_text = null_terminated_text.c_str();
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &raw_text);

	dbus_connection_send(conn, msg.get(), nullptr);
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

bool Orca::Braille(std::string_view) {
	return false;
}

bool Orca::Braille(std::nullptr_t) noexcept {
	return false;
}

bool Orca::StopSpeech() {
	if (!is_active.load(std::memory_order_acquire)) {
		return false;
	}

	std::lock_guard lock(orca_mutex);

#if defined(__linux__) && !defined(__ANDROID__)
	DBusConnection* conn = static_cast<DBusConnection*>(_dbus_connection);
	if (!conn) {
		return false;
	}

	UniqueDBusMessage msg(
		dbus_message_new_signal("/org/a11y/atspi/registry", "org.a11y.atspi.Event.Document", "Reload"));
	if (msg) {
		dbus_connection_send(conn, msg.get(), nullptr);
		return true;
	}
#endif
	return false;
}

bool Orca::IsSpeaking() {
	return false;
}

} // namespace Sral
