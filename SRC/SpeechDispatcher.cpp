#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

#include "SpeechDispatcher.h"
#include "../Dep/utf-8.h"
#include "Encoding.h"

#if defined(__linux__) && !defined(__ANDROID__)
#include <brlapi.h>
#include <speech-dispatcher/libspeechd.h>
#endif

namespace Sral {

std::atomic<bool> SpeechDispatcher::is_active{false};
std::mutex SpeechDispatcher::speechd_mutex;
std::atomic<size_t> SpeechDispatcher::m_activeMsgId{0};

SpeechDispatcher::~SpeechDispatcher() {
    Uninitialize();
    ClearVoiceList();
}

int SpeechDispatcher::SetVoiceIndex() noexcept {
    RefreshVoiceList();
    if (!m_voiceList || m_voiceCount == 0) return 0;

#if defined(__linux__) && !defined(__ANDROID__)
    const char* system_locale = std::getenv("LC_ALL");
    if (!system_locale || std::string_view(system_locale) == "C") {
        system_locale = std::getenv("LC_CTYPE");
        if (!system_locale || std::string_view(system_locale) == "C") {
            system_locale = std::getenv("LANG");
        }
    }
    
    if (!system_locale) return 0;

    std::string_view system_lang_view(system_locale);
    
    size_t spec_index = system_lang_view.find_first_of(".@");
    if (spec_index != std::string_view::npos) {
        system_lang_view = system_lang_view.substr(0, spec_index);
    }
    
    std::string system_lang(system_lang_view);
    size_t underscore_index = system_lang.find('_');
    if (underscore_index != std::string::npos) {
        system_lang[underscore_index] = '-';
    }

    for (int i = 0; i < m_voiceCount; ++i) {
        if (m_voiceList[i] && m_voiceList[i]->language) {
            std::string_view voice_lang_view(m_voiceList[i]->language);
            size_t voice_spec = voice_lang_view.find_first_of(".@");
            if (voice_spec != std::string_view::npos) {
                voice_lang_view = voice_lang_view.substr(0, voice_spec);
            }
            
            std::string voice_lang(voice_lang_view);
            size_t voice_underscore = voice_lang.find('_');
            if (voice_underscore != std::string::npos) {
                voice_lang[voice_underscore] = '-';
            }

            if (voice_lang == system_lang) {
                return i;
            }
        }
    }

    if (system_lang.size() >= 2) {
        std::string_view base_system_lang = std::string_view(system_lang).substr(0, 2);
        for (int i = 0; i < m_voiceCount; ++i) {
            if (m_voiceList[i] && m_voiceList[i]->language) {
                std::string_view voice_lang_view(m_voiceList[i]->language);
                if (voice_lang_view.size() >= 2 && voice_lang_view.substr(0, 2) == base_system_lang) {
                    return i;
                }
            }
        }
    }
#endif

    return 0; 
}
bool SpeechDispatcher::Initialize() {
    std::lock_guard lock(m_mutex);
    if (is_active.load(std::memory_order_acquire)) return true;

    for (size_t i = 0; i < RING_BUFFER_SIZE; ++i) {
        m_ring_queue[i].sequence.store(i, std::memory_order_relaxed);
    }

#if defined(__linux__) && !defined(__ANDROID__)
    if (speech != nullptr) return true;

    const auto* address = spd_get_default_address(nullptr);
    if (address == nullptr) return false;

    speech = spd_open2("SRAL", nullptr, nullptr, SPD_MODE_SINGLE, address, true, &SpeechDispatcher::SpeechNotificationCallback);
    if (speech == nullptr) return false;

    spd_set_data_mode(speech, SPD_DATA_SSML);

    brailleInitialized = brlapi_openConnection(nullptr, nullptr) >= 0;
    if (brailleInitialized) {
        brlapi_enterTtyMode(BRLAPI_TTY_DEFAULT, nullptr);
    }
#endif

    is_active.store(true, std::memory_order_release);
    
    m_worker_thread = std::jthread([this](std::stop_token st) {
        this->BackgroundWorkerLoop(st);
    });

    return true;
}

bool SpeechDispatcher::Uninitialize() {
    {
        std::lock_guard lock(m_mutex);
        if (!is_active.load(std::memory_order_relaxed)) return true;
        is_active.store(false, std::memory_order_release);
    }

    m_worker_thread.request_stop();
    m_ring_bell.store(true, std::memory_order_release);
    m_ring_bell.notify_all();
    
    if (m_worker_thread.joinable()) {
        m_worker_thread.join();
    }

    std::lock_guard lock(m_mutex);
#if defined(__linux__) && !defined(__ANDROID__)
    if (speech != nullptr) {
        spd_close(speech);
        speech = nullptr;
    }
    if (brailleInitialized) {
        brlapi_leaveTtyMode();
        brlapi_closeConnection();
        brailleInitialized = false;
    }
#endif
    ClearStringPool();
    return true;
}

bool SpeechDispatcher::Speak(const char* text, bool interrupt) {
    if (!text || !is_active.load(std::memory_order_relaxed)) return false;
    const char* internal_str = AddString(text);
    if (!internal_str) return false;

    size_t ticket = m_head.load(std::memory_order_relaxed);
    AsyncSpdTask* task = nullptr;

    while (true) {
        task = &m_ring_queue[ticket & RING_MASK];
        size_t seq = task->sequence.load(std::memory_order_acquire);
        intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

        if (difference == 0) {
            if (m_head.compare_exchange_strong(ticket, ticket + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                break;
            }
        } else if (difference < 0) {
            std::this_thread::yield();
            ticket = m_head.load(std::memory_order_relaxed);
        } else {
            ticket = m_head.load(std::memory_order_relaxed);
        }
    }

    task->text_ptr = internal_str; 
    task->type = TaskType::Speak;
    task->interrupt = interrupt;
    
    task->sequence.store(ticket + 1, std::memory_order_release);
    
    m_ring_bell.store(true, std::memory_order_release);
    m_ring_bell.notify_one();
    return true;
}

bool SpeechDispatcher::SpeakSsml(const char* ssml, bool interrupt) {
    if (!ssml || !is_active.load(std::memory_order_relaxed)) return false;

    const char* internal_ssml = AddString(ssml);
    if (!internal_ssml) return false;

    size_t ticket = m_head.load(std::memory_order_relaxed);
    AsyncSpdTask* task = nullptr;

    while (true) {
        task = &m_ring_queue[ticket & RING_MASK];
        size_t seq = task->sequence.load(std::memory_order_acquire);
        intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

        if (difference == 0) {
            if (m_head.compare_exchange_strong(ticket, ticket + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                break;
            }
        } else if (difference < 0) {
            std::this_thread::yield();
            ticket = m_head.load(std::memory_order_relaxed);
        } else {
            ticket = m_head.load(std::memory_order_relaxed);
        }
    }

    task->text_ptr = internal_ssml;
    task->type = TaskType::SpeakSsml;
    task->interrupt = interrupt;

    task->sequence.store(ticket + 1, std::memory_order_release);
    
    m_ring_bell.store(true, std::memory_order_release);
    m_ring_bell.notify_one();
    return true;
}
bool SpeechDispatcher::StopSpeech() noexcept {
    if (!is_active.load(std::memory_order_relaxed)) return false;

#if defined(__linux__) && !defined(__ANDROID__)
    if (speech != nullptr) {
        std::lock_guard<std::mutex> lock(speechd_mutex);
        spd_stop(speech);
        spd_cancel(speech);
    }
#endif
    m_isSpeakingLocal.store(false, std::memory_order_release);

    size_t ticket = m_head.load(std::memory_order_relaxed);
    AsyncSpdTask* task = nullptr;

    while (true) {
        task = &m_ring_queue[ticket & RING_MASK];
        size_t seq = task->sequence.load(std::memory_order_acquire);
        intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

        if (difference == 0) {
            if (m_head.compare_exchange_strong(ticket, ticket + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                break;
            }
        } else if (difference < 0) {
            std::this_thread::yield(); 
            ticket = m_head.load(std::memory_order_relaxed);
        } else {
            ticket = m_head.load(std::memory_order_relaxed);
        }
    }

    task->text_ptr = nullptr; 
    task->type = TaskType::Stop;
    task->interrupt = true;

    task->sequence.store(ticket + 1, std::memory_order_release);
    
    m_ring_bell.store(true, std::memory_order_release);
    m_ring_bell.notify_one();
    return true;
}

bool SpeechDispatcher::SetParameter(int param, const void* value) {
    if (!value || !is_active.load(std::memory_order_relaxed)) return false;

    size_t ticket = m_head.load(std::memory_order_relaxed);
    AsyncSpdTask* task = nullptr;

    while (true) {
        task = &m_ring_queue[ticket & RING_MASK];
        size_t seq = task->sequence.load(std::memory_order_acquire);
        intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

        if (difference == 0) {
            if (m_head.compare_exchange_strong(ticket, ticket + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                break;
            }
        } else if (difference < 0) {
            std::this_thread::yield();
            ticket = m_head.load(std::memory_order_relaxed);
        } else {
            ticket = m_head.load(std::memory_order_relaxed);
#if defined(__aarch64__) || defined(__arm__)
            asm volatile("yield" ::: "memory");
#endif
        }
    }

    task->text_ptr = nullptr;
    task->type = TaskType::SetParam;
    task->param_id = param;
    
    if (param == SRAL_PARAM_ENABLE_SPELLING) {
        task->param_val = static_cast<int>(*static_cast<const bool*>(value));
    } else {
        task->param_val = *static_cast<const int*>(value);
    }

    task->sequence.store(ticket + 1, std::memory_order_release);
    
    m_ring_bell.store(true, std::memory_order_release);
    m_ring_bell.notify_one();
    return true;
}

void SpeechDispatcher::BackgroundWorkerLoop(std::stop_token stop_token) noexcept {
    std::string text_scratchpad;
    text_scratchpad.reserve(512);

    while (!stop_token.stop_requested()) {
        size_t current_tail = m_tail.load(std::memory_order_acquire);
        AsyncSpdTask& task = m_ring_queue[current_tail & RING_MASK];
        
        size_t seq = task.sequence.load(std::memory_order_acquire);
        intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1);

        if (difference != 0) {
            if (m_ring_bell.load(std::memory_order_relaxed)) {
                m_ring_bell.store(false, std::memory_order_release);
            }
            
            seq = task.sequence.load(std::memory_order_acquire);
            if (static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1) != 0) {
                while (!m_ring_bell.load(std::memory_order_acquire) && !stop_token.stop_requested()) {
                    m_ring_bell.wait(false, std::memory_order_acquire);
                }
            }
            
            if (stop_token.stop_requested()) [[unlikely]] return;
            continue;
        }

        const char* localText_ptr = task.text_ptr;
        TaskType localType        = task.type;
        bool localInterrupt       = task.interrupt;
        int localParamId          = task.param_id;
        int localParamVal         = task.param_val;

#if defined(__linux__) && !defined(__ANDROID__)
        if (speech != nullptr) {
            if (localInterrupt && (localType == TaskType::Speak || localType == TaskType::SpeakSsml)) {
                std::lock_guard<std::mutex> lock(speechd_mutex);
                spd_stop(speech);
                spd_cancel(speech);
                m_isSpeakingLocal.store(false, std::memory_order_relaxed);
            }

            if (localType == TaskType::Speak && localText_ptr) {
                if (localText_ptr && localText_ptr[0] != '\0') {
                    if (!enableSpelling) {
                        text_scratchpad.assign(localText_ptr);
                        XmlEncode(text_scratchpad);
                        std::string final_ssml = "<speak>" + text_scratchpad + "</speak>";
                        
                        m_isSpeakingLocal.store(true, std::memory_order_release);
                        std::lock_guard<std::mutex> lock(speechd_mutex);
                        int spd_msg_id = spd_say(speech, SPD_IMPORTANT, final_ssml.c_str());
                        if (spd_msg_id > 0) {
                            m_activeMsgId.store(static_cast<size_t>(spd_msg_id), std::memory_order_release);
                        }
                    } else {
                        utf8_iter iter;
                        utf8_init(&iter, localText_ptr);
                        
                        m_isSpeakingLocal.store(true, std::memory_order_release);
                        while (utf8_next(&iter)) {
                            if (iter.size == 0 || iter.size > 4) continue;

                            char single_char_buf[5] = {0}; 
                            const char* raw_char_ptr = utf8_getchar(&iter);
                            std::memcpy(single_char_buf, raw_char_ptr, iter.size);
                            single_char_buf[iter.size] = '\0';

                            if (!m_isSpeakingLocal.load(std::memory_order_relaxed) || stop_token.stop_requested()) [[unlikely]] {
                                break;
                            }

                            {
                                std::lock_guard<std::mutex> lock(speechd_mutex);
                                int spd_msg_id = spd_char(speech, SPD_IMPORTANT, static_cast<const char*>(single_char_buf));
                                if (spd_msg_id > 0) {
                                    m_activeMsgId.store(static_cast<size_t>(spd_msg_id), std::memory_order_release);
                                }
                            }

                            std::this_thread::sleep_for(std::chrono::milliseconds(2));
                        }
                    }
                }
            } 
            else if (localType == TaskType::SpeakSsml && localText_ptr) {
                if (localText_ptr && localText_ptr[0] != '\0') {
                    m_isSpeakingLocal.store(true, std::memory_order_release);
                    std::lock_guard<std::mutex> lock(speechd_mutex);
                    int spd_msg_id = spd_say(speech, SPD_IMPORTANT, localText_ptr);
                    if (spd_msg_id > 0) {
                        m_activeMsgId.store(static_cast<size_t>(spd_msg_id), std::memory_order_release);
                    }
                }
            } 
            else if (localType == TaskType::Stop) {
                std::lock_guard<std::mutex> lock(speechd_mutex);
                spd_stop(speech);
                m_isSpeakingLocal.store(false, std::memory_order_release);
            }
            else if (localType == TaskType::SetParam) {
                std::lock_guard<std::mutex> lock(speechd_mutex);
                switch (localParamId) {
                case SRAL_PARAM_SYMBOL_LEVEL:
                    spd_set_punctuation(speech, static_cast<SPDPunctuation>(localParamVal));
                    break;
                case SRAL_PARAM_SPEECH_RATE:
                    spd_set_voice_rate(speech, localParamVal);
                    m_speechRate = localParamVal;
                    break;
                case SRAL_PARAM_SPEECH_VOLUME:
                    spd_set_volume(speech, localParamVal);
                    m_speechVolume = localParamVal;
                    break;
                case SRAL_PARAM_ENABLE_SPELLING:
                    this->enableSpelling = static_cast<bool>(localParamVal);
                    break;
                case SRAL_PARAM_VOICE_INDEX: {
                    if (m_voiceList && localParamVal >= 0 && localParamVal < m_voiceCount) {
                        if (spd_set_synthesis_voice(speech, m_voiceList[localParamVal]->name) == 0) {
                            m_voiceIndex = localParamVal;
                        }
                    }
                    break;
                }
                }
            }
        }
#else
        (void)localText_ptr; (void)localType; (void)localInterrupt; (void)localParamId; (void)localParamVal;
#endif

        task.text_ptr = nullptr;
        task.sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
        m_tail.store(current_tail + 1, std::memory_order_release);
    }
}
bool SpeechDispatcher::GetActive() noexcept {
#if defined(__linux__) && !defined(__ANDROID__)
    std::lock_guard<std::mutex> lock(speechd_mutex);
    return speech != nullptr;
#else
    return false;
#endif
}

bool SpeechDispatcher::IsSpeaking() noexcept {
    return m_isSpeakingLocal.load(std::memory_order_acquire);
}

bool SpeechDispatcher::GetParameter(int param, void* value) {
    if (!value) return false;

#if defined(__linux__) && !defined(__ANDROID__)
    if (speech == nullptr) return false;
        
    std::lock_guard<std::mutex> lock(speechd_mutex);
    switch (param) {
    case SRAL_PARAM_SPEECH_RATE:
        *static_cast<int*>(value) = this->m_speechRate;
        return true;
    case SRAL_PARAM_SPEECH_VOLUME:
        *static_cast<int*>(value) = this->m_speechVolume;
        return true;
    case SRAL_PARAM_ENABLE_SPELLING:
        *static_cast<bool*>(value) = this->enableSpelling;
        return true;
    case SRAL_PARAM_VOICE_PROPERTIES: {
        auto* voiceProperties = static_cast<SRAL_VoiceInfo*>(value);
        RefreshVoiceList();
        
        if (!voiceProperties || !m_voiceList) return false;
        
        if (m_voiceIndex >= m_voiceCount || m_voiceIndex < 0) {
            m_voiceIndex = (m_voiceCount > 0) ? 0 : -1;
        }
        
        m_voice_strings.clear();
        m_voice_strings.reserve(m_voiceCount * 4);
        
        auto AddVoiceString = [this](const char* str) -> const char* {
            if (!str) return nullptr;
            size_t len = std::strlen(str) + 1;
            auto buffer = std::make_unique<char[]>(len);
            std::memcpy(buffer.get(), str, len);
            
            m_voice_strings.push_back(std::move(buffer));
            return m_voice_strings.back().get();
        };

        for (int index = 0; index < m_voiceCount; ++index) {
            voiceProperties[index].index = index;
            voiceProperties[index].name = AddVoiceString(m_voiceList[index]->name);
            voiceProperties[index].language = AddVoiceString(m_voiceList[index]->language);
            voiceProperties[index].gender = AddVoiceString(m_voiceList[index]->variant);
            voiceProperties[index].vendor = AddVoiceString("Unknown");
        }
        return true;
    }
    case SRAL_PARAM_VOICE_COUNT:
        RefreshVoiceList();
        
        if (m_voiceIndex >= m_voiceCount || m_voiceIndex < 0) {
            m_voiceIndex = (m_voiceCount > 0) ? 0 : -1;
        }
        
        *static_cast<int*>(value) = m_voiceCount;
        return true;
    case SRAL_PARAM_VOICE_INDEX:
        *static_cast<int*>(value) = m_voiceIndex;
        return true;
    default:
        return false;
    }
#else
    (void)param; (void)value;
    return false;
#endif
}

bool SpeechDispatcher::Braille(const char* text) {
#if defined(__linux__) && !defined(__ANDROID__)
    if (!brailleInitialized || !text) return false;
    std::lock_guard<std::mutex> lock(speechd_mutex);
    return brlapi_writeText(BRLAPI_CURSOR_LEAVE, text) >= 0;
#else
    (void)text; return false;
#endif
}

bool SpeechDispatcher::PauseSpeech() noexcept {
#if defined(__linux__) && !defined(__ANDROID__)
    std::lock_guard<std::mutex> lock(speechd_mutex);
    if (speech == nullptr) return false;
    
    if (spd_pause(speech) == 0) {
        paused = true; 
        return true;
    }
    return false;
#else
    return false;
#endif
}

bool SpeechDispatcher::ResumeSpeech() noexcept {
#if defined(__linux__) && !defined(__ANDROID__)
    std::lock_guard<std::mutex> lock(speechd_mutex);
    if (speech == nullptr) return false;
    
    if (spd_resume(speech) == 0) {
        paused = false; 
        return true;
    }
    return false;
#else
    return false;
#endif
}

void SpeechDispatcher::SpeechNotificationCallback(size_t msg_id, size_t client_id, int type) noexcept {
    (void)client_id;
#if defined(__linux__) && !defined(__ANDROID__)
    if (type == SPD_EVENT_END || type == SPD_EVENT_CANCEL) {
        if (msg_id == m_activeMsgId.load(std::memory_order_acquire)) {
            m_isSpeakingLocal.store(false, std::memory_order_release);
        }
    }
#else
    (void)msg_id; (void)type;
#endif
}

void SpeechDispatcher::RefreshVoiceList() {
#if defined(__linux__) && !defined(__ANDROID__)
    if (!speech) return;
    ClearVoiceList();
    
    m_voiceList = spd_list_synthesis_voices(speech);
    if (m_voiceList) {
        int count = 0;
        while (m_voiceList[count] != nullptr) {
            count++;
        }
        m_voiceCount = count;
    }
#endif
}

void SpeechDispatcher::ClearVoiceList() noexcept {
#if defined(__linux__) && !defined(__ANDROID__)
    if (m_voiceList) {
        free_spd_modules(reinterpret_cast<char**>(m_voiceList));
        m_voiceList = nullptr;
        m_voiceCount = 0;
    }
#endif
}

const char* SpeechDispatcher::AddString(const char* text) {
    if (!text) return nullptr;
    
    std::lock_guard<std::mutex> lock(m_string_pool_mutex);
    for (const auto& str : m_string_pool) {
        if (std::strcmp(str.get(), text) == 0) {
            return str.get();
        }
    }
    
    size_t len = std::strlen(text) + 1;
    auto buffer = std::make_unique<char[]>(len);
    std::memcpy(buffer.get(), text, len);
    
    m_string_pool.push_back(std::move(buffer));
    return m_string_pool.back().get();
}

void SpeechDispatcher::ClearStringPool() noexcept {
    std::lock_guard<std::mutex> lock(m_string_pool_mutex);
    m_string_pool.clear();
}

} // namespace Sral
