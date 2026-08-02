#include "Orca.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <algorithm>

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
        if (msg) [[likely]] {
            dbus_message_unref(msg);
        }
    }
};
using UniqueDBusMessage = std::unique_ptr<DBusMessage, DBusMessageDeleter>;

static void AppendDbusStringVariant(DBusMessageIter* parent_iter, const char* text_ptr) noexcept {
    DBusMessageIter variant_iter;
    static const char sig[] = { DBUS_TYPE_STRING, '\0' };
    dbus_message_iter_open_container(parent_iter, DBUS_TYPE_VARIANT, sig, &variant_iter);
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &text_ptr);
    dbus_message_iter_close_container(parent_iter, &variant_iter);
}
#endif

bool Orca::Initialize() noexcept {
    std::lock_guard lock(orca_mutex);
    if (is_active.load(std::memory_order_acquire)) [[unlikely]] {
        return true;
    }

#if defined(__linux__) && !defined(__ANDROID__)
    if (std::getenv("DISPLAY") == nullptr && std::getenv("WAYLAND_DISPLAY") == nullptr) [[unlikely]] {
        return false;
    }

    if (!dbus_threads_init_default()) [[unlikely]] {
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
    dbus_connection_set_exit_on_disconnect(conn, FALSE);
    
    _dbus_connection = conn;
    is_active.store(true, std::memory_order_release);
    return true;
#else
    return false;
#endif
}

bool Orca::Uninitialize() noexcept {
    std::lock_guard lock(orca_mutex);
    if (!is_active.load(std::memory_order_acquire)) [[unlikely]] {
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

bool Orca::Speak(std::string_view text, bool interrupt) noexcept {
    if (text.empty() || !is_active.load(std::memory_order_acquire)) [[unlikely]] {
        return false;
    }

#if defined(__linux__) && !defined(__ANDROID__)
    DBusConnection* const conn = _dbus_connection;
    if (!conn) [[unlikely]] return false;

    UniqueDBusMessage stop_msg;
    if (interrupt) {
        stop_msg.reset(dbus_message_new_signal("/org/a11y/atspi/registry", "org.a11y.atspi.Event.Document", "Reload"));
        if (stop_msg) [[likely]] {
            DBusMessageIter stop_iter;
            dbus_message_iter_init_append(stop_msg.get(), &stop_iter);

            const char* const stop_detail = "";
            const dbus_int32_t stop_int = 0;
            
            dbus_message_iter_append_basic(&stop_iter, DBUS_TYPE_STRING, &stop_detail);
            dbus_message_iter_append_basic(&stop_iter, DBUS_TYPE_INT32, &stop_int);
            dbus_message_iter_append_basic(&stop_iter, DBUS_TYPE_INT32, &stop_int);
            AppendDbusStringVariant(&stop_iter, stop_detail);
        }
    }

    UniqueDBusMessage msg(dbus_message_new_signal("/org/a11y/atspi/registry", "org.a11y.atspi.Event.Document", "PageChanged"));
    if (!msg) [[unlikely]] return false;

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg.get(), &iter);

    const char* const detail = "announcement";
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &detail);

    const dbus_int32_t detail1 = 0;
    const dbus_int32_t detail2 = static_cast<dbus_int32_t>(text.size());
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &detail1);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &detail2);

    const char* raw_text_ptr = nullptr;
    std::array<char, 512> stack_buffer{};
    
    if (text.data()[text.size()] == '\0') {
        raw_text_ptr = text.data();
    } else {
        const size_t copy_size = (std::min)(text.size(), stack_buffer.size() - 1);
        std::memcpy(stack_buffer.data(), text.data(), copy_size);
        stack_buffer[copy_size] = '\0';
        raw_text_ptr = stack_buffer.data();
    }

    AppendDbusStringVariant(&iter, raw_text_ptr);

    if (interrupt && stop_msg) {
        dbus_connection_send(conn, stop_msg.get(), nullptr);
    }
    dbus_connection_send(conn, msg.get(), nullptr);
    return true;
#else
    (void)interrupt;
    return false;
#endif
}

bool Orca::StopSpeech() noexcept {
#if defined(__linux__) && !defined(__ANDROID__)
    DBusConnection* const conn = _dbus_connection;
    if (!conn || !is_active.load(std::memory_order_acquire)) [[unlikely]] {
        return false;
    }

    UniqueDBusMessage msg(dbus_message_new_signal("/org/a11y/atspi/registry", "org.a11y.atspi.Event.Document", "Reload"));
    if (!msg) [[unlikely]] return false;

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg.get(), &iter);

    const char* const detail = "";
    const dbus_int32_t dummy_int = 0;

    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &detail);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &dummy_int);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &dummy_int);
    AppendDbusStringVariant(&iter, detail);

    dbus_connection_send(conn, msg.get(), nullptr);
    return true;
#else
    return false;
#endif
}

bool Orca::Speak(std::nullptr_t, bool) noexcept {
    return false;
}

bool Orca::SpeakSsml(std::string_view ssml, bool interrupt) noexcept {
    return Speak(ssml, interrupt);
}

bool Orca::SpeakSsml(std::nullptr_t, bool) noexcept {
    return false;
}

bool Orca::Braille(std::string_view text) noexcept {
    return Speak(text, false);
}

bool Orca::Braille(std::nullptr_t) noexcept {
    return false;
}

bool Orca::IsSpeaking() noexcept {
    return false;
}

} // namespace Sral