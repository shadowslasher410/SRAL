#pragma once

#ifndef WASAPI_H_
#define WASAPI_H_

#if defined(_WIN32)
    #if defined(SRAL_STATIC)
        #define WASAPI_API
    #elif defined(WASAPI_EXPORTS)
        #define WASAPI_API __declspec(dllexport)
    #else
        #define WASAPI_API __declspec(dllimport)
    #endif
#else
    #define WASAPI_API
#endif

#include <algorithm>
#include <audioclient.h>
#include <audiopolicy.h>
#include <comdef.h>
#include <mmdeviceapi.h>
#include <string>
#include <string_view>
#include <vector>
#include <windows.h>
#include <utility>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <memory>

_COM_SMARTPTR_TYPEDEF(IMMDevice, __uuidof(IMMDevice));
_COM_SMARTPTR_TYPEDEF(IMMDeviceCollection, __uuidof(IMMDeviceCollection));
_COM_SMARTPTR_TYPEDEF(IMMDeviceEnumerator, __uuidof(IMMDeviceEnumerator));
_COM_SMARTPTR_TYPEDEF(IAudioClient, __uuidof(IAudioClient));
_COM_SMARTPTR_TYPEDEF(IAudioRenderClient, __uuidof(IAudioRenderClient));
_COM_SMARTPTR_TYPEDEF(IAudioClock, __uuidof(IAudioClock));
_COM_SMARTPTR_TYPEDEF(IAudioStreamVolume, __uuidof(IAudioStreamVolume));
_COM_SMARTPTR_TYPEDEF(ISimpleAudioVolume, __uuidof(ISimpleAudioVolume));
_COM_SMARTPTR_TYPEDEF(IPropertyStore, __uuidof(IPropertyStore));

class AutoHandle final {
public:
	constexpr AutoHandle() noexcept : handle(nullptr) {}
	
	explicit AutoHandle(HANDLE h) noexcept 
		: handle((h == INVALID_HANDLE_VALUE || h == nullptr) ? nullptr : h) {}

	~AutoHandle() noexcept {
		reset();
	}

	AutoHandle(const AutoHandle&) = delete;
	AutoHandle& operator=(const AutoHandle&) = delete;

	AutoHandle(AutoHandle&& other) noexcept : handle(other.handle) {
		other.handle = nullptr;
	}

	AutoHandle& operator=(AutoHandle&& other) noexcept {
		if (this != &other) [[likely]] {
			reset();
			handle = other.handle;
			other.handle = nullptr;
		}
		return *this;
	}

	AutoHandle& operator=(HANDLE newHandle) noexcept {
		if (handle != newHandle) [[likely]] {
			reset();
			if (newHandle != INVALID_HANDLE_VALUE && newHandle != nullptr) {
				handle = newHandle;
			}
		}
		return *this;
	}

	[[nodiscard]] operator HANDLE() const noexcept {
		return handle;
	}

	[[nodiscard]] HANDLE get() const noexcept {
		return handle;
	}

	[[nodiscard]] bool isValid() const noexcept {
		return handle != nullptr;
	}

	void reset() noexcept {
		if (handle) {
			(void)::CloseHandle(handle);
			handle = nullptr;
		}
	}

private:
	HANDLE handle = nullptr;
};

class NotificationClient final : public IMMNotificationClient {
public:
	NotificationClient() noexcept : refCount(1), defaultDeviceChangeCount(0), deviceStateChangeCount(0) {}

	ULONG STDMETHODCALLTYPE AddRef() override {
		return static_cast<ULONG>(refCount.fetch_add(1, std::memory_order_relaxed)) + 1;

	}

	ULONG STDMETHODCALLTYPE Release() override {
		const long result = refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
		if (result == 0) {
			delete this;
			return 0;
		}
		return static_cast<ULONG>(result);
	}

	STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) final {
		if (!ppvObject) [[unlikely]] {
			return E_POINTER;
		}

		if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
			AddRef();
			*ppvObject = static_cast<void*>(this);
			return S_OK;
		}
		
		*ppvObject = nullptr;
		return E_NOINTERFACE;
	}

	STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR defaultDeviceId) final {
		(void)defaultDeviceId;
		if (flow == eRender && role == eConsole) {
			defaultDeviceChangeCount.fetch_add(1, std::memory_order_release);
		}
		return S_OK;
	}

	STDMETHODIMP OnDeviceAdded(LPCWSTR deviceId) final {
		(void)deviceId;
		return S_OK;
	}

	STDMETHODIMP OnDeviceRemoved(LPCWSTR deviceId) final {
		(void)deviceId;
		return S_OK;
	}

	STDMETHODIMP OnDeviceStateChanged(LPCWSTR deviceId, DWORD newState) final {
		(void)deviceId;
		(void)newState;
		deviceStateChangeCount.fetch_add(1, std::memory_order_release);
		return S_OK;
	}

	STDMETHODIMP OnPropertyValueChanged(LPCWSTR deviceId, PROPERTYKEY key) final {
		(void)deviceId;
		(void)key;
		return S_OK;
	}

	[[nodiscard]] unsigned int getDefaultDeviceChangeCount() const noexcept {
		return defaultDeviceChangeCount.load(std::memory_order_acquire);
	}

	[[nodiscard]] unsigned int getDeviceStateChangeCount() const noexcept {
		return deviceStateChangeCount.load(std::memory_order_acquire);
	}

