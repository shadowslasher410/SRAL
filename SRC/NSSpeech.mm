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

@interface SRALSpeechDelegate : NSObject <NSSpeechSynthesizerDelegate>
@property (atomic, assign) class NSSpeechSynthesizerWrapper* wrapper;
@end

class NSSpeechSynthesizerWrapper final {
public:
    static constexpr size_t TEXT_BUFFER_SIZE = 512;

private:
    NSSpeechSynthesizer* synth_;
    SRALSpeechDelegate* delegate_;
    
    std::atomic<float> rate_;
    std::atomic<float> volume_;
    std::atomic<bool> isSpeaking_{false};

public:
    NSSpeechSynthesizerWrapper() : synth_(nil), delegate_(nil), rate_(175.0f), volume_(1.0f) {}
    ~NSSpeechSynthesizerWrapper() { InternalCleanup(); }

    NSSpeechSynthesizerWrapper(const NSSpeechSynthesizerWrapper&) = delete;
    NSSpeechSynthesizerWrapper& operator=(const NSSpeechSynthesizerWrapper&) = delete;
    NSSpeechSynthesizerWrapper(NSSpeechSynthesizerWrapper&&) noexcept = delete;
    NSSpeechSynthesizerWrapper& operator=(NSSpeechSynthesizerWrapper&&) noexcept = delete;

    void InternalCleanup() noexcept {
        auto cleanupBlock = ^{
            @autoreleasepool {
                if (delegate_) {
                    delegate_.wrapper = nullptr;
                }
                if (synth_) {
                    [synth_ setDelegate:nil];
                    [synth_ stopSpeaking];
                }
                synth_ = nil;
                delegate_ = nil;
            }
        };

        if ([NSThread isMainThread]) {
            cleanupBlock();
        } else {
            dispatch_sync(dispatch_get_main_queue(), cleanupBlock);
        }
    }

    bool InitializeEngine() noexcept {
        __block bool success = false;
        auto initBlock = ^{
            @autoreleasepool {
                synth_ = [[NSSpeechSynthesizer alloc] init];
                if (synth_) {
                    rate_.store([synth_ rate], std::memory_order_relaxed);
                    volume_.store([synth_ volume], std::memory_order_relaxed);
                    success = true;
                }
            }
        };

        if ([NSThread isMainThread]) {
            initBlock();
        } else {
            dispatch_sync(dispatch_get_main_queue(), initBlock);
        }
        return success;
    }

    void BindDelegate(SRALSpeechDelegate* delegate) noexcept {
        auto bindBlock = ^{
            delegate_ = delegate; 
            if (synth_) {
                [synth_ setDelegate:delegate_];
            }
        };

        if ([NSThread isMainThread]) {
            bindBlock();
        } else {
            dispatch_sync(dispatch_get_main_queue(), bindBlock);
        }
    }

    bool ExecuteSpeak(const char* text, bool interrupt) noexcept {
        if (!synth_ || !text || text[0] == '\0') [[unlikely]] return false;
        __block NSString* nsStr = nil;
        @autoreleasepool {
            nsStr = [NSString stringWithUTF8String:text];
            if (!nsStr) [[unlikely]] {
                nsStr = [NSString stringWithCString:text encoding:NSASCIIStringEncoding];
            }
        }
        if (!nsStr) [[unlikely]] return false;

        dispatch_async(dispatch_get_main_queue(), ^{
            @autoreleasepool {
                if (interrupt) {
                    [synth_ stopSpeaking];
                }
                isSpeaking_.store(true, std::memory_order_release);
                if ([synth_ startSpeakingString:nsStr] != YES) {
                    isSpeaking_.store(false, std::memory_order_release);
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
        const float targetRate = static_cast<float>(val);
        rate_.store(targetRate, std::memory_order_relaxed);
        dispatch_async(dispatch_get_main_queue(), ^{
            if (synth_) [synth_ setRate:targetRate];
        });
    }

    void ExecuteSetVolume(int val) noexcept {
        const float targetVolume = static_cast<float>(val) / 100.0f;
        volume_.store(targetVolume, std::memory_order_relaxed);
        dispatch_async(dispatch_get_main_queue(), ^{
            if (synth_) [synth_ setVolume:targetVolume];
        });
    }

    void GetRate(int* outValue) noexcept {
        if (outValue != nullptr) [[likely]] {
            *outValue = static_cast<int>(rate_.load(std::memory_order_relaxed));
        }
    }

    void GetVolume(int* outValue) noexcept {
        if (outValue != nullptr) [[likely]] {
            *outValue = static_cast<int>(volume_.load(std::memory_order_relaxed) * 100.0f);
        }
    }

    bool IsSpeaking() noexcept { 
        return isSpeaking_.load(std::memory_order_acquire); 
    }

    void OnSpeechFinished() noexcept { 
        isSpeaking_.store(false, std::memory_order_release); 
    }
};

@implementation SRALSpeechDelegate
- (void)speechSynthesizer:(NSSpeechSynthesizer*)sender didFinishSpeaking:(BOOL)finishedSpeaking {
    (void)sender;
    NSSpeechSynthesizerWrapper* currentWrapper = self.wrapper;
    if (currentWrapper && finishedSpeaking) [[likely]] {
        currentWrapper->OnSpeechFinished();
    }
}
@end

namespace Sral {

static std::atomic<NSSpeechSynthesizerWrapper*> g_sral_speech_obj{nullptr};
static std::mutex g_lifecycle_mutex;

void* NSSpeech::obj = nullptr;

bool NSSpeech::Initialize() {
    if (g_sral_speech_obj.load(std::memory_order_acquire) != nullptr) return true;
    std::lock_guard<std::mutex> lock(g_lifecycle_mutex);
    if (g_sral_speech_obj.load(std::memory_order_relaxed) != nullptr) return true;

    NSSpeechSynthesizerWrapper* localObj = new (std::nothrow) NSSpeechSynthesizerWrapper();
    if (!localObj) [[unlikely]] return false;

    if (!localObj->InitializeEngine()) {
        delete localObj;
        return false;
    }

    __block SRALSpeechDelegate* localDel = nil;
    auto delegateBlock = ^{
        localDel = [[SRALSpeechDelegate alloc] init];
        if (localDel) {
            localDel.wrapper = localObj;
        }
    };

    if ([NSThread isMainThread]) {
        delegateBlock();
    } else {
        dispatch_sync(dispatch_get_main_queue(), delegateBlock);
    }

    if (!localDel) [[unlikely]] {
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
    if (!localObj || !text) [[unlikely]] return false;
    return localObj->ExecuteSpeak(text, interrupt);
}

bool NSSpeech::StopSpeech() {
    NSSpeechSynthesizerWrapper* localObj = g_sral_speech_obj.load(std::memory_order_acquire);
    if (!localObj) [[unlikely]] return false;
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
    if (value == nullptr || localObj == nullptr) [[unlikely]] return false;

    const int val = *reinterpret_cast<const int*>(value);

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
    if (value == nullptr || localObj == nullptr) [[unlikely]] return false;

    switch (param) {
        case SRAL_PARAM_SPEECH_RATE:
            localObj->GetRate(reinterpret_cast<int*>(value));
            return true;
            
        case SRAL_PARAM_SPEECH_VOLUME:
            localObj->GetVolume(reinterpret_cast<int*>(value));
            return true;
            
        default:
            return Engine::GetParameter(param, value);
    }
}

}  // namespace Sral

#endif /* TARGET_OS_OSX && TARGET_OS_OSX */
#endif /* defined(__APPLE__) || defined(__MACH__) */