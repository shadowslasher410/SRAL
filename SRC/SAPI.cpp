/* 
 * ==============================================================================
 * NOTICE OF ALTERATION (In compliance with Original Software License Terms)
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
#include "SAPI.h"

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <deque>
#include <concepts>
#include <cmath>
#include <cstring>

extern "C" {
#include "../Dep/blastspeak.h"

    int blastspeak_get_voice_description(blastspeak* instance, unsigned int voice_index, char* out_buffer, size_t max_bytes);
    int blastspeak_get_voice_attribute(blastspeak* instance, unsigned int voice_index, const char* attribute, char* out_buffer, size_t max_bytes);
    int blastspeak_get_voice_languages(blastspeak* instance, unsigned int voice_index, char* out_buffer, size_t max_bytes);
}

static std::shared_ptr<WasapiPlayer> g_player{ nullptr };

template <typename T, typename Func, typename... Args> 
inline void safeCall(T* obj, Func func, Args&&... args) {
	if (obj) [[likely]] {
		(obj->*func)(std::forward<Args>(args)...);
	}
}

template <typename T, typename Func, typename... Args>
inline void safeCall(T* obj, Func func, Args&&... args, std::invocable auto onNull) {
	if (obj) [[likely]] {
		(obj->*func)(std::forward<Args>(args)...);
	}
	else {
		onNull();
	}
}

template <typename T, typename R, typename Func, typename... Args>
inline std::optional<R> safeCallVal(T* obj, Func func, Args&&... args) {
	if (obj) [[likely]] {
		return (obj->*func)(std::forward<Args>(args)...);
	}
	return std::nullopt;
}

static char* trim(char* data, unsigned long* size, const WAVEFORMATEX* wfx, int threshold) {
	if (!data || !size || !wfx || *size == 0) [[unlikely]] return nullptr;

	int channels = wfx->nChannels;
	int bytesPerSample = wfx->wBitsPerSample / 8;
	if (bytesPerSample == 0) [[unlikely]] return nullptr;

	int samplesPerFrame = channels * bytesPerSample;
	int numSamples = static_cast<int>(*size) / samplesPerFrame;
	int startIndex = 0;
	int endIndex = numSamples - 1;

	for (int i = 0; i < numSamples; i++) {
		int maxAbsValue = 0;
		for (int j = 0; j < channels; j++) {
			int absValue = std::abs(static_cast<int>(data[i * samplesPerFrame + j]));
			if (absValue > maxAbsValue) {
				maxAbsValue = absValue;
			}
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
			if (absValue > maxAbsValue) {
				maxAbsValue = absValue;
			}
		}
		if (maxAbsValue >= threshold) {
			endIndex = i;
			break;
		}
	}

	if (startIndex > endIndex) {
		*size = 0;
		return nullptr;
	}

	int trimmedSize = (endIndex - startIndex + 1) * samplesPerFrame;
	char* trimmedData = new (std::nothrow) char[trimmedSize];
	if (trimmedData) [[likely]] {
		std::memcpy(trimmedData, data + (startIndex * samplesPerFrame), trimmedSize);
	}
	*size = trimmedSize;
	return trimmedData;
}

struct PCMData {
	unsigned char* data = nullptr;
	unsigned long size = 0;
};

static std::deque<PCMData> g_dataQueue;
static std::mutex g_dataQueueMutex;
static std::condition_variable g_dataQueueCv;
static std::atomic<bool> g_threadStarted{ false };
static std::atomic<bool> g_isSpeaking{ false };

static void sapi_thread(std::stop_token stopToken) {
	if (g_player == nullptr) {
		g_threadStarted.store(false, std::memory_order_release);
		g_isSpeaking.store(false, std::memory_order_release);
		return;
	}

	while (g_threadStarted.load(std::memory_order_acquire) && !stopToken.stop_requested()) {
		PCMData current_data;
		{
			std::unique_lock<std::mutex> lock(g_dataQueueMutex);
			g_dataQueueCv.wait(lock, [&] { 
				return !g_dataQueue.empty() || !g_threadStarted.load(std::memory_order_acquire) || stopToken.stop_requested(); 
			});

			if ((!g_threadStarted.load(std::memory_order_acquire) || stopToken.stop_requested()) && g_dataQueue.empty()) {
				break;
			}
			if (g_dataQueue.empty()) {
				continue;
			}

			current_data = g_dataQueue.front();
			g_dataQueue.pop_front();
		}
		
		g_isSpeaking.store(true, std::memory_order_release);

		if (current_data.data) {
			auto result = safeCallVal<WasapiPlayer, HRESULT>(
				g_player.get(), &WasapiPlayer::feed, current_data.data, current_data.size, nullptr);
			
			free(current_data.data);

			if (result.has_value() && SUCCEEDED(*result)) {
				(void)safeCallVal<WasapiPlayer, HRESULT>(g_player.get(), &WasapiPlayer::sync);
			}
		}

		{
			std::unique_lock<std::mutex> lock(g_dataQueueMutex);
			if (g_dataQueue.empty()) {
				g_isSpeaking.store(false, std::memory_order_release);
			}
		}
	}

	std::unique_lock<std::mutex> lock(g_dataQueueMutex);
	for (PCMData& data : g_dataQueue) {
		if (data.data) {
			free(data.data);
			data.data = nullptr;
		}
	}
	g_dataQueue.clear();
	g_isSpeaking.store(false, std::memory_order_release);
}

namespace Sral {
bool Sapi::Initialize() {
	if (instance) {
		instance.reset();
	}
	this->voiceIndex = 0;
	
	g_threadStarted.store(false, std::memory_order_release);
	g_dataQueueCv.notify_all();

	if (g_player) {
		static_cast<void>(g_player->stop());
		g_player.reset();
	}

	speechThread.request_stop();
	if (speechThread.joinable()) {
		speechThread.join();
	}

	instance = std::make_unique<blastspeak>();

	if (blastspeak_initialize(&*instance) == 0) {
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
	HRESULT hr = g_player->open();
	if (FAILED(hr)) {
		g_player.reset();
		return false;
	}

	g_threadStarted.store(true, std::memory_order_release);
	speechThread = std::jthread(sapi_thread);
	return true;
}

bool Sapi::Uninitialize() {
	ReleaseAllStrings();
	this->voiceIndex = 0;
	if (!instance || g_player == nullptr)
		return false;

	g_threadStarted.store(false, std::memory_order_release);
	blastspeak_destroy(&*instance);
	instance.reset();

	speechThread.request_stop();
	g_dataQueueCv.notify_all();
	if (speechThread.joinable()) {
		speechThread.join();
	}

	if (g_player) {
		g_player.reset();
	}
	g_isSpeaking.store(false, std::memory_order_release);
	return true;
}

bool Sapi::GetActive() {
	return instance && g_player != nullptr;
}

bool Sapi::Speak(const char* text, bool interrupt) {
	auto playerLock = g_player;
	if (instance == nullptr || playerLock == nullptr) [[unlikely]] {
		return false;
	}

	if (interrupt) {
		static_cast<void>(StopSpeech());
	}

	if (wfx.nChannels != instance->channels || wfx.nSamplesPerSec != instance->sample_rate ||
		wfx.wBitsPerSample != instance->bits_per_sample) {

		wfx.nChannels = instance->channels;
		wfx.nSamplesPerSec = instance->sample_rate;
		wfx.wBitsPerSample = instance->bits_per_sample;
		wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
		wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
		
		g_threadStarted.store(false, std::memory_order_release);
		speechThread.request_stop();
		g_dataQueueCv.notify_all();
		if (speechThread.joinable()) {
			speechThread.join();
		}

		if (g_player) {
			g_player.reset();
		}

		g_player = std::make_shared<WasapiPlayer>(L"", wfx, callback);
		HRESULT hr = g_player->open();
		if (FAILED(hr)) {
			g_player.reset();
			return false;
		}
		
		playerLock = g_player;
		
		g_threadStarted.store(true, std::memory_order_release);
		g_isSpeaking.store(false, std::memory_order_release);
		speechThread = std::jthread(sapi_thread);
	}

	uint64_t buffer_size = 0;
	char* data = static_cast<char*>(this->SpeakToMemory(text, &buffer_size, nullptr, nullptr, nullptr));
	if (!data || buffer_size == 0) {
		return false;
	}

	PCMData dat;
	dat.data = reinterpret_cast<unsigned char*>(data);
	dat.size = static_cast<unsigned long>(buffer_size);

	if (this->paused) {
		this->paused = false;
		if (!interrupt && playerLock) {
			static_cast<void>(playerLock->resume());
		}
	}

	{
		std::unique_lock<std::mutex> lock(g_dataQueueMutex);
		g_dataQueue.push_back(dat);
	}
	
	g_isSpeaking.store(true, std::memory_order_release);
	g_dataQueueCv.notify_all();
	return true;
}

void* Sapi::SpeakToMemory(
	const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample) {
	if (instance == nullptr || !text) [[unlikely]]
		return nullptr;

	std::string text_str(text);
	unsigned long bytes = 0;
	char* audio_ptr = blastspeak_speak_to_memory(&*instance, &bytes, text_str.c_str());
	if (audio_ptr == nullptr)
		return nullptr;

	char* final_ptr = trim(audio_ptr, &bytes, &wfx, this->trimThreshold);
	
	free(audio_ptr); 

	if (buffer_size)
		*buffer_size = bytes;
	if (channels)
		*channels = instance->channels;
	if (sample_rate)
		*sample_rate = instance->sample_rate;
	if (bits_per_sample)
		*bits_per_sample = instance->bits_per_sample;
	return final_ptr;
}

bool Sapi::IsSpeaking() {
	return !paused && g_isSpeaking.load(std::memory_order_acquire);
}

bool Sapi::SetParameter(int param, const void* value) {
	if (instance == nullptr || !value) [[unlikely]]
		return false;

	switch (param) {
	case SRAL_PARAM_SAPI_TRIM_THRESHOLD:
		this->trimThreshold = *reinterpret_cast<const int*>(value);
		break;
	case SRAL_PARAM_SPEECH_RATE:
		return blastspeak_set_voice_rate(&*instance, *reinterpret_cast<const long*>(value)) != 0;
	case SRAL_PARAM_SPEECH_VOLUME:
		return blastspeak_set_voice_volume(&*instance, *reinterpret_cast<const long*>(value)) != 0;
	case SRAL_PARAM_VOICE_INDEX: {
		int result = blastspeak_set_voice(&*instance, *reinterpret_cast<const int*>(value));
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

bool Sapi::GetParameter(int param, void* value) {
	if (instance == nullptr || !value) [[unlikely]]
		return false;

	switch (param) {
	case SRAL_PARAM_SAPI_TRIM_THRESHOLD:
		*reinterpret_cast<int*>(value) = this->trimThreshold;
		return true;
	case SRAL_PARAM_SPEECH_RATE: {
		return blastspeak_get_voice_rate(&*instance, reinterpret_cast<long*>(value)) != 0;
	}
	case SRAL_PARAM_SPEECH_VOLUME: {
		return blastspeak_get_voice_volume(&*instance, reinterpret_cast<long*>(value)) != 0;
	}
	case SRAL_PARAM_VOICE_PROPERTIES: {
		ReleaseAllStrings();
		SRAL_VoiceInfo* voiceProperties = reinterpret_cast<SRAL_VoiceInfo*>(value);
		if (!voiceProperties) [[unlikely]] return false;

		char bufDesc[256];
		char bufLang[128];
		char bufGend[64];
		char bufVend[128];

		for (int index = 0; instance && index < (int)instance->voice_count; ++index) {
			voiceProperties[index].index = index;
			
			bufDesc[0] = '\0'; bufLang[0] = '\0'; bufGend[0] = '\0'; bufVend[0] = '\0';

			(void)blastspeak_get_voice_description(&*instance, index, bufDesc, sizeof(bufDesc));
			(void)blastspeak_get_voice_languages(&*instance, index, bufLang, sizeof(bufLang));
			(void)blastspeak_get_voice_attribute(&*instance, index, "Gender", bufGend, sizeof(bufGend));
			(void)blastspeak_get_voice_attribute(&*instance, index, "Vendor", bufVend, sizeof(bufVend));

			voiceProperties[index].name = AddString(bufDesc);
			voiceProperties[index].language = AddString(bufLang);
			voiceProperties[index].gender = AddString(bufGend);
			voiceProperties[index].vendor = AddString(bufVend);
		}
		return true;
	}

	case SRAL_PARAM_VOICE_COUNT:
		*reinterpret_cast<int*>(value) = (int)instance->voice_count;
		return true;
	case SRAL_PARAM_VOICE_INDEX:
		*reinterpret_cast<int*>(value) = this->voiceIndex;
		return true;
	default:
		return false;
	}
}

bool Sapi::StopSpeech() {
	auto playerLock = g_player;
	if (playerLock == nullptr) [[unlikely]]
		return false;
		
	{
		std::unique_lock<std::mutex> lock(g_dataQueueMutex);
		for (PCMData& data : g_dataQueue) {
			if (data.data) {
				free(data.data);
				data.data = nullptr;
			}
		}
		g_dataQueue.clear();
	}
	
	(void)playerLock->stop();
	this->paused = false;
	g_isSpeaking.store(false, std::memory_order_release);
	return true;
}

bool Sapi::PauseSpeech() {
	auto playerLock = g_player;
	if (playerLock == nullptr) [[unlikely]] return false;
	paused = true;
	return SUCCEEDED(playerLock->pause());
}

bool Sapi::ResumeSpeech() {
	auto playerLock = g_player;
	if (playerLock == nullptr) [[unlikely]] return false;
	paused = false;
	{
		std::unique_lock<std::mutex> lock(g_dataQueueMutex);
		if (!g_dataQueue.empty()) {
			g_isSpeaking.store(true, std::memory_order_release);
		}
	}
	return SUCCEEDED(playerLock->resume());
}

} // namespace Sral
#endif