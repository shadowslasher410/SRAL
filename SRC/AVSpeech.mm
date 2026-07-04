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

@interface SralSpeechInstance : NSObject
@property (nonatomic, strong) AVSpeechSynthesizer* synth;
@property (nonatomic, strong) AVSpeechSynthesisVoice* currentVoice;
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

bool AvSpeech::Initialize() {
    obj = new (std::nothrow) AVSpeechSynthesizerWrapper();
    if (!obj) {
        return false;
    }
    return obj->Initialize();
}

bool AvSpeech::Uninitialize() {
    ReleaseAllStrings();
    if (obj == nullptr) {
        return false;
    }
    obj->Uninitialize();
    delete obj;
    obj = nullptr;
    return true; 
}

bool AvSpeech::GetActive() {
    return obj != nullptr && obj->GetActive();
}

bool AvSpeech::Speak(const char* const text, const bool interrupt) {
    if (!obj) return false;
    return obj->Speak(text, interrupt);
}

bool AvSpeech::StopSpeech() {
    if (!obj) return false;
    return obj->StopSpeech();
}

bool AvSpeech::IsSpeaking() {
    return obj ? obj->IsSpeaking() : false;
}

bool AvSpeech::SetParameter(const int param, const void* const value) {
    if (!obj || !value) return false;
    
    switch (param) {
        case SRAL_PARAM_SPEECH_RATE: {
            float r = static_cast<float>(*reinterpret_cast<const int*>(value)) / 100.0f;
            if (r < 0.0f) r = 0.0f;
            if (r > 1.0f) r = 1.0f;
            obj->rate = r;
            return true;
        }
        case SRAL_PARAM_SPEECH_VOLUME: {
            float v = static_cast<float>(*reinterpret_cast<const int*>(value)) / 100.0f;
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            obj->volume = v;
            return true;
        }
        case SRAL_PARAM_VOICE_INDEX:
            return obj->SetVoice(static_cast<uint64_t>(*reinterpret_cast<const int*>(value)));
        default:
            return false;
    }
}

bool AvSpeech::GetParameter(const int param, void* const value) {
    if (!obj || !value) return false;
    
    switch (param) {
        case SRAL_PARAM_SPEECH_RATE: {
            *reinterpret_cast<int*>(value) = static_cast<int>(std::round(obj->rate * 100.0f));
            return true;
        }
        case SRAL_PARAM_SPEECH_VOLUME: {
            *reinterpret_cast<int*>(value) = static_cast<int>(std::round(obj->volume * 100.0f));
            return true;
        }
        case SRAL_PARAM_VOICE_COUNT: {
            *reinterpret_cast<int*>(value) = static_cast<int>(obj->GetVoiceCount());
            return true;
        }
        case SRAL_PARAM_VOICE_PROPERTIES: {
            ReleaseAllStrings();
            const uint64_t voice_count = obj->GetVoiceCount();
            SRAL_VoiceInfo* const voices = reinterpret_cast<SRAL_VoiceInfo*>(value);
            
            if (!voices) return false;

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

bool AvSpeech::SpeakSsml(const char* const ssml, const bool interrupt) {
    return Speak(ssml, interrupt);
}

void* AvSpeech::SpeakToMemory(const char*, uint64_t*, int*, int*, int*) {
    return nullptr;
}

bool AvSpeech::Braille(const char*) {
    return false;
}


bool AvSpeech::PauseSpeech() {
    if (!obj || !obj->instance || !obj->instance.synth) return false;
    
    if (obj->instance.synth.isSpeaking && !obj->instance.synth.isPaused) {
        return [obj->instance.synth pauseSpeakingAtBoundary:AVSpeechBoundaryImmediate] == YES;
    }
    return false;
}

bool AvSpeech::ResumeSpeech() {
    if (!obj || !obj->instance || !obj->instance.synth) return false;
    
    if (obj->instance.synth.isPaused) {
        return [obj->instance.synth continueSpeaking] == YES;
    }
    return false;
}

int AvSpeech::GetFeatures() {
    return SRAL_SUPPORTS_SPEECH | 
           SRAL_SUPPORTS_SPEECH_RATE | 
           SRAL_SUPPORTS_SPEECH_VOLUME | 
           SRAL_SUPPORTS_SELECT_VOICE | 
           SRAL_SUPPORTS_PAUSE_SPEECH;
}

} // namespace Sral

#endif /* TARGET_OS_OSX || TARGET_OS_IPHONE */
#endif /* defined(__APPLE__) || defined(__MACH__) */
