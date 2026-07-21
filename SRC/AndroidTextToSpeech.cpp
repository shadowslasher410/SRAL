#include "AndroidTextToSpeech.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>

#include "AndroidContext.h"

#ifdef __ANDROID__
#include <android/log.h>
#include <jni.h>
#define LOG_TAG "SRAL_AndroidTTS"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <iostream>
#define LOGE(...) std::cerr << "[SRAL Error] " << __VA_ARGS__ << "\n"

#ifndef JNI_VERSION_1_6
#define JNI_VERSION_1_6 0x00010006
#endif
#ifndef JNI_EDETACHED
#define JNI_EDETACHED -2
#endif
#ifndef JNI_OK
#define JNI_OK 0
#endif
#ifndef JNI_TRUE
#define JNI_TRUE 1
#endif
#ifndef JNI_FALSE
#define JNI_FALSE 0
#endif
#endif

namespace Sral {

template <typename T>
concept SralParameterType = std::integral<std::remove_pointer_t<std::decay_t<T>>>;

enum class TaskType : uint8_t { Speak, Stop, SetVolume, SetRate };

struct alignas(64) AsyncTtsTask {
	std::array<char, 512> text{};
	std::atomic<size_t> sequence{0};
	float parameter_value{0.0f};
	TaskType type{TaskType::Speak};
	bool interrupt{false};
};

struct AndroidTextToSpeech::Impl final {
	static constexpr size_t RING_BUFFER_SIZE = 128;
	static constexpr size_t RING_MASK = RING_BUFFER_SIZE - 1;
	std::mutex mutex;
	JavaVM* jvm{nullptr};
	jclass speechClass{nullptr};
	jobject speechObj{nullptr};
	jmethodID constructor{nullptr};
	jmethodID midIsActive{nullptr};
	jmethodID midIsSpeaking{nullptr};
	jmethodID midSpeak{nullptr};
	jmethodID midSilence{nullptr};
	jmethodID midShutdown{nullptr};
	jmethodID midSetVolume{nullptr};
	jmethodID midSetRate{nullptr};
	std::atomic<uint64_t> cachedVolume{100};
	std::atomic<uint64_t> cachedRate{100};
	std::atomic<bool> initialized{false};
	alignas(64) std::array<AsyncTtsTask, RING_BUFFER_SIZE> ringQueue;
	alignas(64) std::atomic<size_t> head{0};
	alignas(64) std::atomic<size_t> tail{0};
	alignas(64) std::atomic<bool> ringBell{false};
	alignas(64) std::atomic<bool> stopRequested{false};
	std::thread workerThread;

	void ClearGlobalReferences(JNIEnv* const local_env) noexcept {
#ifdef __ANDROID__
		if (local_env) {
			if (speechObj) {
				local_env->DeleteGlobalRef(speechObj);
				speechObj = nullptr;
			}
			if (speechClass) {
				local_env->DeleteGlobalRef(speechClass);
				speechClass = nullptr;
			}
		}
#else
		(void)local_env;
#endif
	}

	void BackgroundWorkerLoop() noexcept {
#ifdef __ANDROID__
		if (!jvm)
			return;
		JNIEnv* env = nullptr;
		if (jvm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) {
			LOGE("Background Worker Error: Could not attach thread to JavaVM.");
			return;
		}
#endif
		while (!stopRequested.load(std::memory_order_acquire)) {
			size_t current_tail = tail.load(std::memory_order_acquire);
			AsyncTtsTask& task = ringQueue[current_tail & RING_MASK];

			size_t seq = task.sequence.load(std::memory_order_acquire);
			intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1);

			if (difference != 0) {
				ringBell.store(false, std::memory_order_release);
				seq = task.sequence.load(std::memory_order_acquire);

				if (static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1) != 0) {
					while (
						!ringBell.load(std::memory_order_acquire) && !stopRequested.load(std::memory_order_acquire)) {
						ringBell.wait(false, std::memory_order_acquire);
					}
				}
				else {
					ringBell.store(true, std::memory_order_release);
				}
				if (stopRequested.load(std::memory_order_acquire)) [[unlikely]]
					break;
				continue;
			}

