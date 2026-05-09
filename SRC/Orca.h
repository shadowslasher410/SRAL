#ifndef ORCA_H_
#define ORCA_H_
#pragma once
#include "Engine.h"
#include <dbus/dbus.h>
#include <brlapi.h>

namespace Sral {
    class Orca final : public Engine {
    public:
        bool Initialize() override;
        bool Uninitialize() override;
        bool Speak(const char* text, bool interrupt) override;
        bool Braille(const char* text) override;
        bool StopSpeech() override;
        bool GetActive() override;
        int GetNumber() override { return SRAL_ENGINE_ORCA; }
        int GetFeatures() override { return SRAL_SUPPORTS_SPEECH | SRAL_SUPPORTS_BRAILLE; }

    private:
        DBusConnection* conn = nullptr;
        bool brailleInitialized = false;
    };
}
#endif
