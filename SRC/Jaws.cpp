#include "Jaws.h"
#include "Encoding.h"

namespace Sral {

static std::atomic<bool> g_jawsConnected{false};

bool Jaws::Initialize() {
    if (s_context && s_context->workerThread.joinable()) return true;
    s_context = std::make_shared<RuntimeContext>();
    g_jawsConnected.store(false, std::memory_order_relaxed);
    s_context->workerThread = std::jthread(&Jaws::WorkerThreadLoop, s_context);
    s_context->state.initSemaphore.acquire();

    return g_jawsConnected.load(std::memory_order_relaxed);
}

bool Jaws::Uninitialize() {
    if (!s_context || !s_context->workerThread.joinable()) return true;

    {
        std::lock_guard<std::mutex> lock(s_context->state.queueMutex);
        s_context->workerThread.request_stop();
        s_context->state.queueSemaphore.release();
    }
    
    s_context->workerThread.detach();
    
    s_context = nullptr; 
    return true;
}

bool Jaws::GetActive() {
    return (FindWindowW(L"JFWUI2", nullptr) != nullptr) && g_jawsConnected.load(std::memory_order_relaxed);
}

bool Jaws::Speak(const char* text, bool interrupt) {
    if (!text || !GetActive() || !s_context) return false;

    std::wstring wstr;
    if (!UnicodeConvert(text, wstr)) return false;

    std::lock_guard<std::mutex> lock(s_context->state.queueMutex);
    
    size_t t = s_context->state.rbTail;
    size_t h = s_context->state.rbHead;
    
    if (((t + 1) % RING_BUFFER_CAPACITY) == h) [[unlikely]] {
        return false; 
    }

    Command& cmd = s_context->state.ringBuffer[t];
    cmd.type = CmdType::SpeakCmd;
    cmd.interrupt = interrupt;
    
    size_t copyLength = (std::min)(wstr.size(), cmd.textBuffer.size() - 1);
    std::copy_n(wstr.begin(), copyLength, cmd.textBuffer.begin());
    cmd.textBuffer[copyLength] = L'\0';
    
    s_context->state.rbTail = (t + 1) % RING_BUFFER_CAPACITY;
    s_context->state.rbTail = (t + 1) % RING_BUFFER_CAPACITY;
    s_context->state.queueSemaphore.release(); 
    return true;
}

bool Jaws::Braille(const char* text) {
    if (!text || !GetActive() || !s_context) return false;

    std::wstring wstr;
    if (!UnicodeConvert(text, wstr)) return false;

    for (auto& ch : wstr) {
        if (ch == L'"') ch = L'\'';
    }

    wstr.insert(0, L"BrailleString(\"");
    wstr.append(L"\")");

    std::lock_guard<std::mutex> lock(s_context->state.queueMutex);
    
    size_t t = s_context->state.rbTail;
    size_t h = s_context->state.rbHead;
    
    if (((t + 1) % RING_BUFFER_CAPACITY) == h) [[unlikely]] {
        return false; 
    }

    Command& cmd = s_context->state.ringBuffer[t];
    cmd.type = CmdType::BrailleCmd;
    cmd.interrupt = false;
    
    size_t copyLength = (std::min)(wstr.size(), cmd.textBuffer.size() - 1);
    std::copy_n(wstr.begin(), copyLength, cmd.textBuffer.begin());
    cmd.textBuffer[copyLength] = L'\0';
    
    s_context->state.rbTail = (t + 1) % RING_BUFFER_CAPACITY;
    s_context->state.queueSemaphore.release();
    return true;
}

bool Jaws::StopSpeech() {
    if (!GetActive() || !s_context) return false;
    
    std::lock_guard<std::mutex> lock(s_context->state.queueMutex);
    
    size_t t = s_context->state.rbTail;
    size_t h = s_context->state.rbHead;
    
    if (((t + 1) % RING_BUFFER_CAPACITY) == h) [[unlikely]] {
        return false; 
    }

    Command& cmd = s_context->state.ringBuffer[t];
    cmd.type = CmdType::StopCmd;
    cmd.interrupt = false;
    cmd.textBuffer[0] = L'\0';
    
    s_context->state.rbTail = (t + 1) % RING_BUFFER_CAPACITY;
    s_context->state.queueSemaphore.release();
    return true;
}

bool Jaws::IsSpeaking() {
    return false; 
}

void Jaws::WorkerThreadLoop(std::stop_token stopToken, std::shared_ptr<RuntimeContext> context) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IJawsApiPtr localJawsApi = nullptr;

    if (SUCCEEDED(hr)) {
        hr = localJawsApi.CreateInstance(CLSID_JawsApi, nullptr, CLSCTX_INPROC_SERVER);
        if (SUCCEEDED(hr)) {
            g_jawsConnected.store(true, std::memory_order_relaxed);
        }
    }

    context->state.initSemaphore.release();

    if (!g_jawsConnected.load(std::memory_order_relaxed)) {
        if (SUCCEEDED(hr)) CoUninitialize();
        return;
    }

    while (!stopToken.stop_requested()) {
        context->state.queueSemaphore.acquire();

        if (stopToken.stop_requested()) break;

        Command cmd;
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
            case CmdType::SpeakCmd: {
                if (cmd.interrupt) {
                    (void)localJawsApi->StopSpeech();
                }
                _bstr_t bstrText(cmd.textBuffer.data());
                VARIANT_BOOL result = VARIANT_FALSE;
                const VARIANT_BOOL flush = cmd.interrupt ? VARIANT_TRUE : VARIANT_FALSE;
                (void)localJawsApi->SayString(bstrText, flush, &result);
                break;
            }
            case CmdType::BrailleCmd: {
                _bstr_t bstrCommand(cmd.textBuffer.data());
                VARIANT_BOOL result = VARIANT_FALSE;
                (void)localJawsApi->RunFunction(bstrCommand, &result);
                break;
            }
            case CmdType::StopCmd: {
                (void)localJawsApi->StopSpeech();
                break;
            }
            default:
                break;
        }
    }

    g_jawsConnected.store(false, std::memory_order_relaxed);
    localJawsApi = nullptr;
    CoUninitialize();
    
}

} // namespace Sral