			const TaskType type = task.type;
			const bool interrupt = task.interrupt;
			const float param_val = task.parameter_value;

#ifdef __ANDROID__
			if (speechObj) [[likely]] {
				switch (type) {
				case TaskType::Speak:
					if (midSpeak) {
						jstring jtext = env->NewStringUTF(task.text.data());
						if (jtext) {
							env->CallVoidMethod(speechObj, midSpeak, jtext, static_cast<jboolean>(interrupt));
							if (env->ExceptionCheck())
								env->ExceptionClear();
							env->DeleteLocalRef(jtext);
						}
					}
					break;
				case TaskType::Stop:
					if (midSilence) {
						env->CallVoidMethod(speechObj, midSilence);
						if (env->ExceptionCheck())
							env->ExceptionClear();
					}
					break;
				case TaskType::SetVolume:
					if (midSetVolume) {
						env->CallVoidMethod(speechObj, midSetVolume, static_cast<jfloat>(param_val));
						if (env->ExceptionCheck())
							env->ExceptionClear();
					}
					break;
				case TaskType::SetRate:
					if (midSetRate) {
						env->CallVoidMethod(speechObj, midSetRate, static_cast<jfloat>(param_val));
						if (env->ExceptionCheck())
							env->ExceptionClear();
					}
					break;
				}
			}
#else
			(void)interrupt;
			(void)param_val;
#endif
			tail.store(current_tail + 1, std::memory_order_release);
			task.sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
		}
#ifdef __ANDROID__
		jvm->DetachCurrentThread();
#endif
	}

	bool PushTask(TaskType type, std::string_view text, float param_val, bool interrupt) noexcept {
		if (!initialized.load(std::memory_order_relaxed) || stopRequested.load(std::memory_order_acquire)) {
			return false;
		}

		size_t ticket = head.load(std::memory_order_relaxed);
		AsyncTtsTask* task = nullptr;

		while (true) {
			task = &ringQueue[ticket & RING_MASK];
			size_t seq = task->sequence.load(std::memory_order_acquire);
			intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

			if (difference == 0) {
				if (head.compare_exchange_weak(ticket, ticket + 1, std::memory_order_relaxed)) {
					break;
				}
			}
			else if (difference < 0) {
				return false;
			}
			else {
				ticket = head.load(std::memory_order_relaxed);
			}
		}

		if (!text.empty()) {
			size_t max_copy = (std::min)(text.size(), task->text.size() - 1);
			std::memcpy(task->text.data(), text.data(), max_copy);
			task->text[max_copy] = '\0';
		}
		else {
			task->text[0] = '\0';
		}

		task->type = type;
		task->parameter_value = param_val;
		task->interrupt = interrupt;

		task->sequence.store(ticket + 1, std::memory_order_release);

		if (!ringBell.exchange(true, std::memory_order_release)) {
			ringBell.notify_one();
		}
		return true;
	}
};

AndroidTextToSpeech::AndroidTextToSpeech() : m_impl(std::make_unique<Impl>()) {
	for (size_t i = 0; i < Impl::RING_BUFFER_SIZE; ++i) {
		m_impl->ringQueue[i].sequence.store(i, std::memory_order_relaxed);
	}
}

AndroidTextToSpeech::~AndroidTextToSpeech() {
	static_cast<void>(Uninitialize());
}

