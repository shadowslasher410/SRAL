#if defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>

#if TARGET_OS_OSX || TARGET_OS_IPHONE

#import "AVSpeech.h"
#import "../Include/SRAL.h"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#include <stdint.h>
#include <cmath>
#include <new>
#include <algorithm>
#include <string_view>
#include <cstring>

#if defined(__GNUC__) || defined(__clang__)
    #define BS_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define BS_LIKELY(x)   __builtin_expect(!!(x), 1)
#else
    #define BS_UNLIKELY(x) (x)
    #define BS_LIKELY(x)   (x)
#endif

@interface SralSpeechInstance : NSObject
@property (nonatomic, strong) AVSpeechSynthesizer* synth;
@property (nonatomic, strong) AVSpeechSynthesisVoice* currentVoice;
@end

@implementation SralSpeechInstance
@end
class AVSpeechSynthesizerWrapper final {
public:
    float rate;
    float volume;
    SralSpeechInstance* instance;

    AVSpeechSynthesizerWrapper() noexcept
        : rate(AVSpeechUtteranceDefaultSpeechRate)
        , volume(1.0f)
        , instance(nil) {}

    ~AVSpeechSynthesizerWrapper() noexcept {
        (void)Uninitialize();
    }

    bool Initialize() noexcept {
        @autoreleasepool {
            instance = [[SralSpeechInstance alloc] init];
            if (!instance) return false;
            
            instance.currentVoice = [AVSpeechSynthesisVoice voiceWithLanguage:@"en-US"];
            instance.synth = [[AVSpeechSynthesizer alloc] init];
            return (instance.synth != nil);
        }
    }

