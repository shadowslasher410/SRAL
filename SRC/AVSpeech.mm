#if defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>

#if TARGET_OS_OSX || TARGET_OS_IPHONE

#import "AV_Speech.h"
#import "SRAL.h"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
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

@interface SralSpeechInstance : NSObject {
@public
    AVSpeechSynthesizer* _synth;
    AVSpeechSynthesisVoice* _currentVoice;
    NSArray<AVSpeechSynthesisVoice*>* _cachedVoices;
}
@property (nonatomic, strong) AVSpeechSynthesizer* synth;
@property (nonatomic, strong) AVSpeechSynthesisVoice* currentVoice;
@end

@implementation SralSpeechInstance
@synthesize synth = _synth;
@synthesize currentVoice = _currentVoice;
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

    AVSpeechSynthesizerWrapper(const AVSpeechSynthesizerWrapper&) = delete;
    AVSpeechSynthesizerWrapper& operator=(const AVSpeechSynthesizerWrapper&) = delete;
    AVSpeechSynthesizerWrapper(AVSpeechSynthesizerWrapper&&) noexcept = delete;
    AVSpeechSynthesizerWrapper& operator=(AVSpeechSynthesizerWrapper&&) noexcept = delete;

    bool Initialize() noexcept {
        @autoreleasepool {
            instance = [[SralSpeechInstance alloc] init];
            if (BS_UNLIKELY(!instance)) return false;
            
            instance->_cachedVoices = [AVSpeechSynthesisVoice speechVoices];
            instance->_currentVoice = [AVSpeechSynthesisVoice voiceWithLanguage:@"en-US"];
            instance->_synth = [[AVSpeechSynthesizer alloc] init];
            return (instance->_synth != nil);
        }
    }

    bool Uninitialize() noexcept {
        @autoreleasepool {
            if (BS_LIKELY(instance != nil)) {
                AVSpeechSynthesizer* const currentSynth = instance->_synth;
                if (BS_LIKELY(currentSynth != nil)) {
                    if (currentSynth.isPaused) {
                        [currentSynth continueSpeaking];
                    }
                    if (currentSynth.isSpeaking) {
                        [currentSynth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
                    }
                }
                instance->_cachedVoices = nil;
                instance->_currentVoice = nil;
                instance->_synth = nil;
                instance = nil;
            }
            return true;
        }
    }
    bool Speak(const char* const text, const bool interrupt) noexcept {
        if (BS_UNLIKELY(!instance || instance->_synth == nil || !text || text[0] == '\0')) {
            return false;
        }
        
        AVSpeechSynthesizer* const currentSynth = instance->_synth;
        if (interrupt) {
            if (currentSynth.isPaused) {
                [currentSynth continueSpeaking];
            }
            if (currentSynth.isSpeaking) {
                [currentSynth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
            }
        }
        
        NSString* const nstext = [NSString stringWithUTF8String:text];
        if (BS_UNLIKELY(nstext == nil)) {
            return false;
        }
        
        AVSpeechUtterance* const utterance = [[AVSpeechUtterance alloc] initWithString:nstext];
        if (BS_UNLIKELY(!utterance)) return false;
        
        utterance.rate = rate;
        utterance.volume = volume;
        utterance.voice = instance->_currentVoice;
        
        [currentSynth speakUtterance:utterance];
        return true;
    }

    bool StopSpeech() noexcept {
        if (BS_UNLIKELY(!instance || instance->_synth == nil)) return false;
        AVSpeechSynthesizer* const currentSynth = instance->_synth;
        
        if (currentSynth.isPaused) {
            [currentSynth continueSpeaking];
        }
        if (currentSynth.isSpeaking) {
            return [currentSynth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate] == YES;
        }
        return false;
    }

    bool PauseSpeech() noexcept {
        if (BS_UNLIKELY(!instance || instance->_synth == nil)) return false;
        AVSpeechSynthesizer* const currentSynth = instance->_synth;
        
        if (currentSynth.isSpeaking && !currentSynth.isPaused) {
            return [currentSynth pauseSpeakingAtBoundary:AVSpeechBoundaryImmediate] == YES;
        }
        return false;
    }

    bool ResumeSpeech() noexcept {
        if (BS_UNLIKELY(!instance || instance->_synth == nil)) return false;
        AVSpeechSynthesizer* const currentSynth = instance->_synth;
        
        if (currentSynth.isPaused) {
            return [currentSynth continueSpeaking] == YES;
        }
        return false;
    }

    [[nodiscard]] bool IsSpeaking() const noexcept { 
        return (instance && instance->_synth) ? instance->_synth.isSpeaking : false; 
    }
    
    [[nodiscard]] bool IsPaused() const noexcept { 
        return (instance && instance->_synth) ? instance->_synth.isPaused : false; 
    }

    [[nodiscard]] bool GetActive() const noexcept { 
        return instance && instance->_synth != nil; 
    }

    [[nodiscard]] uint64_t GetVoiceCount() const noexcept {
        return (instance && instance->_cachedVoices) ? static_cast<uint64_t>(instance->_cachedVoices.count) : 0;
    }

    [[nodiscard]] NSString* GetVoiceNameObject(const uint64_t index) const noexcept {
        if (BS_UNLIKELY(!instance || !instance->_cachedVoices)) return nil;
        NSArray<AVSpeechSynthesisVoice*>* const voices = instance->_cachedVoices;
        if (BS_UNLIKELY(index >= voices.count)) {
            return nil;
        }
        return [voices objectAtIndex:index].name;
    }

    [[nodiscard]] NSString* GetVoiceLanguageObject(const uint64_t index) const noexcept {
        if (BS_UNLIKELY(!instance || !instance->_cachedVoices)) return nil;
        NSArray<AVSpeechSynthesisVoice*>* const voices = instance->_cachedVoices;
        if (BS_UNLIKELY(index >= voices.count)) {
            return nil;
        }
        return [voices objectAtIndex:index].language;
    }

    bool SetVoice(const uint64_t index) noexcept {
        if (BS_UNLIKELY(!instance || !instance->_cachedVoices)) return false;
        NSArray<AVSpeechSynthesisVoice*>* const voices = instance->_cachedVoices;
        if (BS_UNLIKELY(index >= voices.count)) {
            return false;
        }
        instance->_currentVoice = [voices objectAtIndex:index];
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
        if (BS_UNLIKELY(!obj || !obj->Initialize())) {
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
        if (BS_UNLIKELY(obj == nullptr)) {
            return false;
        }
        obj->Uninitialize();
        delete obj;
        obj = nullptr;
        return true; 
    }

    bool AvSpeech::PushTask(TaskType type, std::string_view text, float param_val, bool interrupt) noexcept {
        if (BS_UNLIKELY(!m_initialized.load(std::memory_order_relaxed))) {
            return false;
        }

        size_t ticket = m_head.load(std::memory_order_acquire);
        AsyncSpeechTask* task = nullptr;

        while (true) {
            task = &m_ring_queue[ticket & RING_MASK];
            size_t seq = task->sequence.load(std::memory_order_acquire);
            intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

            if (difference == 0) {
                if (m_head.compare_exchange_weak(ticket, ticket + 1, std::memory_order_release, std::memory_order_acquire)) {
                    break;
                }
            } else if (difference < 0) {
                return false;
            } else {
                ticket = m_head.load(std::memory_order_acquire);
            }
        }

        if (!text.empty()) {
            const size_t max_copy = (std::min)(text.size(), task->text.size() - 1);
            std::memcpy(task->text.data(), text.data(), max_copy);
            task->text[max_copy] = '\0';
        } else {
            task->text[0] = '\0';
        }

        task->type = type;
        task->parameter_value = param_val;
        task->interrupt = interrupt;

        task->sequence.store(ticket + 1, std::memory_order_release);

        m_ring_bell.store(true, std::memory_order_release);
        m_ring_bell.notify_one();
        return true;
    }

    void AvSpeech::BackgroundWorkerLoop(std::stop_token stop_token) noexcept {
        while (BS_LIKELY(!stop_token.stop_requested())) {
            size_t current_tail = m_tail.load(std::memory_order_acquire);
            AsyncSpeechTask& task = m_ring_queue[current_tail & RING_MASK];

            size_t seq = task.sequence.load(std::memory_order_acquire);
            intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1);

            if (difference != 0) {
                m_ring_bell.store(false, std::memory_order_release);
                seq = task.sequence.load(std::memory_order_acquire);
                
                if (static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1) != 0) {
                    while (!m_ring_bell.load(std::memory_order_acquire)) {
                        if (BS_UNLIKELY(stop_token.stop_requested())) break;
                        m_ring_bell.wait(false, std::memory_order_acquire);
                    }
                } else {
                    m_ring_bell.store(true, std::memory_order_release);
                }
                if (BS_UNLIKELY(stop_token.stop_requested())) break;
                continue;
            }

            const TaskType type = task.type;
            const bool interrupt = task.interrupt;
            const float param_val = task.parameter_value;
            const char* const payload_text = task.text.data();

            @autoreleasepool {
                if (BS_LIKELY(obj != nullptr)) {
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
                        case TaskType::SetVoice:
                            obj->SetVoice(static_cast<uint64_t>(param_val));
                            break;
                    }
                }
            }

            m_tail.store(current_tail + 1, std::memory_order_release);
            task.sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
        }
    }
    bool AvSpeech::GetActive() {
        if (!m_initialized.load(std::memory_order_acquire)) return false;
        std::lock_guard lock(m_init_mutex);
        return obj != nullptr && obj->GetActive();
    }

    bool AvSpeech::Speak(const char* const text, const bool interrupt) {
        if (BS_UNLIKELY(!m_initialized.load(std::memory_order_acquire))) {
            std::lock_guard lock(m_init_mutex);
            if (!m_initialized.load(std::memory_order_relaxed)) {
                if (!Initialize()) return false;
            }
        }

        if (interrupt) {
            (void)PushTask(TaskType::Stop, "", 0.0f, true);
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
        if (!m_initialized.load(std::memory_order_acquire)) return false;
        std::lock_guard lock(m_init_mutex);
        return obj ? obj->IsSpeaking() : false;
    }

    bool AvSpeech::SetParameter(const int param, const void* const value) {
        if (BS_UNLIKELY(!value)) return false;

        switch (param) {
            case SRAL_PARAM_SPEECH_VOLUME: {
                const int val_int = *static_cast<const int*>(value);
                m_cached_volume.store(static_cast<uint8_t>(val_int), std::memory_order_relaxed);
                
                float v = static_cast<float>(val_int) / 100.0f;
                if (v < 0.0f) v = 0.0f;
                if (v > 1.0f) v = 1.0f;
                return PushTask(TaskType::SetVolume, "", v, false);
            }
            case SRAL_PARAM_SPEECH_RATE: {
                const int val_int = *static_cast<const int*>(value);
                m_cached_rate.store(static_cast<uint8_t>(val_int), std::memory_order_relaxed);
                
                float r = static_cast<float>(val_int) / 100.0f;
                if (r < 0.0f) r = 0.0f;
                if (r > 1.0f) r = 1.0f;
                return PushTask(TaskType::SetRate, "", r, false);
            }
            case SRAL_PARAM_VOICE_INDEX: {
                const float voice_idx = static_cast<float>(*static_cast<const int*>(value));
                return PushTask(TaskType::SetVoice, "", voice_idx, false);
            }
            default:
                return false;
        }
    }

    bool AvSpeech::GetParameter(const int param, void* const value) {
        if (BS_UNLIKELY(!value)) return false;

        switch (param) {
            case SRAL_PARAM_SPEECH_VOLUME:
                *static_cast<int*>(value) = static_cast<int>(m_cached_volume.load(std::memory_order_relaxed));
                return true;
            case SRAL_PARAM_SPEECH_RATE:
                *static_cast<int*>(value) = static_cast<int>(m_cached_rate.load(std::memory_order_relaxed));
                return true;
            case SRAL_PARAM_VOICE_COUNT: {
                if (BS_UNLIKELY(!m_initialized.load(std::memory_order_acquire))) return false;
                std::lock_guard lock(m_init_mutex);
                if (BS_UNLIKELY(!obj)) return false;
                *static_cast<int*>(value) = static_cast<int>(obj->GetVoiceCount());
                return true;
            }
            case SRAL_PARAM_VOICE_PROPERTIES: {
                if (BS_UNLIKELY(!m_initialized.load(std::memory_order_acquire))) return false;
                std::lock_guard lock(m_init_mutex);
                if (BS_UNLIKELY(!obj || !obj->instance)) return false;
                
                NSArray<AVSpeechSynthesisVoice*>* const native_voices = obj->instance->_cachedVoices;
                const uint64_t voice_count = native_voices ? native_voices.count : 0;
                if (voice_count == 0) return false;

                SRAL_VoiceInfo* const voices_array = static_cast<SRAL_VoiceInfo*>(::SRAL_malloc(sizeof(SRAL_VoiceInfo) * voice_count));
                if (BS_UNLIKELY(!voices_array)) return false;

                void* const fallback_name = AddString("");
                void* const fallback_lang = AddString("en-US");
                void* const structural_gender = AddString("unknown");
                void* const structural_vendor = AddString("Apple");

                @autoreleasepool {
                    SEL nameSel = @selector(name);
                    SEL langSel = @selector(language);
                    using StringGetterIMP = NSString* (*)(id, SEL);
                    
                    static StringGetterIMP nameFunc = nullptr;
                    static StringGetterIMP langFunc = nullptr;

                    if (BS_UNLIKELY(!nameFunc && voice_count > 0)) {
                        AVSpeechSynthesisVoice* sample_obj = [native_voices objectAtIndex:0];
                        nameFunc = reinterpret_cast<StringGetterIMP>([sample_obj methodForSelector:nameSel]);
                        langFunc = reinterpret_cast<StringGetterIMP>([sample_obj methodForSelector:langSel]);
                    }

                    for (uint64_t i = 0; i < voice_count; ++i) {
                        AVSpeechSynthesisVoice* const voice_obj = [native_voices objectAtIndex:i];
                        
                        NSString* const nsName = (nameFunc) ? nameFunc(voice_obj, nameSel) : nil;
                        NSString* const nsLang = (langFunc) ? langFunc(voice_obj, langSel) : nil;
                        
                        voices_array[i].index = static_cast<int>(i);
                        
                        if (BS_LIKELY(nsName != nil)) {
                            const char* const name_ptr = [nsName UTF8String];
                            voices_array[i].name = AddString(name_ptr ? name_ptr : "");
                        } else {
                            voices_array[i].name = fallback_name;
                        }
                        
                        if (BS_LIKELY(nsLang != nil)) {
                            const char* const lang_ptr = [nsLang UTF8String];
                            voices_array[i].language = AddString(lang_ptr ? lang_ptr : "en-US");
                        } else {
                            voices_array[i].language = fallback_lang;
                        }
                        
                        voices_array[i].gender = structural_gender;
                        voices_array[i].vendor = structural_vendor;
                    }
                }

                *static_cast<SRAL_VoiceInfo**>(value) = voices_array;
                return true;
            }
            default:
                return Engine::GetParameter(param, value);
        }
    }

    int AvSpeech::GetFeatures() {
        return SRAL_SUPPORTS_SPEECH | SRAL_SUPPORTS_SPEECH_RATE | SRAL_SUPPORTS_SPEECH_VOLUME | SRAL_SUPPORTS_PAUSE_SPEECH;
    }

    bool AvSpeech::SpeakSsml(const char* const ssml, const bool interrupt) {
        return Speak(ssml, interrupt);
    }

    bool AvSpeech::Braille(const char* const text) { 
        (void)text;
        return false; 
    }

    void* AvSpeech::SpeakToMemory(const char* const text, uint64_t* const buffer_size, int* const channels, int* const sample_rate, int* const bits_per_sample) {
        (void)text;
        if (BS_LIKELY(buffer_size != nullptr))   *buffer_size = 0;
        if (BS_LIKELY(channels != nullptr))      *channels = 0;
        if (BS_LIKELY(sample_rate != nullptr))   *sample_rate = 0;
        if (BS_LIKELY(bits_per_sample != nullptr)) *bits_per_sample = 0;
        return nullptr; 
    }

} // namespace Sral

#endif /* TARGET_OS_OSX || TARGET_OS_IPHONE */
#endif /* defined(__APPLE__) || defined(__MACH__) */
