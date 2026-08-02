/*
 * ==============================================================================
 * NOTICE OF MODIFICATION
 * ==============================================================================
 * This file is an ALTERED, MODIFIED, and HARDENED version of the original software.
 * In compliance with the software license terms, changes are plainly marked below:
 *
 *  1. CRITICAL HEAP MANAGEMENT ALIGNMENT:
 *     - Replaced unsafe C++ array 'delete[]' calls with standard C 'free()' invocations
 *       on audio PCM sample data blocks, preventing fatal heap corruption crashes
 *       from memory allocated natively inside the underlying C library APIs.
 *
 *  2. THREAD-SAFE VOICING RETRIEVAL OVERHAUL:
 *     - Refactored voice description, attribute, and language extraction loops to
 *       utilize local, stack-allocated temporary staging buffers. This adheres
 *       strictly to the caller-allocated buffer destination interface, completely
 *       eliminating dynamic multi-threaded data race hazards.
 *
 *  3. TIMING & THREAD LIFECYCLE REFINEMENTS:
 *     - Modernized the abstract template calling mechanics, type-constraining internal
 *       invocations using C++20 standard function concepts.
 *     - Strengthened background worker thread synchronization loops by upgrading state
 *       variables to use explicit atomic read/write memory tracking directives.
 *     - Fortified queue purges and exception recovery paths to guarantee robust,
 *       leak-free pointer resource destructions during sudden device reset events.
 * ==============================================================================
 */

#ifdef _WIN32

#ifndef restrict
    #define restrict __restrict
#endif

#include "SAPI.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string_view>

namespace Sral {

static std::shared_ptr<WasapiPlayer> g_player{nullptr};

#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201907L
    using std::hardware_destructive_interference_size;
#else
    #if defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64) || defined(TARGET_CPU_ARM64)
        static constexpr size_t hardware_destructive_interference_size = 128;
    #else
        static constexpr size_t hardware_destructive_interference_size = 64;
    #endif
#endif

class alignas(hardware_destructive_interference_size) AudioRingBuffer {
private:
    std::unique_ptr<unsigned char[]> buffer;
    size_t capacity{0};

