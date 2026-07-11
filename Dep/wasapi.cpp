#define WASAPI_EXPORTS
#define NOMINMAX
#include "wasapi.h"
#include <functiondiscoverykeys.h>
#include <functiondiscoverykeys_devpkey.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>

#ifndef PKEY_Device_FriendlyName
#undef DEFINE_PROPERTYKEY
#define DEFINE_PROPERTYKEY(id, a, b, c, d, e, f, g, h, i, j, k, l) \
    const PROPERTYKEY id = { { a, b, c, { d, e, f, g, h, i, j, k, } }, l };
DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);
#endif

constexpr REFERENCE_TIME REFTIMES_PER_MILLISEC = 10000;
constexpr REFERENCE_TIME BUFFER_SIZE = 20 * REFTIMES_PER_MILLISEC;

static IMMNotificationClientPtr g_notificationClient{ nullptr };
static std::once_flag g_notificationInitFlag;

WasapiPlayer::WasapiPlayer(std::wstring_view targetDeviceName, const WAVEFORMATEX& audioFormat, ChunkCompletedCallback endChunkCallback)
    : format(audioFormat), deviceName(targetDeviceName), callback(endChunkCallback) {
    
    std::call_once(g_notificationInitFlag, []() {
        IMMDeviceEnumeratorPtr enumerator;
        HRESULT hr = enumerator.CreateInstance(__uuidof(MMDeviceEnumerator));
        if (SUCCEEDED(hr)) {
            g_notificationClient = IMMNotificationClientPtr(new (std::nothrow) NotificationClient());
            if (g_notificationClient) [[likely]] {
                (void)enumerator->RegisterEndpointNotificationCallback(g_notificationClient);
            }
        }
    });

    ringBufferCapacity = 2 * 1024 * 1024;
    ringBuffer = std::make_unique<unsigned char[]>(ringBufferCapacity);
    rbHead.store(0, std::memory_order_relaxed);
    rbTail.store(0, std::memory_order_relaxed);
    audioEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);

    isRunning.store(true, std::memory_order_release);
    workerThread = std::thread(&WasapiPlayer::processAudioLoop, this);
}

WasapiPlayer::~WasapiPlayer() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        isRunning.store(false, std::memory_order_release);
        playState = PlayState::stopping;
    }
    queueCV.notify_all();
    if (audioEvent) ::SetEvent(audioEvent);

    if (workerThread.joinable()) {
        workerThread.join();
    }

    if (audioEvent) {
        ::CloseHandle(audioEvent);
        audioEvent = nullptr;
    }
}

size_t WasapiPlayer::getAvailableWriteSpace() const {
    size_t h = rbHead.load(std::memory_order_acquire);
    size_t t = rbTail.load(std::memory_order_relaxed);
    if (t >= h) {
        return ringBufferCapacity - (t - h) - 1;
    }
    return h - t - 1;
}

size_t WasapiPlayer::getAvailableReadSpace() const {
    size_t h = rbHead.load(std::memory_order_relaxed);
    size_t t = rbTail.load(std::memory_order_acquire);
    return (t >= h) ? (t - h) : (ringBufferCapacity - h + t);
}

void WasapiPlayer::writeToRingBuffer(size_t& tail, const unsigned char* src, size_t len) {
    unsigned char* rbPtr = ringBuffer.get();
    size_t bytesToEnd = ringBufferCapacity - tail;
    if (len <= bytesToEnd) {
        std::memcpy(&rbPtr[tail], src, len);
        tail = (tail + len) % ringBufferCapacity;
    } else {
        std::memcpy(&rbPtr[tail], src, bytesToEnd);
        std::memcpy(&rbPtr[0], src + bytesToEnd, len - bytesToEnd);
        tail = len - bytesToEnd;
    }
}

void WasapiPlayer::readFromRingBuffer(size_t& head, unsigned char* dest, size_t len) {
    const unsigned char* rbPtr = ringBuffer.get();
    size_t bytesToEnd = ringBufferCapacity - head;
    if (len <= bytesToEnd) {
        std::memcpy(dest, &rbPtr[head], len);
        head = (head + len) % ringBufferCapacity;
    } else {
        std::memcpy(dest, &rbPtr[head], bytesToEnd);
        std::memcpy(dest + bytesToEnd, &rbPtr[0], len - bytesToEnd);
        head = len - bytesToEnd;
    }
}

