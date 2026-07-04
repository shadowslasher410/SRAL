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
#include <thread>

#ifndef PKEY_Device_FriendlyName
#undef DEFINE_PROPERTYKEY
#define DEFINE_PROPERTYKEY(id, a, b, c, d, e, f, g, h, i, j, k, l) \
	const PROPERTYKEY id = { { a, b, c, { d, e, f, g, h, i, j, k, } }, l };
DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);
#endif

constexpr REFERENCE_TIME REFTIMES_PER_MILLISEC = 10000;
constexpr REFERENCE_TIME BUFFER_SIZE = 400 * REFTIMES_PER_MILLISEC;

static IMMNotificationClientPtr g_notificationClient{ nullptr };
static std::atomic<bool> g_wakeSignal{ false };

WasapiPlayer::WasapiPlayer(std::wstring_view targetDeviceName, const WAVEFORMATEX& audioFormat, ChunkCompletedCallback endChunkCallback)
	: format(audioFormat), deviceName(targetDeviceName), callback(endChunkCallback) {
	
	IMMDeviceEnumeratorPtr enumerator;
	HRESULT hr = enumerator.CreateInstance(__uuidof(MMDeviceEnumerator));
	if (SUCCEEDED(hr)) {
		if (!g_notificationClient) {
			g_notificationClient = IMMNotificationClientPtr(new (std::nothrow) NotificationClient());
		}
		if (g_notificationClient) [[likely]] {
			(void)enumerator->RegisterEndpointNotificationCallback(g_notificationClient);
		}
	}
}

HRESULT WasapiPlayer::open(bool force) {
	if (client && !force) {
		return S_OK;
	}
	
	if (!g_notificationClient) [[unlikely]] {
		return E_UNEXPECTED;
	}

	render = nullptr;
	clock = nullptr;
	client = nullptr;

	NotificationClient* const pClientImpl = static_cast<NotificationClient*>(g_notificationClient.GetInterfacePtr());
	defaultDeviceChangeCount = pClientImpl->getDefaultDeviceChangeCount();
	deviceStateChangeCount = pClientImpl->getDeviceStateChangeCount();
	
	IMMDeviceEnumeratorPtr enumerator;
	HRESULT hr = enumerator.CreateInstance(__uuidof(MMDeviceEnumerator));
	if (FAILED(hr)) {
		return hr;
	}
	
	IMMDevicePtr device;
	isUsingPreferredDevice = false;
	
	if (deviceName.empty()) {
		hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
	}
	else {
		hr = getPreferredDevice(device);
		if (SUCCEEDED(hr)) {
			isUsingPreferredDevice = true;
		}
		else {
			hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
		}
	}
	
	if (FAILED(hr)) {
		return hr;
	}
	
	hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client));
	if (FAILED(hr)) {
		return hr;
	}
	
	hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
		BUFFER_SIZE, 0, &format, nullptr);
	if (FAILED(hr)) {
		return hr;
	}
	
	hr = client->GetBufferSize(&bufferFrames);
	if (FAILED(hr)) {
		return hr;
	}
	
	hr = client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&render));
	if (FAILED(hr)) {
		return hr;
	}
	
	hr = client->GetService(__uuidof(IAudioClock), reinterpret_cast<void**>(&clock));
	if (FAILED(hr)) {
		return hr;
	}
	
	hr = clock->GetFrequency(&clockFreq);
	if (FAILED(hr)) {
		return hr;
	}
	
	playState = PlayState::stopped;
	g_wakeSignal.store(false, std::memory_order_release);
	return S_OK;
}

