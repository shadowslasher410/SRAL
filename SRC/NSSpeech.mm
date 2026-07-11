#if defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>

#if defined(TARGET_OS_OSX) && TARGET_OS_OSX

#include "NsSpeech.h"
#import <AppKit/AppKit.h>
#include <mutex>
#include <atomic>
#include <thread>
#include <semaphore>
#include <array>
#include <memory>
#include <algorithm>
#include <cstring>

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
public:
    enum class CmdType { None, SpeakCmd, StopCmd, SetRateCmd, SetVolumeCmd };
    
    static constexpr size_t TEXT_BUFFER_SIZE = 512;
    static constexpr size_t RING_BUFFER_CAPACITY = 32;

    struct Command {
        CmdType type = CmdType::None;
        std::array<char, TEXT_BUFFER_SIZE> textBuffer{};
        bool interrupt = false;
        int paramVal = 0;
    };

    struct SharedState {
        std::mutex queueMutex;
        std::array<Command, RING_BUFFER_CAPACITY> ringBuffer{};
        size_t rbHead{0};
        size_t rbTail{0};
        std::counting_semaphore<RING_BUFFER_CAPACITY> queueSemaphore{0};
        std::counting_semaphore<1> initSemaphore{0};
    };

    struct RuntimeContext {
        std::jthread workerThread;
        SharedState state;
    };

private:
    std::mutex wrapperMutex_;
    NSSpeechSynthesizer* synth_;
    SRALSpeechDelegate* delegate_;
    float rate_;
    float volume_;
    std::atomic<bool> isSpeaking_{false};
    
    static inline std::shared_ptr<RuntimeContext> s_context{ nullptr };

public:
    NSSpeechSynthesizerWrapper() 
        : synth_(nil), delegate_(nil), rate_(175.0f), volume_(1.0f) {}

    ~NSSpeechSynthesizerWrapper() {
        InternalCleanup();
    }

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

    bool InitializeEngine(std::shared_ptr<RuntimeContext> context) noexcept {
        @autoreleasepool {
            synth_ = [[NSSpeechSynthesizer alloc] init];
            if (!synth_) return false;

            rate_ = [synth_ rate];
            volume_ = [synth_ volume];
            
            delegate_ = [[SRALSpeechDelegate alloc] init];
            if (!delegate_) return false;

            [delegate_ setWrapper:this];
            [synth_ setDelegate:delegate_];
            return true;
        }
    }

    bool ExecuteSpeak(const char* text, bool interrupt) noexcept {
        if (!synth_ || !text) return false;
        @autoreleasepool {
            if (interrupt) {
                [synth_ stopSpeaking];
            }

            NSString* nsStr = [NSString stringWithUTF8String:text];
            if (!nsStr) {
                nsStr = [NSString stringWithCString:text encoding:NSASCIIStringEncoding];
                if (!nsStr) return false;
            }
            isSpeaking_.store(true, std::memory_order_relaxed);
            return [synth_ startSpeakingString:nsStr] == YES;
        }
    }

    void ExecuteStop() noexcept {
        if (synth_) {
            [synth_ stopSpeaking];
        }
        isSpeaking_.store(false, std::memory_order_relaxed);
    }

    void ExecuteSetRate(int val) noexcept {
        std::lock_guard<std::mutex> lock(wrapperMutex_);
        rate_ = static_cast<float>(val);
        if (synth_) {
            [synth_ setRate:rate_];
        }
    }

    void ExecuteSetVolume(int val) noexcept {
        std::lock_guard<std::mutex> lock(wrapperMutex_);
        volume_ = static_cast<float>(val) / 100.0f;
        if (synth_) {
            [synth_ setVolume:volume_];
        }
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
        return isSpeaking_.load(std::memory_order_relaxed);
    }

    void OnSpeechFinished() noexcept {
        isSpeaking_.store(false, std::memory_order_relaxed);
    }

    static void WorkerThreadLoop(std::stop_token stopToken, std::shared_ptr<RuntimeContext> context, NSSpeechSynthesizerWrapper* wrapper) {
        @autoreleasepool {
            bool success = wrapper->InitializeEngine(context);
            
            context->state.initSemaphore.release();
            if (!success) return;

            while (!stopToken.stop_requested()) {
                bool earnedToken = context->state.queueSemaphore.try_acquire_for(std::chrono::milliseconds(10));
                
                [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.005]];

                if (stopToken.stop_requested()) break;
                if (!earnedToken) continue;

                Command cmd{};
                bool hasCommand = false;

                {
                    std::lock_guard<std::mutex> lock(context->state.queueMutex);
                    if (context->state.rbHead != context->state.rbTail) {
                        cmd = context->state.ringBuffer[context->state.rbHead];
                        context->state.rbHead = (context->state.rbHead + 1) % RING_BUFFER_CAPACITY;
                        hasCommand = true;
                    }
                }

                if (!hasCommand) continue;

                switch (cmd.type) {
                    case CmdType::SpeakCmd:
                        wrapper->ExecuteSpeak(cmd.textBuffer.data(), cmd.interrupt);
                        break;
                    case CmdType::StopCmd:
                        wrapper->ExecuteStop();
                        break;
                    case CmdType::SetRateCmd:
                        wrapper->ExecuteSetRate(cmd.paramVal);
                        break;
                    case CmdType::SetVolumeCmd:
                        wrapper->ExecuteSetVolume(cmd.paramVal);
                        break;
                    default:
                        break;
                }
            }
            
            wrapper->InternalCleanup();
        }
    }