private:
	~NotificationClient() = default;

	NotificationClient(const NotificationClient&) = delete;
	NotificationClient& operator=(const NotificationClient&) = delete;

	std::atomic<long> refCount;
	std::atomic<unsigned int> defaultDeviceChangeCount;
	std::atomic<unsigned int> deviceStateChangeCount;
};

_COM_SMARTPTR_TYPEDEF(IMMNotificationClient, __uuidof(IMMNotificationClient));

class WASAPI_API WasapiPlayer final {
public:
	using ChunkCompletedCallback = void(*)(WasapiPlayer* player, unsigned int id);

	WasapiPlayer(std::wstring_view targetDeviceName, const WAVEFORMATEX& audioFormat, ChunkCompletedCallback endChunkCallback);
	~WasapiPlayer();
	WasapiPlayer(const WasapiPlayer&) = delete;
	WasapiPlayer& operator=(const WasapiPlayer&) = delete;
	WasapiPlayer(WasapiPlayer&&) = delete;
	WasapiPlayer& operator=(WasapiPlayer&&) = delete;

	[[nodiscard]] HRESULT open(bool force = false);
	[[nodiscard]] HRESULT feed(const unsigned char* data, unsigned int size, unsigned int* id);
	[[nodiscard]] HRESULT stop();
	[[nodiscard]] HRESULT sync();
	[[nodiscard]] HRESULT idle();
	[[nodiscard]] HRESULT pause();
	[[nodiscard]] HRESULT resume();
	[[nodiscard]] HRESULT setVolume(float volume); // For basic UI mixer utility if exposed upstream
	[[nodiscard]] HRESULT setChannelVolume(unsigned int channel, float level);

	WAVEFORMATEX format;

private:
	void processAudioLoop();
	HRESULT writeFramesToWasapi(const unsigned char* data, UINT32 totalFrames, unsigned int chunkId);

	void maybeFireCallback();
	void maybeFireCallbackInternal();
	void completeStop();

	[[nodiscard]] inline UINT64 framesToMs(UINT64 frames) const noexcept {
		if (format.nSamplesPerSec == 0) [[unlikely]] return 0;
		return (frames * 1000) / format.nSamplesPerSec;
	}

	[[nodiscard]] UINT64 getPlayPos();
	[[nodiscard]] UINT64 getPlayPosInternal();
	void waitUntilNeeded(UINT64 maxWait = INFINITE);
	[[nodiscard]] HRESULT getPreferredDevice(IMMDevicePtr& preferredDevice);
	[[nodiscard]] bool didPreferredDeviceBecomeAvailable();
	[[nodiscard]] size_t getAvailableWriteSpace() const;
	[[nodiscard]] size_t getAvailableReadSpace() const;

	void writeToRingBuffer(size_t& tail, const unsigned char* src, size_t len);
	void readFromRingBuffer(size_t& head, unsigned char* dest, size_t len);

	enum class PlayState {
		stopped,
		playing,
		paused,
		stopping,
	};

	size_t                           ringBufferCapacity = 0;
	std::unique_ptr<unsigned char[]> ringBuffer;
	std::atomic<size_t>              rbHead{ 0 };
	std::atomic<size_t>              rbTail{ 0 };
	std::thread               workerThread;
	std::mutex                queueMutex;
	std::condition_variable   queueCV;
	std::atomic<bool>         isRunning{ false };
	IAudioClientPtr       client{ nullptr };
	IAudioRenderClientPtr render{ nullptr };
	IAudioClockPtr        clock{ nullptr };
	UINT32                bufferFrames = 0;
	std::wstring          deviceName;
	ChunkCompletedCallback callback = nullptr;
	PlayState             playState = PlayState::stopped;
	std::vector<std::pair<unsigned int, UINT64>> feedEnds;
	UINT64                clockFreq = 0;
	UINT64                baseDevicePos = 0;
	UINT64                sentFrames = 0; 
	unsigned int          nextFeedId = 0;
	unsigned int          defaultDeviceChangeCount = 0;
	unsigned int          deviceStateChangeCount = 0;
	bool                  isUsingPreferredDevice = false;
	AutoHandle            audioEvent; 
};

#endif