bool AndroidTextToSpeech::Initialize() {
	std::lock_guard<std::mutex> lock(m_impl->mutex);
	if (m_impl->initialized.load(std::memory_order_relaxed))
		return true;

	m_impl->jvm = GetAndroidJavaVM();
	if (!m_impl->jvm)
		return false;

	ScopedAttachmentGuard jni(m_impl->jvm);
	JNIEnv* const env = jni.GetEnv();
	if (!env)
		return false;

	ScopedLocalRef activityRef = GetAndroidActivity();
	const jobject activity = activityRef.get();
	if (!activity)
		return false;

	m_impl->ClearGlobalReferences(env);

#ifdef __ANDROID__
	jclass localClass = env->FindClass("org/sral/AndroidTTSHelper");
	if (!localClass || env->ExceptionCheck()) {
		if (localClass)
			env->DeleteLocalRef(localClass);
		env->ExceptionClear();
		return false;
	}

	m_impl->speechClass = static_cast<jclass>(env->NewGlobalRef(localClass));
	env->DeleteLocalRef(localClass);

	m_impl->constructor = env->GetMethodID(m_impl->speechClass, "<init>", "(Landroid/content/Context;)V");
	m_impl->midSpeak = env->GetMethodID(m_impl->speechClass, "speak", "(Ljava/lang/String;Z)V");
	m_impl->midSilence = env->GetMethodID(m_impl->speechClass, "stop", "()V");
	m_impl->midIsActive = env->GetMethodID(m_impl->speechClass, "isActive", "()Z");
	m_impl->midIsSpeaking = env->GetMethodID(m_impl->speechClass, "isSpeaking", "()Z");
	m_impl->midShutdown = env->GetMethodID(m_impl->speechClass, "shutdown", "()V");
	m_impl->midSetVolume = env->GetMethodID(m_impl->speechClass, "setVolume", "(F)V");
	m_impl->midSetRate = env->GetMethodID(m_impl->speechClass, "setSpeechRate", "(F)V");

	if (!m_impl->constructor || !m_impl->midSpeak || !m_impl->midSilence || !m_impl->midIsActive ||
		!m_impl->midIsSpeaking || !m_impl->midShutdown || !m_impl->midSetVolume || !m_impl->midSetRate) {
		m_impl->ClearGlobalReferences(env);
		return false;
	}

	jobject localObj = env->NewObject(m_impl->speechClass, m_impl->constructor, activity);
	if (!localObj || env->ExceptionCheck()) {
		if (localObj)
			env->DeleteLocalRef(localObj);
		m_impl->ClearGlobalReferences(env);
		return false;
	}

	m_impl->speechObj = env->NewGlobalRef(localObj);
	env->DeleteLocalRef(localObj);
#endif

	m_impl->head.store(0, std::memory_order_relaxed);
	m_impl->tail.store(0, std::memory_order_relaxed);
	m_impl->stopRequested.store(false, std::memory_order_release);
	m_impl->initialized.store(true, std::memory_order_release);

	m_impl->workerThread = std::thread([this]() { m_impl->BackgroundWorkerLoop(); });

	return true;
}

bool AndroidTextToSpeech::Uninitialize() {
	std::thread thread_to_join;
	{
		std::lock_guard<std::mutex> lock(m_impl->mutex);
		if (!m_impl->initialized.load(std::memory_order_relaxed))
			return true;

		m_impl->stopRequested.store(true, std::memory_order_release);

		size_t head_snap = m_impl->head.load(std::memory_order_relaxed);
		m_impl->tail.store(head_snap, std::memory_order_release);
		m_impl->initialized.store(false, std::memory_order_release);

		m_impl->ringBell.store(true, std::memory_order_release);
		m_impl->ringBell.notify_one();

		thread_to_join = std::move(m_impl->workerThread);
	}

	if (thread_to_join.joinable()) {
		thread_to_join.join();
	}

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	if (!m_impl->jvm)
		return true;

	ScopedAttachmentGuard jni(m_impl->jvm);
	JNIEnv* const env = jni.GetEnv();
	if (env) {
#ifdef __ANDROID__
		if (m_impl->speechObj && m_impl->midShutdown) {
			env->CallVoidMethod(m_impl->speechObj, m_impl->midShutdown);
			if (env->ExceptionCheck())
				env->ExceptionClear();
		}
#endif
		m_impl->ClearGlobalReferences(env);
	}
	m_impl->jvm = nullptr;
	return true;
}

bool AndroidTextToSpeech::Speak(const char* const speech_text, const bool interrupt) {
	if (!m_impl->initialized.load(std::memory_order_relaxed)) {
		if (!GetActive())
			return false;
		if (!Initialize())
			return false;
	}

	if (interrupt) {
		size_t head_snap = m_impl->head.load(std::memory_order_relaxed);
		m_impl->tail.store(head_snap, std::memory_order_release);
	}

	return m_impl->PushTask(TaskType::Speak, speech_text ? speech_text : "", 0.0f, interrupt);
}