HRESULT WasapiPlayer::open(bool force) {
    if (client && !force) return S_OK;
    if (!g_notificationClient) [[unlikely]] return E_UNEXPECTED;

    render = nullptr;
    clock = nullptr;
    client = nullptr;

    NotificationClient* const pClientImpl = static_cast<NotificationClient*>(g_notificationClient.GetInterfacePtr());
    defaultDeviceChangeCount = pClientImpl->getDefaultDeviceChangeCount();
    deviceStateChangeCount = pClientImpl->getDeviceStateChangeCount();

    IMMDeviceEnumeratorPtr enumerator;
    HRESULT hr = enumerator.CreateInstance(__uuidof(MMDeviceEnumerator));
    if (FAILED(hr)) return hr;

    IMMDevicePtr device;
    isUsingPreferredDevice = false;

    if (deviceName.empty()) {
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    } else {
        hr = getPreferredDevice(device);
        if (SUCCEEDED(hr)) {
            isUsingPreferredDevice = true;
        } else {
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        }
    }
    if (FAILED(hr)) return hr;

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client));
    if (FAILED(hr)) return hr;

    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 
                            AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | 
                            AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY | 
                            AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 
                            BUFFER_SIZE, 0, &format, nullptr);
    if (FAILED(hr)) return hr;

    hr = client->SetEventHandle(audioEvent);
    if (FAILED(hr)) return hr;

    hr = client->GetBufferSize(&bufferFrames);
    if (FAILED(hr)) return hr;

    hr = client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&render));
    if (FAILED(hr)) return hr;

    hr = client->GetService(__uuidof(IAudioClock), reinterpret_cast<void**>(&clock));
    if (FAILED(hr)) return hr;

    hr = clock->GetFrequency(&clockFreq);
    if (FAILED(hr)) return hr;

    baseDevicePos = 0;
    return S_OK;
}

HRESULT WasapiPlayer::feed(const unsigned char* data, unsigned int size, unsigned int* id) {
    if (format.nBlockAlign == 0) [[unlikely]] return E_INVALIDARG;
    if (size == 0 || !data) return S_OK;

    const size_t requiredSpace = sizeof(unsigned int) + sizeof(unsigned int) + size;
    if (getAvailableWriteSpace() < requiredSpace) {
        return AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED; 
    }

    unsigned int chunkId = 0;
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        chunkId = nextFeedId++;
        if (id) *id = chunkId;
        
        if (playState == PlayState::stopped || playState == PlayState::stopping) {
            playState = PlayState::playing;
        }
    }

    size_t t = rbTail.load(std::memory_order_relaxed);
    writeToRingBuffer(t, reinterpret_cast<const unsigned char*>(&chunkId), sizeof(chunkId));
    writeToRingBuffer(t, reinterpret_cast<const unsigned char*>(&size), sizeof(size));
    writeToRingBuffer(t, data, size);

    rbTail.store(t, std::memory_order_release);
    queueCV.notify_one();
    return S_OK;
}

