#include "Orca.h"

namespace Sral {

    bool Orca::Initialize() {
        DBusError err;
        dbus_error_init(&err);
        conn = dbus_bus_get(DBUS_BUS_SESSION, &err);

        if (brlapi_openConnection(nullptr, nullptr) >= 0) {
            if (brlapi_enterTtyMode(BRLAPI_TTY_DEFAULT, nullptr) >= 0) {
                brailleInitialized = true;
            }
            else {
                brlapi_closeConnection();
            }
        }

        return (conn != nullptr);
    }

    bool Orca::Uninitialize() {
        if (brailleInitialized) {
            brlapi_leaveTtyMode();
            brlapi_closeConnection();
            brailleInitialized = false;
        }
        conn = nullptr;
        return true;
    }

    bool Orca::Speak(const char* text, bool interrupt) {
        if (!conn || !text) return false;

        DBusMessage* msg = dbus_message_new_method_call(
            "org.gnome.Orca", "/org/gnome/Orca", "org.gnome.Orca", "presentMessage");

        if (!msg) return false;

        dbus_message_append_args(msg, DBUS_TYPE_STRING, &text, DBUS_TYPE_INVALID);
        dbus_connection_send(conn, msg, nullptr);
        dbus_connection_flush(conn);
        dbus_message_unref(msg);

        return true;
    }

    bool Orca::Braille(const char* text) {
        if (!brailleInitialized || !text) return false;
        return brlapi_writeText(0, text) >= 0;
    }

    bool Orca::GetActive() {
        if (!conn) return false;
        return dbus_bus_name_has_owner(conn, "org.gnome.Orca", nullptr);
    }

    bool Orca::StopSpeech() {
        Braille("");
        return true;
    }

}