public:
    [[nodiscard]] bool IsInitialized() const noexcept {
        return synth_ != nil;
    }

    static bool InitializeGlobal(NSSpeechSynthesizerWrapper* wrapper) {
        if (s_context) {
            if (s_context->workerThread.joinable() && wrapper->IsInitialized()) {
                return true;
            }
            UninitializeGlobal();
        }

        s_context = std::make_shared<RuntimeContext>();
        
        {
            std::lock_guard<std::mutex> lock(s_context->state.queueMutex);
            s_context->state.rbHead = 0;
            s_context->state.rbTail = 0;
            while (s_context->state.queueSemaphore.try_acquire());
            while (s_context->state.initSemaphore.try_acquire());
        }

        s_context->workerThread = std::jthread(&NSSpeechSynthesizerWrapper::WorkerThreadLoop, s_context, wrapper);
        s_context->state.initSemaphore.acquire();
        if (!wrapper->IsInitialized()) {
            if (s_context->workerThread.joinable()) {
                s_context->workerThread.detach();
            }
            s_context = nullptr;
            return false;
        }

        return true;
    }

    static void UninitializeGlobal() {
        if (!s_context || !s_context->workerThread.joinable()) return;

        {
            std::lock_guard<std::mutex> lock(s_context->state.queueMutex);
            s_context->workerThread.request_stop();
            s_context->state.queueSemaphore.release();
        }
        
        s_context->workerThread.detach();
        s_context = nullptr;
    }

    static bool PushCommand(const Command& cmd) {
        if (!s_context) return false;
        std::lock_guard<std::mutex> lock(s_context->state.queueMutex);
        
        size_t t = s_context->state.rbTail;
        size_t h = s_context->state.rbHead;
        
        if (((t + 1) % RING_BUFFER_CAPACITY) == h) [[unlikely]] {
            return false; 
        }

        s_context->state.ringBuffer[t] = cmd;
        s_context->state.rbTail = (t + 1) % RING_BUFFER_CAPACITY;
        s_context->state.queueSemaphore.release();
        return true;
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

    if (!NSSpeechSynthesizerWrapper::InitializeGlobal(localObj)) {
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
        NSSpeechSynthesizerWrapper::UninitializeGlobal();
        delete localObj; 
        obj = nullptr;
    }
    return true;
}

bool NsSpeech::Speak(const char* text, bool interrupt) {
    NSSpeechSynthesizerWrapper* localObj = g_sral_speech_obj.load();
    if (!localObj || !text) return false;

    NSSpeechSynthesizerWrapper::Command cmd{};
    cmd.type = NSSpeechSynthesizerWrapper::CmdType::SpeakCmd;
    cmd.interrupt = interrupt;
    
    size_t copyLength = std::min(std::strlen(text), cmd.textBuffer.size() - 1);
    std::copy_n(text, copyLength, cmd.textBuffer.begin());
    cmd.textBuffer[copyLength] = '\0';

    return NSSpeechSynthesizerWrapper::PushCommand(cmd);
}

bool NsSpeech::StopSpeech() {
    NSSpeechSynthesizerWrapper* localObj = g_sral_speech_obj.load();
    if (!localObj) return false;
    NSSpeechSynthesizerWrapper::Command cmd{};
    cmd.type = NSSpeechSynthesizerWrapper::CmdType::StopCmd;
    return NSSpeechSynthesizerWrapper::PushCommand(cmd);
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
    NSSpeechSynthesizerWrapper::Command cmd{};
    
    switch (param) {
        case SRAL_PARAM_SPEECH_RATE:
            cmd.type = NSSpeechSynthesizerWrapper::CmdType::SetRateCmd;
            cmd.paramVal = val;
            break;
        case SRAL_PARAM_SPEECH_VOLUME:
            cmd.type = NSSpeechSynthesizerWrapper::CmdType::SetVolumeCmd;
            cmd.paramVal = val;
            break;
        default:
            return false;
    }
    return NSSpeechSynthesizerWrapper::PushCommand(cmd);
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
