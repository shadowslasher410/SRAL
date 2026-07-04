#if defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>

#if TARGET_OS_OSX

#include "NsSpeech.h"
#import <AppKit/AppKit.h>
#include <mutex>
#include <atomic>

class NSSpeechSynthesizerWrapper;

@interface SRALSpeechDelegate : NSObject <NSSpeechSynthesizerDelegate>
@property (atomic, assign) NSSpeechSynthesizerWrapper* wrapper;
@end

@implementation SRALSpeechDelegate
- (void)speechSynthesizer:(NSSpeechSynthesizer *)sender didFinishSpeaking:(BOOL)finishedSpeaking {
    NSSpeechSynthesizerWrapper* currentWrapper = self.wrapper;
    if (currentWrapper && finishedSpeaking) {
        currentWrapper->OnSpeechFinished();
    }
}
@end

class NSSpeechSynthesizerWrapper final {
private:
    std::mutex mutex_;
    NSSpeechSynthesizer* synth_;
    SRALSpeechDelegate* delegate_;
    float rate_;
    float volume_;

    void InternalCleanup() noexcept {
        if (delegate_) {
            [delegate_ setWrapper:nullptr];
        }
        if (synth_) {
            [synth_ setDelegate:nil];
            [synth_ stopSpeaking];
        }
        synth_ = nil;
        delegate_ = nil;
    }

public:
    NSSpeechSynthesizerWrapper() 
        : synth_(nil), delegate_(nil), rate_(175.0f), volume_(1.0f) {}

    ~NSSpeechSynthesizerWrapper() {
        InternalCleanup();
    }

    bool Initialize() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (synth_) return true;

        NSSpeechSynthesizer* localSynth = [[NSSpeechSynthesizer alloc] init];
        if (!localSynth) return false;

        rate_ = [localSynth rate];
        volume_ = [localSynth volume];
        
        SRALSpeechDelegate* localDelegate = [[SRALSpeechDelegate alloc] init];
        if (!localDelegate) {
            return false;
        }

        [localDelegate setWrapper:this];
        [localSynth setDelegate:localDelegate];

        synth_ = localSynth;
        delegate_ = localDelegate;
        return true;
    }

    void Uninitialize() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        InternalCleanup();
    }

    bool Speak(const char* text, bool interrupt) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!synth_ || !text) return false;

        if (interrupt) {
            [synth_ stopSpeaking];
        }

        NSString* nsStr = [NSString stringWithUTF8String:text];
        if (!nsStr) {
            nsStr = [NSString stringWithCString:text encoding:NSASCIIStringEncoding];
            if (!nsStr) return false;
        }
        return [synth_ startSpeakingString:nsStr] == YES;
    }

    bool Stop() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (synth_) {
            [synth_ stopSpeaking];
        }
        return true;
    }

    bool IsSpeaking() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return synth_ && [synth_ isSpeaking];
    }

    void SetRate(int val) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        rate_ = static_cast<float>(val);
        if (synth_) {
            [synth_ setRate:rate_];
        }
    }

    void SetVolume(int val) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        volume_ = static_cast<float>(val) / 100.0f;
        if (synth_) {
            [synth_ setVolume:volume_];
        }
    }

    void GetRate(int* outValue) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (outValue) {
            *outValue = static_cast<int>(rate_);
        }
    }

    void GetVolume(int* outValue) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (outValue) {
            *outValue = static_cast<int>(volume_ * 100.0f);
        }
    }

    void OnSpeechFinished() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
    }
};

namespace Sral {

static std::atomic<NSSpeechSynthesizerWrapper*> g_sral_speech_obj{nullptr};
static std::mutex g_lifecycle_mutex;

void* NsSpeech::obj = nullptr;

bool NsSpeech::Initialize() {
    std::lock_guard<std::mutex> lock(g_lifecycle_mutex);
    if (g_sral_speech_obj.load() != nullptr) return true;

    NSSpeechSynthesizerWrapper* localObj = new(std::nothrow) NSSpeechSynthesizerWrapper();
    if (!localObj) return false;

    if (!localObj->Initialize()) {
        delete localObj;
        return false;
    }

    g_sral_speech_obj.store(localObj);
    obj = localObj;
    return true;
}

bool NsSpeech::Uninitialize() {
    std::lock_guard<std::mutex> lock(g_lifecycle_mutex);
    NSSpeechSynthesizerWrapper* localObj = g_sral_speech_obj.exchange(nullptr);
    if (localObj) {
        localObj->Uninitialize();
        delete localObj; 
        obj = nullptr;
    }
    return true;
}

bool NsSpeech::Speak(const char* text, bool interrupt) {
    NSSpeechSynthesizerWrapper* localObj = g_sral_speech_obj.load();
    return localObj ? localObj->Speak(text, interrupt) : false;
}

bool NsSpeech::StopSpeech() {
    NSSpeechSynthesizerWrapper* localObj = g_sral_speech_obj.load();
    return localObj ? localObj->Stop() : false;
}

bool NsSpeech::IsSpeaking() {
    NSSpeechSynthesizerWrapper* localObj = g_sral_speech_obj.load();
    return localObj ? localObj->IsSpeaking() : false;
}

bool NsSpeech::GetActive() {
    return g_sral_speech_obj.load() != nullptr;
}

bool NsSpeech::SetParameter(int param, const void* value) {
    NSSpeechSynthesizerWrapper* localObj = g_sral_speech_obj.load();
    if (!localObj || !value) return false;

    int val = *reinterpret_cast<const int*>(value);
    switch (param) {
        case SRAL_PARAM_SPEECH_RATE:
            localObj->SetRate(val);
            break;
        case SRAL_PARAM_SPEECH_VOLUME:
            localObj->SetVolume(val);
            break;
        default:
            return false;
    }
    return true;
}

bool NsSpeech::GetParameter(int param, void* value) {
    NSSpeechSynthesizerWrapper* localObj = g_sral_speech_obj.load();
    if (!localObj || !value) return false;

    switch (param) {
        case SRAL_PARAM_SPEECH_RATE:
            localObj->GetRate(reinterpret_cast<int*>(value));
            break;
        case SRAL_PARAM_SPEECH_VOLUME:
            localObj->GetVolume(reinterpret_cast<int*>(value));
            break;
        default:
            return false;
    }
    return true;
}

} // namespace Sral

#endif /* TARGET_OS_OSX */
#endif /* defined(__APPLE__) || defined(__MACH__) */
