#ifndef SPEECHDISPATCHER_H_
#define SPEECHDISPATCHER_H_

#include "../Include/SRAL.h"
#include "Engine.h"

#if defined(__linux__)
#include <speech-dispatcher/libspeechd.h>
#include <cstddef>

namespace Sral {

class SpeechDispatcher final : public Engine {
public:
    SpeechDispatcher() noexcept = default;
    ~SpeechDispatcher() override { ClearVoiceList(); }

    SpeechDispatcher(const SpeechDispatcher&) = delete;
    SpeechDispatcher& operator=(const SpeechDispatcher&) = delete;
    SpeechDispatcher(SpeechDispatcher&&) noexcept = delete;
    SpeechDispatcher& operator=(SpeechDispatcher&&) noexcept = delete;

    bool Speak(const char* text, bool interrupt) override;
    bool SpeakSsml(const char* ssml, bool interrupt) override;
    bool Braille(const char* text) override;

    bool IsSpeaking() const noexcept override;
    bool GetActive() const noexcept override;

    bool SetParameter(int param, const void* value) override;
    bool GetParameter(int param, void* value) override;

    bool StopSpeech() noexcept override;
    bool PauseSpeech() noexcept override;
    bool ResumeSpeech() noexcept override;

    int GetNumber() const noexcept override { return SRAL_ENGINE_SPEECH_DISPATCHER; }
    int GetCategory() const noexcept override { return SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE; }
    
    bool Initialize() noexcept override;
    bool Uninitialize() noexcept override;

    int GetFeatures() const noexcept override {
        return SRAL_SUPPORTS_SPEECH | SRAL_SUPPORTS_BRAILLE | SRAL_SUPPORTS_SPEECH_RATE | 
               SRAL_SUPPORTS_SPEECH_VOLUME | SRAL_SUPPORTS_PAUSE_SPEECH | 
               SRAL_SUPPORTS_SPELLING | SRAL_SUPPORTS_SSML | SRAL_SUPPORTS_SELECT_VOICE;
    }

    int GetKeyFlags() const noexcept override { return HANDLE_NONE; }

private:
    SPDConnection* speech = nullptr;
    bool enableSpelling = false;
    bool brailleInitialized = false;

    SPDVoice** m_voiceList = nullptr;
    int m_voiceCount = 0;
    int m_voiceIndex = 0;

    int SetVoiceIndex() noexcept;

    void ClearVoiceList() noexcept {
        if (m_voiceList) {
            free_spd_voices(m_voiceList);
            m_voiceList = nullptr;
        }
        m_voiceCount = 0;
    }

    void RefreshVoiceList() noexcept {
        ClearVoiceList();
        if (!speech) {
            return;
        }
        m_voiceList = spd_list_synthesis_voices(speech);
        if (!m_voiceList) {
            return;
        }
        while (m_voiceList[m_voiceCount] != nullptr) {
            ++m_voiceCount;
        }
    }

    static void SpeechNotificationCallback(size_t msg_id, size_t client_id, SPDNotificationType type) noexcept;
};

} // namespace Sral
#endif
#endif