bool AndroidTextToSpeech::StopSpeech() {
	return m_impl->PushTask(TaskType::Stop, "", 0.0f, true);
}

bool AndroidTextToSpeech::IsSpeaking() {
	std::lock_guard<std::mutex> lock(m_impl->mutex);
	if (!m_impl->jvm)
		return false;
	ScopedAttachmentGuard jni(m_impl->jvm);
	JNIEnv* const env = jni.GetEnv();
	if (!env || !m_impl->speechObj || !m_impl->midIsSpeaking)
		return false;

#ifdef __ANDROID__
	const jboolean speaking = env->CallBooleanMethod(m_impl->speechObj, m_impl->midIsSpeaking);
	if (env->ExceptionCheck()) {
		env->ExceptionClear();
		return false;
	}
	return (speaking == JNI_TRUE);
#else
	return false;
#endif
}

bool AndroidTextToSpeech::GetActive() {
	std::lock_guard<std::mutex> lock(m_impl->mutex);
	if (!m_impl->jvm)
		return false;
	ScopedAttachmentGuard jni(m_impl->jvm);
	JNIEnv* const env = jni.GetEnv();
	if (!env || !m_impl->speechObj || !m_impl->midIsActive)
		return false;

#ifdef __ANDROID__
	const jboolean active = env->CallBooleanMethod(m_impl->speechObj, m_impl->midIsActive);
	if (env->ExceptionCheck()) {
		env->ExceptionClear();
		return false;
	}
	return (active == JNI_TRUE);
#else
	return false;
#endif
}

bool AndroidTextToSpeech::SetParameter(const int param, const void* const value) {
	if (!value) [[unlikely]] {
		return false;
	}

	switch (param) {
	case SRAL_PARAM_SPEECH_VOLUME: {
		const int volume_int = *static_cast<const int*>(value);
		const float volume_float = static_cast<float>(volume_int);

		m_impl->cachedVolume.store(static_cast<uint64_t>(volume_int), std::memory_order_relaxed);
		return m_impl->PushTask(TaskType::SetVolume, "", volume_float / 100.0f, false);
	}
	case SRAL_PARAM_SPEECH_RATE: {
		const int rate_int = *static_cast<const int*>(value);
		const float rate_float = static_cast<float>(rate_int);

		m_impl->cachedRate.store(static_cast<uint64_t>(rate_int), std::memory_order_relaxed);
		return m_impl->PushTask(TaskType::SetRate, "", rate_float / 100.0f, false);
	}
	default:
		return false;
	}
}

bool AndroidTextToSpeech::GetParameter(const int param, void* const value) {
	if (!value) [[unlikely]] {
		return false;
	}

	switch (param) {
	case SRAL_PARAM_SPEECH_VOLUME:
		*static_cast<int*>(value) = static_cast<int>(m_impl->cachedVolume.load(std::memory_order_relaxed));
		return true;
	case SRAL_PARAM_SPEECH_RATE:
		*static_cast<int*>(value) = static_cast<int>(m_impl->cachedRate.load(std::memory_order_relaxed));
		return true;
	default:
		return false;
	}
}

bool AndroidTextToSpeech::SpeakSsml(const char* const, const bool) {
	return false;
}

void* AndroidTextToSpeech::SpeakToMemory(const char* const text,
	uint64_t* const buffer_size,
	int* const channels,
	int* const sample_rate,
	int* const bits_per_sample) {
	(void)text;
	(void)buffer_size;
	(void)channels;
	(void)sample_rate;
	(void)bits_per_sample;
	return nullptr;
}

bool AndroidTextToSpeech::Braille(const char* const) {
	return false;
}

bool AndroidTextToSpeech::PauseSpeech() {
	return false;
}

bool AndroidTextToSpeech::ResumeSpeech() {
	return false;
}

int AndroidTextToSpeech::GetFeatures() {
	return SRAL_SUPPORTS_SPEECH | SRAL_SUPPORTS_SPEECH_RATE | SRAL_SUPPORTS_SPEECH_VOLUME;
}

} // namespace Sral