HRESULT WasapiPlayer::feed(const unsigned char* data, unsigned int size, unsigned int* id) {
	if (playState == PlayState::stopping) {
		completeStop();
	}
	
	if (format.nBlockAlign == 0) [[unlikely]] {
		return E_INVALIDARG;
	}
	
	UINT32 remainingFrames = size / format.nBlockAlign;
	HRESULT hr = S_OK;

	auto reopenUsingNewDev = [&] {
		hr = open(true);
		if (FAILED(hr)) {
			return false;
		}
		for (auto& [itemId, itemEnd] : feedEnds) {
			if (callback) {
				callback(this, itemId);
			}
		}
		feedEnds.clear();
		sentFrames = 0;
		return true;
	};

	while (remainingFrames > 0) {
		UINT32 paddingFrames = 0;

		auto getPaddingHandlingStopOrDevChange = [&] {
			if (playState == PlayState::stopping) {
				completeStop();
				hr = S_OK;
				return false;
			}
			
			NotificationClient* const pClientImpl = static_cast<NotificationClient*>(g_notificationClient.GetInterfacePtr());
			if (didPreferredDeviceBecomeAvailable() ||
				(!isUsingPreferredDevice && defaultDeviceChangeCount != pClientImpl->getDefaultDeviceChangeCount())) {
				if (!reopenUsingNewDev()) {
					return false;
				}
			}
			hr = client->GetCurrentPadding(&paddingFrames);
			if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == AUDCLNT_E_NOT_INITIALIZED) {
				if (!reopenUsingNewDev()) {
					return false;
				}
				hr = client->GetCurrentPadding(&paddingFrames);
			}
			return SUCCEEDED(hr);
		};

		if (!getPaddingHandlingStopOrDevChange()) {
			return hr;
		}
		
		if (paddingFrames > bufferFrames / 2) {
			waitUntilNeeded(framesToMs(paddingFrames - bufferFrames / 2));
			if (!getPaddingHandlingStopOrDevChange()) {
				return hr;
			}
		}
		
		const UINT32 sendFrames = std::min(remainingFrames, bufferFrames - paddingFrames);
		if (sendFrames == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		
		const UINT32 sendBytes = sendFrames * format.nBlockAlign;
		BYTE* buffer = nullptr;
		hr = render->GetBuffer(sendFrames, &buffer);
		if (FAILED(hr)) {
			return hr;
		}
		
		std::memcpy(buffer, data, sendBytes);
		hr = render->ReleaseBuffer(sendFrames, 0);
		if (FAILED(hr)) {
			return hr;
		}
		
		if (playState == PlayState::stopped) {
			hr = client->Start();
			if (FAILED(hr)) {
				return hr;
			}
			if (playState == PlayState::stopping) {
				completeStop();
				return S_OK;
			}
			playState = PlayState::playing;
		}
		
		maybeFireCallback();
		data += sendBytes;
		remainingFrames -= sendFrames;
		sentFrames += sendFrames;
	}

	if (playState == PlayState::playing) {
		maybeFireCallback();
	}
	
	if (id) {
		*id = nextFeedId++;
		feedEnds.push_back({ *id, framesToMs(sentFrames) });
	}
	return S_OK;
}

void WasapiPlayer::maybeFireCallback() {
	if (!callback) return;
	
	const UINT64 playPos = getPlayPos();
	std::erase_if(feedEnds, [&](const auto& val) noexcept -> bool {
		const auto [id, end] = val;
		if (playPos >= end) {
			callback(this, id);
			return true;
		}
		return false;
	});
}

UINT64 WasapiPlayer::getPlayPos() {
	if (!clock || clockFreq == 0) [[unlikely]] {
		return framesToMs(sentFrames);
	}
	
	UINT64 pos = 0;
	HRESULT hr = clock->GetPosition(&pos, nullptr);
	if (FAILED(hr)) {
		return framesToMs(sentFrames);
	}
	return (pos * 1000) / clockFreq;
}

