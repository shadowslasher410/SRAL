#if defined(__linux__)
#include "SpeechDispatcher.h"

#include "../Dep/utf-8.h"
#include "Encoding.h"

#include <atomic>
#include <brlapi.h>
#include <clocale>
#include <cstdlib>
#include <string_view>
#include <string>

std::atomic<bool> g_isSpeaking{false};

namespace Sral {

extern "C" {
    static void SpdBeginCallback(size_t msg_id, size_t client_id) noexcept {
        g_isSpeaking.store(true);
    }

    static void SpdEndOrCancelCallback(size_t msg_id, size_t client_id) noexcept {
        g_isSpeaking.store(false);
    }
}

int SpeechDispatcher::SetVoiceIndex() noexcept {
    RefreshVoiceList();
    if (!m_voiceList || m_voiceCount == 0) return 0;

    const char* system_locale = setlocale(LC_ALL, "");
    if (!system_locale || std::string_view(system_locale) == "C") {
        system_locale = getenv("LANG");
    }
    
    if (!system_locale) return 0;

    std::string system_lang = system_locale;
    
    size_t spec_index = system_lang.find_first_of(".@");
    if (spec_index != std::string::npos) {
        system_lang = system_lang.substr(0, spec_index);
    }
    
    size_t underscore_index = system_lang.find('_');
    if (underscore_index != std::string::npos) {
        system_lang[underscore_index] = '-';
    }

    for (int i = 0; i < m_voiceCount; ++i) {
        if (m_voiceList[i] && m_voiceList[i]->language) {
            std::string voice_lang = m_voiceList[i]->language;
            size_t voice_spec = voice_lang.find_first_of(".@");
            if (voice_spec != std::string::npos) {
                voice_lang = voice_lang.substr(0, voice_spec);
            }
            
            size_t voice_underscore = voice_lang.find('_');
            if (voice_underscore != std::string::npos) {
                voice_lang[voice_underscore] = '-';
            }

            if (voice_lang == system_lang) {
                return i;
            }
        }
    }

    std::string base_system_lang = system_lang.substr(0, 2);
    for (int i = 0; i < m_voiceCount; ++i) {
        if (m_voiceList[i] && m_voiceList[i]->language) {
            std::string voice_lang = m_voiceList[i]->language;
            if (voice_lang.substr(0, 2) == base_system_lang) {
                return i;
            }
        }
    }

    return 0; 
}

bool SpeechDispatcher::Initialize() noexcept {
    const auto* address = spd_get_default_address(nullptr);
    if (address == nullptr) {
        return false;
    }
    speech = spd_open2("SRAL", nullptr, nullptr, SPD_MODE_THREADED, address, true, nullptr);
    if (speech == nullptr) {
        return false;
    }

    spd_set_data_mode(speech, SPD_DATA_SSML);

    speech->callback_begin = SpdBeginCallback;
    speech->callback_end = SpdEndOrCancelCallback;
    speech->callback_cancel = SpdEndOrCancelCallback;
    
    spd_set_notification_on(speech, SPD_BEGIN);
    spd_set_notification_on(speech, SPD_END);
    spd_set_notification_on(speech, SPD_CANCEL);

    int index = this->SetVoiceIndex();
    this->SetParameter(SRAL_PARAM_VOICE_INDEX, &index);
    brailleInitialized = brlapi_openConnection(nullptr, nullptr) >= 0;
    if (brailleInitialized) {
        brlapi_enterTtyMode(BRLAPI_TTY_DEFAULT, nullptr);
    }
    return true;
}

bool SpeechDispatcher::GetActive() const noexcept {
    return speech != nullptr;
}

bool SpeechDispatcher::Uninitialize() noexcept {
    if (speech == nullptr)
        return false;
    g_isSpeaking.store(false);
    ReleaseAllStrings();
    ClearVoiceList();
    m_voiceIndex = 0;
    spd_close(speech);
    speech = nullptr;

    if (brailleInitialized) {
        brlapi_leaveTtyMode();
        brlapi_closeConnection();
        brailleInitialized = false;
    }
    return true;
}

bool SpeechDispatcher::Speak(const char* text, bool interrupt) {
    if (!enableSpelling) {
        std::string text_str(text);
        if (text_str.empty())
            return false;
        XmlEncode(text_str);
        std::string final_ssml = "<speak>" + text_str + "</speak>";
        return this->SpeakSsml(final_ssml.c_str(), interrupt);
    }
    else {
        if (interrupt) {
            spd_stop(speech);
            spd_cancel(speech);
        }

        utf8_iter iter;
        bool result = true;
        utf8_init(&iter, text);
        while (utf8_next(&iter) && result) {
            result = (spd_char(speech, SPD_IMPORTANT, utf8_getchar(&iter)) != -1);
        }
        return result;
    }
}

bool SpeechDispatcher::SpeakSsml(const char* ssml, bool interrupt) {
    if (speech == nullptr)
        return false;
    if (interrupt) {
        spd_stop(speech);
        spd_cancel(speech);
    }

    return spd_say(speech, SPD_IMPORTANT, ssml) != -1;
}

bool SpeechDispatcher::Braille(const char* text) {
    if (!brailleInitialized)
        return false;
    return brlapi_writeText(0, text) >= 0;
}

bool SpeechDispatcher::IsSpeaking() const noexcept {
    return g_isSpeaking.load();
}

bool SpeechDispatcher::SetParameter(int param, const void* value) {
    if (speech == nullptr)
        return false;
    switch (param) {
    case SRAL_PARAM_SYMBOL_LEVEL:
        spd_set_punctuation(speech, static_cast<SPDPunctuation>(*reinterpret_cast<const int*>(value)));
        break;
    case SRAL_PARAM_SPEECH_RATE:
        spd_set_voice_rate(speech, *reinterpret_cast<const int*>(value));
        break;
    case SRAL_PARAM_SPEECH_VOLUME:
        spd_set_volume(speech, *reinterpret_cast<const int*>(value));
        break;
    case SRAL_PARAM_ENABLE_SPELLING:
        this->enableSpelling = *reinterpret_cast<const bool*>(value);
        break;
    case SRAL_PARAM_VOICE_INDEX: {
        RefreshVoiceList();
        if (!m_voiceList)
            return false;
        int index = *reinterpret_cast<const int*>(value);
        if (index >= 0 && index < m_voiceCount && spd_set_synthesis_voice(speech, m_voiceList[index]->name) == 0) {
            m_voiceIndex = index;
            return true;
        }
        return false;
    }
    default:
        return false;
    }
    return true;
}

bool SpeechDispatcher::GetParameter(int param, void* value) {
    if (speech == nullptr)
        return false;
    switch (param) {
    case SRAL_PARAM_SPEECH_RATE:
        *static_cast<int*>(value) = spd_get_voice_rate(speech);
        return true;
    case SRAL_PARAM_SPEECH_VOLUME:
        *static_cast<int*>(value) = spd_get_volume(speech);
        return true;
    case SRAL_PARAM_ENABLE_SPELLING:
        *static_cast<bool*>(value) = this->enableSpelling;
        return true;
    case SRAL_PARAM_VOICE_PROPERTIES: {
        ReleaseAllStrings();
        auto* voiceProperties = static_cast<SRAL_VoiceInfo*>(value);
        RefreshVoiceList();
        for (int index = 0; voiceProperties && m_voiceList && index < m_voiceCount; ++index) {
            voiceProperties[index].index = index;
            voiceProperties[index].name = AddString(m_voiceList[index]->name);
            voiceProperties[index].language = AddString(m_voiceList[index]->language);
            voiceProperties[index].gender = AddString(m_voiceList[index]->variant);
            voiceProperties[index].vendor = AddString("Unknown");
        }
        return true;
    }
    case SRAL_PARAM_VOICE_COUNT:
        RefreshVoiceList();
        *static_cast<int*>(value) = m_voiceCount;
        return true;
    case SRAL_PARAM_VOICE_INDEX:
        *static_cast<int*>(value) = m_voiceIndex;
        return true;
    default:
        return false;
    }
}

bool SpeechDispatcher::StopSpeech() noexcept {
    if (speech == nullptr)
        return false;
    spd_stop(speech);
    spd_cancel(speech);
    return true;
}

bool SpeechDispatcher::PauseSpeech() noexcept {
    if (!GetActive())
        return false;
    return spd_pause(speech) == 0;
}

bool SpeechDispatcher::ResumeSpeech() noexcept {
    if (!GetActive())
        return false;
    return spd_resume(speech) == 0;
}

void SpeechDispatcher::SpeechNotificationCallback(size_t msg_id, size_t client_id, SPDNotificationType type) noexcept {
    switch (type) {
    case SPD_EVENT_BEGIN:
        g_isSpeaking.store(true);
        break;
    case SPD_EVENT_END:
    case SPD_EVENT_CANCEL:
        g_isSpeaking.store(false);
        break;
    default:
        return;
    }
}

} // namespace Sral
#endif
