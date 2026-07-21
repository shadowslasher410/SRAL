#if defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>
#if defined(TARGET_OS_OSX) && TARGET_OS_OSX

#import <AppKit/AppKit.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include "NSSpeech.h"

class NSSpeechSynthesizerWrapper final {
public:
    static constexpr size_t TEXT_BUFFER_SIZE = 512;

private:
    std::mutex wrapperMutex_;
    NSSpeechSynthesizer* synth_;
    id delegate_;
    float rate_;
    float volume_;
    std::atomic<bool> isSpeaking_{false};

public:
    NSSpeechSynthesizerWrapper() : synth_(nil), delegate_(nil), rate_(175.0f), volume_(1.0f) {}
    ~NSSpeechSynthesizerWrapper() { InternalCleanup(); }

    void InternalCleanup() noexcept {
        dispatch_sync(dispatch_get_main_queue(), ^{
            @autoreleasepool {
                if (delegate_) {
                    id d = delegate_;
                    if ([d respondsToSelector:@selector(setWrapper:)]) {
                        [d performSelector:@selector(setWrapper:) withObject:nil];
                    }
                }
                if (synth_) {
                    [synth_ setDelegate:nil];
                    [synth_ stopSpeaking];
                }
                synth_ = nil;
                delegate_ = nil;
            }
        });
    }

    bool InitializeEngine() noexcept {
        __block bool success = false;
        dispatch_sync(dispatch_get_main_queue(), ^{
            @autoreleasepool {
                synth_ = [[NSSpeechSynthesizer alloc] init];
                if (synth_) {
                    rate_ = [synth_ rate];
                    volume_ = [synth_ volume];
                    success = true;
                }
            }
        });
        return success;
    }

    void BindDelegate(id delegate) noexcept {
        dispatch_sync(dispatch_get_main_queue(), ^{
            delegate_ = delegate; 
            if (synth_) {
                [synth_ setDelegate:delegate_];
            }
        });
    }

    bool ExecuteSpeak(const char* text, bool interrupt) noexcept {
        if (!synth_ || !text) return false;
        
        std::array<char, TEXT_BUFFER_SIZE> localBuf{};
        size_t copyLength = std::min(std::strlen(text), localBuf.size() - 1);
        std::copy_n(text, copyLength, localBuf.begin());
        localBuf[copyLength] = '\0';

        dispatch_async(dispatch_get_main_queue(), ^{
            @autoreleasepool {
                if (interrupt) {
                    [synth_ stopSpeaking];
                }
                NSString* nsStr = [NSString stringWithUTF8String:localBuf.data()];
                if (!nsStr) {
                    nsStr = [NSString stringWithCString:localBuf.data() encoding:NSASCIIStringEncoding];
                }
                if (nsStr) {
                    isSpeaking_.store(true, std::memory_order_release);
                    if ([synth_ startSpeakingString:nsStr] != YES) {
                        isSpeaking_.store(false, std::memory_order_release);
                    }
                }
            }
        });
        return true; 
    }

    void ExecuteStop() noexcept {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (synth_) {
                [synth_ stopSpeaking];
            }
            isSpeaking_.store(false, std::memory_order_release);
        });
    }

    void ExecuteSetRate(int val) noexcept {
        std::lock_guard<std::mutex> lock(wrapperMutex_);
        rate_ = static_cast<float>(val);
        dispatch_async(dispatch_get_main_queue(), ^{
            if (synth_) [synth_ setRate:rate_];
        });
    }

    void ExecuteSetVolume(int val) noexcept {
        std::lock_guard<std::mutex> lock(wrapperMutex_);
        volume_ = static_cast<float>(val) / 100.0f;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (synth_) [synth_ setVolume:volume_];
        });
    }

    void GetRate(int* outValue) noexcept {
        std::lock_guard<std::mutex> lock(wrapperMutex_);
        if (outValue) *outValue = static_cast<int>(rate_);
    }

    void GetVolume(int* outValue) noexcept {
        std::lock_guard<std::mutex> lock(wrapperMutex_);
        if (outValue) *outValue = static_cast<int>(volume_ * 100.0f);
    }

    bool IsSpeaking() noexcept { 
        return isSpeaking_.load(std::memory_order_acquire); 
    }

    void OnSpeechFinished() noexcept { 
        isSpeaking_.store(false, std::memory_order_release); 
    }
};