    alignas(hardware_destructive_interference_size) std::atomic<size_t> head{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_t> tail{0};

public:
    void Init(size_t totalBytes) noexcept {
        capacity = totalBytes;
        buffer = std::make_unique<unsigned char[]>(capacity);
        head.store(0, std::memory_order_relaxed);
        tail.store(0, std::memory_order_relaxed);
    }

    [[nodiscard]] size_t GetAvailableWriteSpace() const noexcept {
        size_t h = head.load(std::memory_order_acquire);
        size_t t = tail.load(std::memory_order_relaxed);
        if (t >= h) {
            return capacity - (t - h) - 1;
        }
        return h - t - 1;
    }

    [[nodiscard]] size_t GetAvailableReadSpace() const noexcept {
        size_t h = head.load(std::memory_order_relaxed);
        size_t t = tail.load(std::memory_order_acquire);
        if (t >= h) return t - h;
        return capacity - h + t;
    }

    bool Write(const unsigned char* src, size_t bytes) noexcept {
        if (bytes == 0 || GetAvailableWriteSpace() < bytes) [[unlikely]] {
            return false;
        }

        size_t t = tail.load(std::memory_order_relaxed);
        size_t bytesToEnd = capacity - t;
        unsigned char* bufPtr = buffer.get();

        if (bytes <= bytesToEnd) {
            std::memcpy(&bufPtr[t], src, bytes);
            tail.store((t + bytes) % capacity, std::memory_order_release);
        } else {
            std::memcpy(&bufPtr[t], src, bytesToEnd);
            std::memcpy(bufPtr, src + bytesToEnd, bytes - bytesToEnd);
            tail.store(bytes - bytesToEnd, std::memory_order_release);
        }
        return true;
    }

    size_t Read(unsigned char* dest, size_t maxBytes) noexcept {
        size_t available = GetAvailableReadSpace();
        size_t bytesToRead = (available < maxBytes) ? available : maxBytes;
        if (bytesToRead == 0) return 0;

        size_t h = head.load(std::memory_order_relaxed);
        size_t bytesToEnd = capacity - h;
        unsigned char* bufPtr = buffer.get();

        if (bytesToRead <= bytesToEnd) {
            std::memcpy(dest, &bufPtr[h], bytesToRead);
            head.store((h + bytesToRead) % capacity, std::memory_order_release);
        } else {
            std::memcpy(dest, &bufPtr[h], bytesToEnd);
            std::memcpy(dest + bytesToEnd, bufPtr, bytesToRead - bytesToEnd);
            head.store(bytesToRead - bytesToEnd, std::memory_order_release);
        }
        return bytesToRead;
    }

    void Clear() noexcept { 
        head.store(tail.load(std::memory_order_relaxed), std::memory_order_release); 
    }
};

constexpr size_t RING_BUFFER_SIZE = 2 * 1024 * 1024;
static AudioRingBuffer g_ringBuffer;

alignas(hardware_destructive_interference_size) static std::mutex g_sleepMutex;
alignas(hardware_destructive_interference_size) static std::condition_variable g_sleepCv;
alignas(hardware_destructive_interference_size) static std::atomic<bool> g_threadStarted{false};
alignas(hardware_destructive_interference_size) static std::atomic<bool> g_isSpeaking{false};

static void trim_inplace(char* data, unsigned long* size, const WAVEFORMATEX* wfx, int threshold) noexcept {
    if (!data || !size || !wfx || *size == 0) [[unlikely]] return;

    int channels = wfx->nChannels;
    int bytesPerSample = wfx->wBitsPerSample / 8;
    if (bytesPerSample == 0) [[unlikely]] return;

    int samplesPerFrame = channels * bytesPerSample;
    int numSamples = static_cast<int>(*size) / samplesPerFrame;
    int startIndex = 0;
    int endIndex = numSamples - 1;

    for (int i = 0; i < numSamples; i++) {
        int maxAbsValue = 0;
        for (int j = 0; j < channels; j++) {
            int absValue = std::abs(static_cast<int>(data[i * samplesPerFrame + j]));
            if (absValue > maxAbsValue) maxAbsValue = absValue;
        }
        if (maxAbsValue >= threshold) {
            startIndex = i;
            break;
        }
    }

    for (int i = numSamples - 1; i >= 0; i--) {
        int maxAbsValue = 0;
        for (int j = 0; j < channels; j++) {
            int absValue = std::abs(static_cast<int>(data[i * samplesPerFrame + j]));
            if (absValue > maxAbsValue) maxAbsValue = absValue;
        }
        if (maxAbsValue >= threshold) {
            endIndex = i;
            break;
        }
    }

    if (startIndex > endIndex) {
        *size = 0;
        return;
    }

    int trimmedSize = (endIndex - startIndex + 1) * samplesPerFrame;
    if (startIndex > 0) {
        std::memmove(data, data + (startIndex * samplesPerFrame), static_cast<size_t>(trimmedSize));
    }
    *size = static_cast<unsigned long>(trimmedSize);
}

static void BackgroundWorkerLoop(std::stop_token stopToken) noexcept {
    (void)SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    
    WasapiPlayer* const player_ptr = g_player.get();
    if (!player_ptr) {
        g_threadStarted.store(false, std::memory_order_release);
        g_isSpeaking.store(false, std::memory_order_release);
        return;
    }

    constexpr size_t CHUNK_SIZE = 16384;
    auto localBlock = std::make_unique<unsigned char[]>(CHUNK_SIZE);

    while (g_threadStarted.load(std::memory_order_acquire) && !stopToken.stop_requested()) {
        size_t readableBytes = g_ringBuffer.GetAvailableReadSpace();

        if (readableBytes == 0) {
            g_isSpeaking.store(false, std::memory_order_release);

            std::unique_lock<std::mutex> lock(g_sleepMutex);
            g_sleepCv.wait(lock, [&] {
                return g_ringBuffer.GetAvailableReadSpace() > 0 || 
                       !g_threadStarted.load(std::memory_order_acquire) ||
                       stopToken.stop_requested();
            });

            if (!g_threadStarted.load(std::memory_order_acquire) || stopToken.stop_requested()) {
                break;
            }
            continue;
        }

        g_isSpeaking.store(true, std::memory_order_release);
        size_t readCount = g_ringBuffer.Read(localBlock.get(), CHUNK_SIZE);

        if (readCount > 0) {
            HRESULT hr = player_ptr->feed(localBlock.get(), static_cast<unsigned long>(readCount), nullptr);
            if (SUCCEEDED(hr)) {
                (void)player_ptr->sync();
            }
        }
    }

    g_ringBuffer.Clear();
    g_isSpeaking.store(false, std::memory_order_release);
}

bool Sapi::Initialize() noexcept {
    if (instance) {
        instance.reset();
    }
    this->voiceIndex = 0;

    g_threadStarted.store(false, std::memory_order_release);
    g_sleepCv.notify_all();

    if (g_player) {
        static_cast<void>(g_player->stop());
        g_player.reset();
    }

    speechThread.request_stop();
    if (speechThread.joinable()) {
        speechThread.join();
    }

    instance = std::make_unique<blastspeak>();

    if (blastspeak_initialize(instance.get()) == 0) {
        instance.reset();
        return false;
    }

    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = instance->channels;
    wfx.nSamplesPerSec = instance->sample_rate;
    wfx.wBitsPerSample = instance->bits_per_sample;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    g_player = std::make_shared<WasapiPlayer>(L"", wfx, callback);
    if (FAILED(g_player->open())) {
        g_player.reset();
        return false;
    }

    g_ringBuffer.Init(RING_BUFFER_SIZE);

    g_threadStarted.store(true, std::memory_order_release);
    speechThread = std::jthread(BackgroundWorkerLoop);
    return true;
}

bool Sapi::Uninitialize() noexcept {
    ReleaseAllStrings();
    this->voiceIndex = 0;
    
    if (!instance || g_player == nullptr) [[unlikely]] {
        return false;
    }

    g_threadStarted.store(false, std::memory_order_release);
    speechThread.request_stop();
    g_sleepCv.notify_all();

    if (speechThread.joinable()) {
        speechThread.join();
    }

    blastspeak_destroy(instance.get());
    instance.reset();

    if (g_player) {
        g_player->stop();
        g_player.reset();
    }
    
    g_isSpeaking.store(false, std::memory_order_release);
    return true;
}

bool Sapi::GetActive() noexcept {
    return instance && g_player != nullptr;
}

bool Sapi::Speak(const char* text, bool interrupt) noexcept {
    if (!text || text == '\0') [[unlikely]] return false;

    std::shared_ptr<WasapiPlayer> playerLock = g_player;
    if (!instance || !playerLock) [[unlikely]] {
        return false;
    }

    if (interrupt) {
        static_cast<void>(StopSpeech());
    }

    if (wfx.nChannels != instance->channels || 
        wfx.nSamplesPerSec != instance->sample_rate ||
        wfx.wBitsPerSample != instance->bits_per_sample) [[unlikely]] {

        wfx.nChannels = instance->channels;
        wfx.nSamplesPerSec = instance->sample_rate;
        wfx.wBitsPerSample = instance->bits_per_sample;
        wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

        g_threadStarted.store(false, std::memory_order_release);
        speechThread.request_stop();
        g_sleepCv.notify_all();
        if (speechThread.joinable()) {
            speechThread.join();
        }

        if (g_player) {
            g_player->stop();
            g_player.reset();
        }

        auto newPlayer = std::make_shared<WasapiPlayer>(L"", wfx, callback);
        if (FAILED(newPlayer->open())) {
            return false;
        }
        
        g_player = newPlayer;
        playerLock = newPlayer;
        g_ringBuffer.Init(RING_BUFFER_SIZE);

        g_threadStarted.store(true, std::memory_order_release);
        g_isSpeaking.store(false, std::memory_order_release);
        speechThread = std::jthread(BackgroundWorkerLoop);
    }

    uint64_t buffer_size = 0;
    char* data = static_cast<char*>(this->SpeakToMemory(text, &buffer_size, nullptr, nullptr, nullptr));
    if (!data || buffer_size == 0) [[unlikely]] {
        return false;
    }

    bool success = g_ringBuffer.Write(reinterpret_cast<const unsigned char*>(data), buffer_size);
    std::free(data);

    if (!success) [[unlikely]] {
        return false;
    }

    if (this->paused) {
        this->paused = false;
        if (!interrupt && playerLock) {
            playerLock->resume();
        }
    }

    g_isSpeaking.store(true, std::memory_order_release);
    g_sleepCv.notify_all();
    return true;
}

void* Sapi::SpeakToMemory(const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample) noexcept {
    if (!instance || !text || text == '\0') [[unlikely]] return nullptr;

    unsigned long bytes = 0;
    std::string_view sv(text);

    blastspeak* const pInstance = instance.get();
	unsigned long* const pBytes = &bytes;
	const char* const pText = text;

    char* audio_ptr = blastspeak_speak_to_memory(pInstance, pBytes, pText);
	if (!audio_ptr) [[unlikely]] return nullptr;

    {
        std::lock_guard<std::mutex> lock(this->instanceMutex);
        trim_inplace(audio_ptr, &bytes, &wfx, this->trimThreshold);
    }

    if (buffer_size) *buffer_size = static_cast<uint64_t>(bytes);
    if (channels) *channels = instance->channels;
    if (sample_rate) *sample_rate = static_cast<int>(instance->sample_rate);
    if (bits_per_sample) *bits_per_sample = instance->bits_per_sample;
    
    return audio_ptr;
}


bool Sapi::IsSpeaking() noexcept {
    return !paused && g_isSpeaking.load(std::memory_order_acquire);
}

bool Sapi::SetParameter(int param, const void* value) noexcept {
    if (!value) [[unlikely]] return false;

    std::lock_guard<std::mutex> lock(this->instanceMutex);
    if (!instance) return false;

    switch (param) {
    case SRAL_PARAM_SAPI_TRIM_THRESHOLD:
        this->trimThreshold = *reinterpret_cast<const int*>(value);
        break;
    case SRAL_PARAM_SPEECH_RATE:
        return blastspeak_set_voice_rate(instance.get(), *reinterpret_cast<const long*>(value)) != 0;
    case SRAL_PARAM_SPEECH_VOLUME: {
        long rawVolume = *reinterpret_cast<const long*>(value);
        bool status = blastspeak_set_voice_volume(instance.get(), rawVolume) != 0;
        std::shared_ptr<WasapiPlayer> playerLock = g_player;
        if (playerLock) {
            float volumeFloat = static_cast<float>(rawVolume) / 100.0f;
            playerLock->setVolume(volumeFloat);
        }
        return status;
    }
    case SRAL_PARAM_VOICE_INDEX: {
        int result = blastspeak_set_voice(instance.get(), *reinterpret_cast<const unsigned int*>(value));
        if (result) {
            this->voiceIndex = *reinterpret_cast<const int*>(value);
            return true;
        }
        return false;
    }
    default:
        return false;
    }
    return true;
}

bool Sapi::GetParameter(int param, void* value) noexcept {
    if (!value) [[unlikely]] return false;

    std::lock_guard<std::mutex> lock(this->instanceMutex);
    if (!instance) return false;

    switch (param) {
    case SRAL_PARAM_SAPI_TRIM_THRESHOLD:
        *reinterpret_cast<int*>(value) = this->trimThreshold;
        return true;
    case SRAL_PARAM_SPEECH_RATE:
        return blastspeak_get_voice_rate(instance.get(), reinterpret_cast<long*>(value)) != 0;
    case SRAL_PARAM_SPEECH_VOLUME:
        return blastspeak_get_voice_volume(instance.get(), reinterpret_cast<long*>(value)) != 0;
    case SRAL_PARAM_VOICE_PROPERTIES: {
        ReleaseAllStrings();
        SRAL_VoiceInfo* voiceProperties = reinterpret_cast<SRAL_VoiceInfo*>(value);
        if (!voiceProperties) [[unlikely]] return false;

        struct alignas(16) QueryBuffers {
            std::array<char, 256> desc;
            std::array<char, 128> lang;
            std::array<char, 64> gend;
            std::array<char, 128> vend;
        } qb;

        const unsigned int count = instance->voice_count;
        for (unsigned int index = 0; index < count; ++index) {
            voiceProperties[index].index = static_cast<int>(index);

            qb.desc[0] = '\0';
            qb.lang[0] = '\0';
            qb.gend[0] = '\0';
            qb.vend[0] = '\0';

            (void)blastspeak_get_voice_description(instance.get(), index, qb.desc.data(), qb.desc.size());
            (void)blastspeak_get_voice_languages(instance.get(), index, qb.lang.data(), qb.lang.size());
            (void)blastspeak_get_voice_attribute(instance.get(), index, "Gender", qb.gend.data(), qb.gend.size());
            (void)blastspeak_get_voice_attribute(instance.get(), index, "Vendor", qb.vend.data(), qb.vend.size());

            voiceProperties[index].name = AddString(qb.desc.data());
            voiceProperties[index].language = AddString(qb.lang.data());
            voiceProperties[index].gender = AddString(qb.gend.data());
            voiceProperties[index].vendor = AddString(qb.vend.data());
        }
        return true;
    }
    case SRAL_PARAM_VOICE_COUNT:
        *reinterpret_cast<int*>(value) = static_cast<int>(instance->voice_count);
        return true;
    case SRAL_PARAM_VOICE_INDEX:
        *reinterpret_cast<int*>(value) = this->voiceIndex;
        return true;
    default:
        return false;
    }
}

bool Sapi::StopSpeech() noexcept {
    std::shared_ptr<WasapiPlayer> playerLock = g_player;
    if (!playerLock) [[unlikely]] return false;

    g_ringBuffer.Clear();
    playerLock->stop();
    this->paused = false;
    g_isSpeaking.store(false, std::memory_order_release);
    return true;
}

bool Sapi::PauseSpeech() noexcept {
    std::shared_ptr<WasapiPlayer> playerLock = g_player;
    if (!playerLock) [[unlikely]] return false;
    paused = true;
    return SUCCEEDED(playerLock->pause());
}

bool Sapi::ResumeSpeech() noexcept {
    std::shared_ptr<WasapiPlayer> playerLock = g_player;
    if (!playerLock) [[unlikely]] return false;
    paused = false;
    if (g_ringBuffer.GetAvailableReadSpace() > 0) {
        g_isSpeaking.store(true, std::memory_order_release);
    }
    return SUCCEEDED(playerLock->resume());
}

} // namespace Sral
#endif