void WasapiPlayer::processAudioLoop() {
    size_t allocatedHeapSize = 16384;
    auto localHeapBlock = std::make_unique<unsigned char[]>(allocatedHeapSize);

    while (isRunning.load(std::memory_order_acquire)) {
        size_t availableBytes = getAvailableReadSpace();

        if (availableBytes < (sizeof(unsigned int) + sizeof(unsigned int))) {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait(lock, [this]() {
                return (getAvailableReadSpace() >= (sizeof(unsigned int) + sizeof(unsigned int))) 
                    || !isRunning.load(std::memory_order_acquire) 
                    || playState != PlayState::playing;
            });

            if (!isRunning.load(std::memory_order_acquire)) break;
            if (playState != PlayState::playing) continue;
            availableBytes = getAvailableReadSpace();
        }

        if (!client) {
            if (FAILED(open(false))) {
                ::Sleep(100);
                continue;
            }
        }

        size_t h = rbHead.load(std::memory_order_relaxed);
        unsigned int trackingChunkId = 0;
        unsigned int dataPayloadSize = 0;
        
        readFromRingBuffer(h, reinterpret_cast<unsigned char*>(&trackingChunkId), sizeof(trackingChunkId));
        readFromRingBuffer(h, reinterpret_cast<unsigned char*>(&dataPayloadSize), sizeof(dataPayloadSize));

        if (dataPayloadSize > allocatedHeapSize) {
            allocatedHeapSize = dataPayloadSize;
            localHeapBlock = std::make_unique<unsigned char[]>(allocatedHeapSize);
        }

        readFromRingBuffer(h, localHeapBlock.get(), dataPayloadSize);
        rbHead.store(h, std::memory_order_release);

        UINT32 totalFrames = static_cast<UINT32>(dataPayloadSize / format.nBlockAlign);
        if (totalFrames > 0) {
            (void)writeFramesToWasapi(localHeapBlock.get(), totalFrames, trackingChunkId);
        }
    }
}

        HRESULT WasapiPlayer::writeFramesToWasapi(const unsigned char* data, UINT32 totalFrames, unsigned int chunkId) {
    UINT32 remainingFrames = totalFrames;
    HRESULT hr = S_OK;

    auto reopenUsingNewDev = [&] {
        hr = open(true);
        if (FAILED(hr)) return false;

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            for (auto& [itemId, itemEnd] : feedEnds) {
                if (callback) callback(this, itemId);
            }
            feedEnds.clear();
        }
        sentFrames = 0;
        return true;
    };

    while (remainingFrames > 0 && isRunning.load(std::memory_order_acquire)) {
        // FIX: Thread-safe acquisition of playState to eliminate data race undefined behavior
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (playState != PlayState::playing) break;
        }

        NotificationClient* const pClientImpl = static_cast<NotificationClient*>(g_notificationClient.GetInterfacePtr());
        if (didPreferredDeviceBecomeAvailable() || (!isUsingPreferredDevice && defaultDeviceChangeCount != pClientImpl->getDefaultDeviceChangeCount())) {
            if (!reopenUsingNewDev()) return hr;
        }

        UINT32 paddingFrames = 0;
        hr = client->GetCurrentPadding(&paddingFrames);
        if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == AUDCLNT_E_NOT_INITIALIZED) {
            if (!reopenUsingNewDev()) return hr;
            hr = client->GetCurrentPadding(&paddingFrames);
        }
        if (FAILED(hr)) return hr;

        // OPTIMIZATION: Instant kernel-level wakeups via audioEvent (No slow thread-sleep cycles)
        if (paddingFrames >= bufferFrames) {
            ::WaitForSingleObject(audioEvent, 20); 
            continue; 
        }

        const UINT32 sendFrames = std::min(remainingFrames, bufferFrames - paddingFrames);
        if (sendFrames == 0) {
            ::WaitForSingleObject(audioEvent, 5);
            continue;
        }

        const UINT32 sendBytes = sendFrames * format.nBlockAlign;
        BYTE* buffer = nullptr;

        hr = render->GetBuffer(sendFrames, &buffer);
        if (FAILED(hr)) return hr;

        std::memcpy(buffer, data, sendBytes);
        hr = render->ReleaseBuffer(sendFrames, 0);
        if (FAILED(hr)) return hr;

        if (sentFrames == 0 && clock) {
            UINT64 rawPos = 0;
            if (SUCCEEDED(clock->GetPosition(&rawPos, nullptr))) {
                baseDevicePos = rawPos;
            }
            (void)client->Start();
        }

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            sentFrames += sendFrames;
            feedEnds.push_back({ chunkId, framesToMs(sentFrames) });
            maybeFireCallbackInternal();
        }

        data += sendBytes;
        remainingFrames -= sendFrames;
    }

    return hr;
}

void WasapiPlayer::maybeFireCallbackInternal() {
    if (!callback) return;
    const UINT64 playPos = getPlayPosInternal();

    std::erase_if(feedEnds, [&](const auto& val) noexcept -> bool {
        const auto [id, end] = val;
        if (playPos >= end) {
            callback(this, id);
            return true;
        }
        return false;
    });
}

void WasapiPlayer::maybeFireCallback() {
    std::lock_guard<std::mutex> lock(queueMutex);
    maybeFireCallbackInternal();
}

UINT64 WasapiPlayer::getPlayPosInternal() {
    if (!clock || clockFreq == 0) [[unlikely]] {
        return framesToMs(sentFrames);
    }
    UINT64 pos = 0;
    if (FAILED(clock->GetPosition(&pos, nullptr))) {
        return framesToMs(sentFrames);
    }
    UINT64 relativePos = (pos >= baseDevicePos) ? (pos - baseDevicePos) : 0;
    return (relativePos * 1000) / clockFreq;
}

UINT64 WasapiPlayer::getPlayPos() {
    std::lock_guard<std::mutex> lock(queueMutex);
    return getPlayPosInternal();
}

void WasapiPlayer::waitUntilNeeded(UINT64 maxWait) {
    if (maxWait == 0) return;

    std::unique_lock<std::mutex> lock(queueMutex);
    if (!feedEnds.empty()) {
        const UINT64 feedEnd = feedEnds.front().second;
        const UINT64 playPos = getPlayPosInternal();
        if (feedEnd > playPos) {
            maxWait = std::min(maxWait, feedEnd - playPos);
        } else {
            return;
        }
    }

    (void)queueCV.wait_for(lock, std::chrono::milliseconds(maxWait), [this]() {
        return !isRunning.load(std::memory_order_acquire) || (playState != PlayState::playing);
    });
}