    bool Uninitialize() noexcept {
        @autoreleasepool {
            if (instance && instance.synth) {
                if (instance.synth.isPaused) {
                    [instance.synth continueSpeaking];
                }
                if (instance.synth.isSpeaking) {
                    [instance.synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
                }
            }
            instance = nil;
            return true;
        }
    }

    bool Speak(const char* const text, const bool interrupt) noexcept {
        if (!instance || instance.synth == nil || !text) {
            return false;
        }
        
        @autoreleasepool {
            if (interrupt) {
                if (instance.synth.isPaused) {
                    [instance.synth continueSpeaking];
                }
                if (instance.synth.isSpeaking) {
                    [instance.synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
                }
            }
            
            NSString* const nstext = [NSString stringWithUTF8String:text];
            if (nstext == nil || nstext.length == 0) {
                return false;
            }
            
            AVSpeechUtterance* const utterance = [[AVSpeechUtterance alloc] initWithString:nstext];
            if (!utterance) return false;
            
            utterance.rate = rate;
            utterance.volume = volume;
            utterance.voice = instance.currentVoice;
            
            [instance.synth speakUtterance:utterance];
            return true;
        }
    }

    bool StopSpeech() noexcept {
        if (!instance || instance.synth == nil) return false;
        
        if (instance.synth.isPaused) {
            [instance.synth continueSpeaking];
        }
        if (instance.synth.isSpeaking) {
            return [instance.synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate] == YES;
        }
        return false;
    }

    bool PauseSpeech() noexcept {
        if (!instance || instance.synth == nil) return false;
        if (instance.synth.isSpeaking && !instance.synth.isPaused) {
            return [instance.synth pauseSpeakingAtBoundary:AVSpeechBoundaryImmediate] == YES;
        }
        return false;
    }

    bool ResumeSpeech() noexcept {
        if (!instance || instance.synth == nil) return false;
        if (instance.synth.isPaused) {
            return [instance.synth continueSpeaking] == YES;
        }
        return false;
    }

    [[nodiscard]] bool IsSpeaking() const noexcept { 
        return (instance && instance.synth) ? instance.synth.isSpeaking : false; 
    }
    
    [[nodiscard]] bool IsPaused() const noexcept { 
        return (instance && instance.synth) ? instance.synth.isPaused : false; 
    }

    [[nodiscard]] bool GetActive() const noexcept { 
        return instance && instance.synth != nil; 
    }

    [[nodiscard]] uint64_t GetVoiceCount() const noexcept {
        return [AVSpeechSynthesisVoice speechVoices].count;
    }

    [[nodiscard]] NSString* GetVoiceNameObject(const uint64_t index) const noexcept {
        NSArray<AVSpeechSynthesisVoice*>* const voices = [AVSpeechSynthesisVoice speechVoices];
        if (index >= voices.count) {
            return nil;
        }
        return [voices objectAtIndex:index].name;
    }

    [[nodiscard]] NSString* GetVoiceLanguageObject(const uint64_t index) const noexcept {
        NSArray<AVSpeechSynthesisVoice*>* const voices = [AVSpeechSynthesisVoice speechVoices];
        if (index >= voices.count) {
            return nil;
        }
        return [voices objectAtIndex:index].language;
    }

    bool SetVoice(const uint64_t index) noexcept {
        NSArray<AVSpeechSynthesisVoice*>* const voices = [AVSpeechSynthesisVoice speechVoices];
        if (index >= voices.count) {
            return false;
        }
        if (instance) {
            instance.currentVoice = [voices objectAtIndex:index];
        }
        return true;
    }
};
namespace Sral {

    AvSpeech::AvSpeech() noexcept : m_ring_queue{} {
        for (size_t i = 0; i < RING_BUFFER_SIZE; ++i) {
            m_ring_queue[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    AvSpeech::~AvSpeech() noexcept {
        static_cast<void>(Uninitialize());
    }

    bool AvSpeech::Initialize() {
        std::lock_guard lock(m_init_mutex);
        if (m_initialized.load(std::memory_order_relaxed)) return true;

        obj = new (std::nothrow) AVSpeechSynthesizerWrapper();
        if (!obj || !obj->Initialize()) {
            if (obj) { delete obj; obj = nullptr; }
            return false;
        }

        m_head.store(0, std::memory_order_relaxed);
        m_tail.store(0, std::memory_order_relaxed);
        m_initialized.store(true, std::memory_order_release);

        m_worker_thread = std::jthread([this](std::stop_token st) {
            BackgroundWorkerLoop(st);
        });

        return true;
    }

    bool AvSpeech::Uninitialize() {
        std::jthread thread_to_join;
        {
            std::lock_guard lock(m_init_mutex);
            if (!m_initialized.load(std::memory_order_relaxed)) return true;

            m_worker_thread.request_stop();
            
            size_t head_snap = m_head.load(std::memory_order_relaxed);
            m_tail.store(head_snap, std::memory_order_release);
            m_initialized.store(false, std::memory_order_release);

            m_ring_bell.store(true, std::memory_order_release);
            m_ring_bell.notify_one();

            thread_to_join = std::move(m_worker_thread);
        }

        if (thread_to_join.joinable()) {
            thread_to_join.join();
        }

        std::lock_guard lock(m_init_mutex);
        ReleaseAllStrings();
        if (obj == nullptr) {
            return false;
        }
        obj->Uninitialize();
        delete obj;
        obj = nullptr;
        return true; 
    }
    
    bool AvSpeech::PushTask(TaskType type, std::string_view text, float param_val, bool interrupt) noexcept {
        if (!m_initialized.load(std::memory_order_relaxed) || m_worker_thread.get_stop_token().stop_requested()) {
            return false;
        }

        size_t ticket = m_head.load(std::memory_order_relaxed);
        AsyncSpeechTask* task = nullptr;

        while (true) {
            task = &m_ring_queue[ticket & RING_MASK];
            size_t seq = task->sequence.load(std::memory_order_acquire);
            intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

            if (difference == 0) {
                if (m_head.compare_exchange_weak(ticket, ticket + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (difference < 0) {
                return false;
            } else {
                ticket = m_head.load(std::memory_order_relaxed);
            }
        }

        if (!text.empty()) {
            size_t max_copy = (std::min)(text.size(), task->text.size() - 1);
            std::memcpy(task->text.data(), text.data(), max_copy);
            task->text[max_copy] = '\0';
        } else {
            task->text[0] = '\0';
        }

        task->type = type;
        task->parameter_value = param_val;
        task->interrupt = interrupt;

        task->sequence.store(ticket + 1, std::memory_order_release);

        if (!m_ring_bell.exchange(true, std::memory_order_release)) {
            m_ring_bell.notify_one();
        }
        return true;
    }

    void AvSpeech::BackgroundWorkerLoop(std::stop_token stop_token) noexcept {
        while (!stop_token.stop_requested()) {
            size_t current_tail = m_tail.load(std::memory_order_acquire);
            AsyncSpeechTask& task = m_ring_queue[current_tail & RING_MASK];

            size_t seq = task.sequence.load(std::memory_order_acquire);
            intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1);

            if (difference != 0) {
                m_ring_bell.store(false, std::memory_order_release);
                seq = task.sequence.load(std::memory_order_acquire);
                
                if (static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1) != 0) {
                    while (!m_ring_bell.load(std::memory_order_acquire) && !stop_token.stop_requested()) {
                        m_ring_bell.wait(false, std::memory_order_acquire);
                    }
                } else {
                    m_ring_bell.store(true, std::memory_order_release);
                }
                if (stop_token.stop_requested()) [[unlikely]] break;
                continue;
            }

            const TaskType type = task.type;
            const bool interrupt = task.interrupt;
            const float param_val = task.parameter_value;
            const char* const payload_text = task.text.data();

            @autoreleasepool {
                if (obj) [[likely]] {
                    switch (type) {
                        case TaskType::Speak:
                            obj->Speak(payload_text, interrupt);
                            break;
                        case TaskType::Stop:
                            obj->StopSpeech();
                            break;
                        case TaskType::Pause:
                            obj->PauseSpeech();
                            break;
                        case TaskType::Resume:
                            obj->ResumeSpeech();
                            break;
                        case TaskType::SetVolume:
                            obj->volume = param_val;
                            break;
                        case TaskType::SetRate:
                            obj->rate = param_val;
                            break;
                    }
                }
            }

            m_tail.store(current_tail + 1, std::memory_order_release);
            task.sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
        }
    }

    bool AvSpeech::GetActive() {
        std::lock_guard lock(m_init_mutex);
        return m_initialized.load(std::memory_order_relaxed) && obj != nullptr && obj->GetActive();
    }

    bool AvSpeech::Speak(const char* const text, const bool interrupt) {
        if (!m_initialized.load(std::memory_order_relaxed)) {
            if (!GetActive()) return false;
            if (!Initialize()) return false;
        }

        if (interrupt) {
            size_t head_snap = m_head.load(std::memory_order_relaxed);
            m_tail.store(head_snap, std::memory_order_release);
        }

        return PushTask(TaskType::Speak, text ? text : "", 0.0f, interrupt);
    }

    bool AvSpeech::StopSpeech() {
        return PushTask(TaskType::Stop, "", 0.0f, true);
    }

    bool AvSpeech::PauseSpeech() {
        return PushTask(TaskType::Pause, "", 0.0f, false);
    }

    bool AvSpeech::ResumeSpeech() {
        return PushTask(TaskType::Resume, "", 0.0f, false);
    }

    bool AvSpeech::IsSpeaking() {
        std::lock_guard lock(m_init_mutex);
        return obj ? obj->IsSpeaking() : false;
    }

    bool AvSpeech::SetParameter(const int param, const void* const value) {
        if (!value) [[unlikely]] return false;

        switch (param) {
            case SRAL_PARAM_SPEECH_VOLUME: {
                const int val_int = *static_cast<const int*>(value);
                m_cached_volume.store(static_cast<uint64_t>(val_int), std::memory_order_relaxed);
                
                float v = static_cast<float>(val_int) / 100.0f;
                if (v < 0.0f) v = 0.0f;
                if (v > 1.0f) v = 1.0f;
                return PushTask(TaskType::SetVolume, "", v, false);
            }
            case SRAL_PARAM_SPEECH_RATE: {
                const int val_int = *static_cast<const int*>(value);
                m_cached_rate.store(static_cast<uint64_t>(val_int), std::memory_order_relaxed);
                
                float r = static_cast<float>(val_int) / 100.0f;
                if (r < 0.0f) r = 0.0f;
                if (r > 1.0f) r = 1.0f;
                return PushTask(TaskType::SetRate, "", r, false);
            }
            case SRAL_PARAM_VOICE_INDEX: {
                std::lock_guard lock(m_init_mutex);
                if (!obj) return false;
                return obj->SetVoice(static_cast<uint64_t>(*reinterpret_cast<const int*>(value)));
            }
            default:
                return false;
        }
    }

    bool AvSpeech::GetParameter(const int param, void* const value) {
        if (!value) [[unlikely]] return false;

        switch (param) {
            case SRAL_PARAM_SPEECH_VOLUME:
                *static_cast<int*>(value) = static_cast<int>(m_cached_volume.load(std::memory_order_relaxed));
                return true;
            case SRAL_PARAM_SPEECH_RATE:
                *static_cast<int*>(value) = static_cast<int>(m_cached_rate.load(std::memory_order_relaxed));
                return true;
            case SRAL_PARAM_VOICE_COUNT: {
                std::lock_guard lock(m_init_mutex);
                if (!obj) return false;
                *reinterpret_cast<int*>(value) = static_cast<int>(obj->GetVoiceCount());
                return true;
            }
            case SRAL_PARAM_VOICE_PROPERTIES: {
                std::lock_guard lock(m_init_mutex);
                if (!obj || !value) return false;
                
                ReleaseAllStrings();
                const uint64_t voice_count = obj->GetVoiceCount();
                SRAL_VoiceInfo* const voices = reinterpret_cast<SRAL_VoiceInfo*>(value);

                @autoreleasepool {
                    for (uint64_t i = 0; i < voice_count; ++i) {
                        NSString* const nsName = obj->GetVoiceNameObject(i);
                        NSString* const nsLang = obj->GetVoiceLanguageObject(i);
                        
                        const char* const utf8Name = nsName ? [nsName UTF8String] : "";
                        const char* const utf8Lang = nsLang ? [nsLang UTF8String] : "en-US";
                        
                        voices[i].index = static_cast<int>(i);
                        voices[i].name = AddString(utf8Name);
                        voices[i].language = AddString(utf8Lang);
                        voices[i].gender = AddString("unknown");
                        voices[i].vendor = AddString("Apple");
                    }
                }
                return true;
            }
            default:
                return false;
        }
    }

    int AvSpeech::GetFeatures() {
        return SRAL_SUPPORTS_SPEECH | SRAL_SUPPORTS_SPEECH_RATE | SRAL_SUPPORTS_SPEECH_VOLUME;
    }

    bool AvSpeech::SpeakSsml(const char* const ssml, const bool interrupt) {
        return Speak(ssml, interrupt);
    }

    bool AvSpeech::Braille(const char* const) { 
        return false; 
    }

    void* AvSpeech::SpeakToMemory(const char* const, uint64_t* const, int* const, int* const, int* const) {
        return nullptr; 
    }

} // namespace Sral

#endif /* TARGET_OS_OSX || TARGET_OS_IPHONE */
#endif /* defined(__APPLE__) || defined(__MACH__) */