@interface SRALSpeechDelegate : NSObject <NSSpeechSynthesizerDelegate>
@property (atomic, assign) NSSpeechSynthesizerWrapper* wrapper;
@end

@implementation SRALSpeechDelegate
- (void)speechSynthesizer:(NSSpeechSynthesizer*)sender didFinishSpeaking:(BOOL)finishedSpeaking {
    (void)sender;
    NSSpeechSynthesizerWrapper* currentWrapper = self.wrapper;
    if (currentWrapper && finishedSpeaking) {
        currentWrapper->OnSpeechFinished();
    }
}
@end

namespace Sral {

static std::atomic<NSSpeechSynthesizerWrapper*> g_sral_speech_obj{nullptr};
static std::mutex g_lifecycle_mutex;

void* NSSpeech::obj = nullptr;

bool NSSpeech::Initialize() {
    std::lock_guard<std::mutex> lock(g_lifecycle_mutex);
    if (g_sral_speech_obj.load(std::memory_order_acquire) != nullptr) return true;

    NSSpeechSynthesizerWrapper* localObj = new (std::nothrow) NSSpeechSynthesizerWrapper();
    if (!localObj) return false;

    if (!localObj->InitializeEngine()) {
        delete localObj;
        return false;
    }

    __block id localDel = nil;
    dispatch_sync(dispatch_get_main_queue(), ^{
        localDel = [[SRALSpeechDelegate alloc] init];
        if (localDel) {
            [localDel setWrapper:localObj];
        }
    });

    if (!localDel) {
        delete localObj;
        return false;
    }
    localObj->BindDelegate(localDel);
    
    g_sral_speech_obj.store(localObj, std::memory_order_release);
    obj = localObj;
    return true;
}

bool NSSpeech::Uninitialize() {
    std::lock_guard<std::mutex> lock(g_lifecycle_mutex);
    NSSpeechSynthesizerWrapper* localObj = g_sral_speech_obj.exchange(nullptr, std::memory_order_acq_rel);
    if (localObj) {
        delete localObj;
        obj = nullptr;
    }
    return true;
}

bool NSSpeech::Speak(const char* text, bool interrupt) {
    NSSpeechSynthesizerWrapper* localObj = g_sral_speech_obj.load(std::memory_order_acquire);
    if (!localObj || !text) return false;
    return localObj->ExecuteSpeak(text, interrupt);
}

bool NSSpeech::StopSpeech() {
    NSSpeechSynthesizerWrapper* localObj = g_sral_speech_obj.load(std::memory_order_acquire);
    if (!localObj) return false;
    localObj->ExecuteStop();
    return true;
}

bool NSSpeech::IsSpeaking() {
    NSSpeechSynthesizerWrapper* localObj = g_sral_speech_obj.load(std::memory_order_acquire);
    return localObj ? localObj->IsSpeaking() : false;
}

bool NSSpeech::GetActive() { 
    return g_sral_speech_obj.load(std::memory_order_acquire) != nullptr; 
}

bool NSSpeech::SetParameter(int param, const void* value) {
    NSSpeechSynthesizerWrapper* localObj = g_sral_speech_obj.load(std::memory_order_acquire);
    if (!localObj || !value) return false;

    int val = *reinterpret_cast<const int*>(value);

    switch (param) {
        case SRAL_PARAM_SPEECH_RATE:
            localObj->ExecuteSetRate(val);
            break;
        case SRAL_PARAM_SPEECH_VOLUME:
            localObj->ExecuteSetVolume(val);
            break;
        default:
            return false;
    }
    return true;
}

bool NSSpeech::GetParameter(int param, void* value) {
    NSSpeechSynthesizerWrapper* localObj = g_sral_speech_obj.load(std::memory_order_acquire);
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

}  // namespace Sral

#endif /* TARGET_OS_OSX */
#endif /* defined(__APPLE__) || defined(__MACH__) */