HRESULT WasapiPlayer::getPreferredDevice(IMMDevicePtr& preferredDevice) {
    IMMDeviceEnumeratorPtr enumerator;
    HRESULT hr = enumerator.CreateInstance(__uuidof(MMDeviceEnumerator));
    if (FAILED(hr)) return hr;

    IMMDeviceCollectionPtr devices;
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices);
    if (FAILED(hr)) return hr;

    UINT count = 0;
    hr = devices->GetCount(&count);
    if (FAILED(hr)) return hr;

    constexpr size_t MAX_CHARS = MAXPNAMELEN - 1;
    for (UINT d = 0; d < count; ++d) {
        IMMDevicePtr device;
        hr = devices->Item(d, &device);
        if (FAILED(hr)) continue;

        IPropertyStorePtr props;
        hr = device->OpenPropertyStore(STGM_READ, &props);
        if (FAILED(hr)) continue;

        PROPVARIANT val;
        PropVariantInit(&val);
        hr = props->GetValue(PKEY_Device_FriendlyName, &val);
        if (SUCCEEDED(hr) && val.vt == VT_LPWSTR && val.pwszVal) {
            std::wstring_view systemDevName(val.pwszVal);
            size_t checkLen = std::min({ systemDevName.length(), deviceName.length(), MAX_CHARS });
            if (systemDevName.substr(0, checkLen) == deviceName.substr(0, checkLen)) {
                (void)::PropVariantClear(&val);
                preferredDevice = std::move(device);
                return S_OK;
            }
        }
        (void)::PropVariantClear(&val);
    }
    return E_NOTFOUND;
}

bool WasapiPlayer::didPreferredDeviceBecomeAvailable() {
    if (isUsingPreferredDevice || deviceName.empty() || !g_notificationClient) {
        return false;
    }
    NotificationClient* const pClientImpl = static_cast<NotificationClient*>(g_notificationClient.GetInterfacePtr());
    if (deviceStateChangeCount == pClientImpl->getDeviceStateChangeCount()) {
        return false;
    }
    IMMDevicePtr device;
    return SUCCEEDED(getPreferredDevice(device));
}

HRESULT WasapiPlayer::stop() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        playState = PlayState::stopping;
        rbHead.store(rbTail.load(std::memory_order_relaxed), std::memory_order_release);
    }
    queueCV.notify_all();
    if (audioEvent) ::SetEvent(audioEvent);

    if (client) {
        (void)client->Stop();
        (void)client->Reset();
    }

    completeStop();
    return S_OK;
}

void WasapiPlayer::completeStop() {
    std::lock_guard<std::mutex> lock(queueMutex);
    nextFeedId = 0;
    sentFrames = 0;
    feedEnds.clear();
    playState = PlayState::stopped;
}

HRESULT WasapiPlayer::sync() {
    while (true) {
        // FIX: Evaluated dynamically inside loop to verify hardware states 
        // across sudden device-swap zeroing events. Prevents permanent thread deadlocks.
        const auto [sentMs, currentPlayState] = [this]() {
            std::lock_guard<std::mutex> lock(queueMutex);
            return std::make_pair(framesToMs(sentFrames), playState);
        }();

        if (currentPlayState != PlayState::playing) {
            return S_OK;
        }

        UINT64 playPos = getPlayPos();
        if (playPos >= sentMs) {
            break; 
        }

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            maybeFireCallbackInternal();
        }

        waitUntilNeeded(sentMs - playPos);
    }

    std::lock_guard<std::mutex> lock(queueMutex);
    if (playState == PlayState::playing) {
        maybeFireCallbackInternal();
    }
    return S_OK;
}

HRESULT WasapiPlayer::idle() {
    HRESULT hr = sync();
    if (FAILED(hr)) return hr;
    return stop(); 
}

HRESULT WasapiPlayer::pause() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (playState == PlayState::playing) {
            playState = PlayState::paused; 
        }
    }
    if (client) {
        return client->Stop();
    }
    return S_OK;
}

HRESULT WasapiPlayer::resume() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (playState == PlayState::paused) {
            playState = PlayState::playing;
        }
    }
    queueCV.notify_one();
    if (audioEvent) ::SetEvent(audioEvent);
    
    if (client) {
        return client->Start();
    }
    return S_OK;
}

HRESULT WasapiPlayer::setChannelVolume(unsigned int channel, float level) {
    if (!client) return E_UNEXPECTED;
    
    level = std::max(0.0f, std::min(level, 1.0f));
    IAudioStreamVolumePtr streamVolume = nullptr;
    
    HRESULT hr = client->GetService(__uuidof(IAudioStreamVolume), reinterpret_cast<void**>(&streamVolume));
    if (SUCCEEDED(hr)) {
        UINT32 channelCount = 0;
        hr = streamVolume->GetChannelCount(&channelCount);
        if (SUCCEEDED(hr) && channel < channelCount) {
            hr = streamVolume->SetChannelVolume(channel, level);
        }
    }
    return hr;
}