void WasapiPlayer::waitUntilNeeded(UINT64 maxWait) {
	if (!feedEnds.empty()) {
		const UINT64 feedEnd = feedEnds.front().second;
		const UINT64 playPos = getPlayPos();
		if (feedEnd > playPos) {
			const UINT64 nextCallbackTime = feedEnd - playPos;
			if (nextCallbackTime < maxWait) {
				maxWait = nextCallbackTime;
			}
		} else {
			maxWait = 0;
		}
	}
	
	if (maxWait > 0) {
		auto startTime = std::chrono::steady_clock::now();
		while (!g_wakeSignal.load(std::memory_order_acquire)) {
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
			if (static_cast<UINT64>(elapsed) >= maxWait) {
				break;
			}
			g_wakeSignal.wait(false, std::memory_order_relaxed);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

HRESULT WasapiPlayer::getPreferredDevice(IMMDevicePtr& preferredDevice) {
	IMMDeviceEnumeratorPtr enumerator;
	HRESULT hr = enumerator.CreateInstance(__uuidof(MMDeviceEnumerator));
	if (FAILED(hr)) {
		return hr;
	}
	
	IMMDeviceCollectionPtr devices;
	hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices);
	if (FAILED(hr)) {
		return hr;
	}
	
	UINT count = 0;
	hr = devices->GetCount(&count);
	if (FAILED(hr)) {
		return hr;
	}
	
	constexpr size_t MAX_CHARS = MAXPNAMELEN - 1;
	for (UINT d = 0; d < count; ++d) {
		IMMDevicePtr device;
		hr = devices->Item(d, &device);
		if (FAILED(hr)) {
			continue;
		}
		
		IPropertyStorePtr props;
		hr = device->OpenPropertyStore(STGM_READ, &props);
		if (FAILED(hr)) {
			continue;
		}
		
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
	playState = PlayState::stopping;
	if (!client) return S_OK;
	
	HRESULT hr = client->Stop();
	if (hr != AUDCLNT_E_DEVICE_INVALIDATED && hr != AUDCLNT_E_NOT_INITIALIZED) {
		if (FAILED(hr)) {
			return hr;
		}
		hr = client->Reset();
		if (FAILED(hr)) {
			return hr;
		}
	}
	
	g_wakeSignal.store(true, std::memory_order_release);
	g_wakeSignal.notify_all();
	return S_OK;
}

void WasapiPlayer::completeStop() {
	nextFeedId = 0;
	sentFrames = 0;
	feedEnds.clear();
	playState = PlayState::stopped;
	g_wakeSignal.store(false, std::memory_order_release);
}

HRESULT WasapiPlayer::sync() {
	const UINT64 sentMs = framesToMs(sentFrames);
	for (UINT64 playPos = getPlayPos(); playPos < sentMs; playPos = getPlayPos()) {
		if (playState != PlayState::playing) {
			return S_OK;
		}
		maybeFireCallback();
		waitUntilNeeded(sentMs - playPos);
	}
	if (playState == PlayState::playing) {
		maybeFireCallback();
	}
	return S_OK;
}

HRESULT WasapiPlayer::idle() {
	HRESULT hr = sync();
	if (FAILED(hr)) {
		return hr;
	}
	hr = stop();
	if (FAILED(hr)) {
		return hr;
	}
	completeStop();
	return S_OK;
}

HRESULT WasapiPlayer::pause() {
	if (playState != PlayState::playing || !client) {
		return S_OK;
	}
	return client->Stop();
}

HRESULT WasapiPlayer::resume() {
	if (playState != PlayState::playing || !client) {
		return S_OK;
	}
	return client->Start();
}

HRESULT WasapiPlayer::setChannelVolume(unsigned int channel, float level) {
	if (!client) return E_UNEXPECTED;
	
	IAudioStreamVolumePtr volume;
	HRESULT hr = client->GetService(__uuidof(IAudioStreamVolume), reinterpret_cast<void**>(&volume));
	if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
		hr = open(true);
		if (FAILED(hr)) {
			return hr;
		}
		hr = client->GetService(__uuidof(IAudioStreamVolume), reinterpret_cast<void**>(&volume));
	}
	if (FAILED(hr)) {
		return hr;
	}
	return volume->SetChannelVolume(channel, level);
}
