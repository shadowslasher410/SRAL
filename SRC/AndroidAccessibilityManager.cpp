#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <cstring>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string_view>
#include <thread>

#include "AndroidAccessibilityManager.h"
#include "../Dep/AndroidContext.h"

#ifdef __ANDROID__
#include <android/log.h>
#include <jni.h>
#define LOG_TAG "SRAL_AndroidAccessibility"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <iostream>
#define LOGE(...) std::cerr << "[SRAL Error] " << __VA_ARGS__ << "\n"

#ifndef JNI_VERSION_1_6
#define JNI_VERSION_1_6 0x00010006
#endif
#ifndef JNI_EDETACHED
#define JNI_EDETACHED    -2
#endif
#ifndef JNI_OK
#define JNI_OK           0
#endif
#ifndef JNI_TRUE
#define JNI_TRUE         1
#endif
#ifndef JNI_FALSE
#define JNI_FALSE        0
#endif
#endif

namespace Sral {
	
#ifdef __ANDROID__
static bool CheckAndClearException(JNIEnv* env, const char* contextMessage) noexcept {
	if (env->ExceptionCheck()) [[unlikely]] {
		LOGE("JNI Exception intercepted during execution loop step: %s", contextMessage);
		env->ExceptionDescribe();
		env->ExceptionClear();
		return true;
	}
	return false;
}
#endif

AndroidAccessibilityManager::AndroidAccessibilityManager() {
	for (size_t i = 0; i < RING_BUFFER_SIZE; ++i) {
		m_ring_queue[i].sequence.store(i, std::memory_order_relaxed);
	}
}

AndroidAccessibilityManager::~AndroidAccessibilityManager() {
	Uninitialize();
}

bool AndroidAccessibilityManager::IsScreenReaderActive() noexcept {
	return true;
}

bool AndroidAccessibilityManager::Initialize() {
	std::lock_guard lock(m_init_mutex);
	if (m_initialized) return true;

#ifdef __ANDROID__
	m_jvm = GetAndroidJavaVM();
	if (!m_jvm) [[unlikely]] {
		LOGE("Initialization Failed: Android JavaVM tracking context pointer is NULL.");
		return false;
	}

	ScopedAttachmentGuard jni(m_jvm);
	JNIEnv* localEnv = jni.GetEnv();
	if (!localEnv) [[unlikely]] {
		LOGE("Initialization Failed: Unable to attach native thread context to JavaVM.");
		return false;
	}
	
	ScopedLocalRef activityRef = GetAndroidActivity();
	jobject activity = activityRef.get();
	if (!activity) [[unlikely]] {
		LOGE("Initialization Failed: Target android/app/Activity local context handle is NULL.");
		return false;
	}

	UninitializeInternal(localEnv);

	jclass localClass = localEnv->FindClass("org/sral/AndroidAccessibilityManagerHelper");
	if (!localClass || localEnv->ExceptionCheck()) [[unlikely]] {
		localEnv->ExceptionClear();
		LOGE("Initialization Failed: Unable to locate helper class 'org/sral/AndroidAccessibilityManagerHelper'.");
		return false;
	}

	m_announcerClass = static_cast<jclass>(localEnv->NewGlobalRef(localClass));
	localEnv->DeleteLocalRef(localClass);
	
	m_constructor = localEnv->GetMethodID(m_announcerClass, "<init>", "(Landroid/content/Context;Landroidx/lifecycle/LifecycleOwner;)V");
	m_midIsActive = localEnv->GetMethodID(m_announcerClass, "isActive", "()Z");
	m_midAnnounce = localEnv->GetMethodID(m_announcerClass, "announce", "(Ljava/lang/String;Z)V");
	m_midStop = localEnv->GetMethodID(m_announcerClass, "stop", "()V");
	m_midShutdown = localEnv->GetMethodID(m_announcerClass, "shutdown", "()V");

	if (!m_constructor || !m_midIsActive || !m_midAnnounce || !m_midStop || !m_midShutdown) [[unlikely]] {
		LOGE("Initialization Failed: One or more target JNI Java Method ID mappings are missing.");
		UninitializeInternal(localEnv);
		return false;
	}

	jobject localObj = localEnv->NewObject(m_announcerClass, m_constructor, activity, static_cast<jobject>(nullptr));
	if (!localObj || CheckAndClearException(localEnv, "NewObject Failed")) [[unlikely]] {
		if (localObj) localEnv->DeleteLocalRef(localObj);
		UninitializeInternal(localEnv);
		return false;
	}

	m_announcerObj = localEnv->NewGlobalRef(localObj);
	localEnv->DeleteLocalRef(localObj);
#endif

	m_initialized = true;
	m_worker_thread = std::jthread([this](std::stop_token st) {
		BackgroundWorkerLoop(st);
	});

	return true;
}

bool AndroidAccessibilityManager::Uninitialize() {
	std::jthread thread_to_join;
	{
		std::lock_guard lock(m_init_mutex);
		if (!m_initialized) return true;
		
		m_worker_thread.request_stop();
		
		size_t head_snap = m_head.load(std::memory_order_relaxed);
		m_tail.store(head_snap, std::memory_order_release);
		
		m_ring_bell.store(true, std::memory_order_release);
		m_ring_bell.notify_one();
		
		thread_to_join = std::move(m_worker_thread);
		m_initialized = false;
	}

	if (thread_to_join.joinable()) {
		thread_to_join.join();
	}

#ifdef __ANDROID__
	{
		std::lock_guard lock(m_init_mutex);
		if (m_jvm) {
			ScopedAttachmentGuard jni(m_jvm);
			JNIEnv* localEnv = jni.GetEnv();
			if (localEnv) {
				UninitializeInternal(localEnv);
			}
			m_jvm = nullptr;
		}
	}
#endif
	return true;
}

void AndroidAccessibilityManager::UninitializeInternal(JNIEnv* localEnv) noexcept {
#ifdef __ANDROID__
	if (m_announcerObj) {
		if (m_midShutdown) {
			localEnv->CallVoidMethod(m_announcerObj, m_midShutdown);
			CheckAndClearException(localEnv, "CallVoidMethod: shutdown");
		}
		localEnv->DeleteGlobalRef(m_announcerObj);
		m_announcerObj = nullptr;
	}
	if (m_announcerClass) {
		localEnv->DeleteGlobalRef(m_announcerClass);
		m_announcerClass = nullptr;
	}
#else
	(void)localEnv;
#endif
	m_constructor = nullptr;
	m_midIsActive = nullptr;
	m_midAnnounce = nullptr;
	m_midStop = nullptr;
	m_midShutdown = nullptr;
}

bool AndroidAccessibilityManager::GetActive() {
	std::lock_guard lock(m_init_mutex);
	if (!m_initialized) return false;

#ifdef __ANDROID__
	if (!m_jvm || !m_announcerObj || !m_midIsActive) return false;
	ScopedAttachmentGuard jni(m_jvm);
	JNIEnv* localEnv = jni.GetEnv();
	if (!localEnv) return false;

	jboolean active = localEnv->CallBooleanMethod(m_announcerObj, m_midIsActive);
	if (CheckAndClearException(localEnv, "CallBooleanMethod: isActive")) return false;
	return active == JNI_TRUE;
#else
	return false;
#endif
}

bool AndroidAccessibilityManager::Speak(const char* speech_text, bool interrupt) {
	std::string_view text_view(speech_text ? speech_text : "");
	
	if (!m_initialized) {
		if (!IsScreenReaderActive()) return false;
		std::lock_guard lock(m_init_mutex);
		if (!m_initialized && !Initialize()) return false;
	}

	if (m_worker_thread.get_stop_token().stop_requested()) return false;

	AsyncSpeechTask* task = nullptr;
	size_t ticket = m_head.load(std::memory_order_relaxed);

	while (true) {
		task = &m_ring_queue[ticket & RING_MASK];
		size_t seq = task->sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

		if (difference == 0) {
			if (m_head.compare_exchange_weak(ticket, ticket + 1, std::memory_order_relaxed)) {
				break;
			}
		} else if (difference < 0) {
			return false;
		} else {
			ticket = m_head.load(std::memory_order_relaxed);
		}
	}

	size_t max_copy = (std::min)(static_cast<size_t>(text_view.size()), static_cast<size_t>(task->text.size() - 1));
	std::memcpy(task->text.data(), text_view.data(), max_copy);
	task->text[max_copy] = '\0';
	task->type = TaskType::Speak;
	task->interrupt = interrupt;

	task->sequence.store(ticket + 1, std::memory_order_release);
	
	if (!m_ring_bell.exchange(true, std::memory_order_release)) {
		m_ring_bell.notify_one();
	}
	return true;
}

bool AndroidAccessibilityManager::StopSpeech() {
	if (!m_initialized) [[unlikely]] return false;

	AsyncSpeechTask* task = nullptr;
	size_t ticket = m_head.load(std::memory_order_relaxed);

	while (true) {
		task = &m_ring_queue[ticket & RING_MASK];
		size_t seq = task->sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

		if (difference == 0) {
			if (m_head.compare_exchange_weak(ticket, ticket + 1, std::memory_order_relaxed)) {
				break;
			}
		} else {
			if (difference < 0) {
				return false;
			}
			ticket = m_head.load(std::memory_order_relaxed);
#if defined(__aarch64__) || defined(__arm__)
			asm volatile("yield" ::: "memory");
#endif
		}
	}

	task->text[0] = '\0';
	task->type = TaskType::Stop;
	task->interrupt = true;

	task->sequence.store(ticket + 1, std::memory_order_release);
	
	if (!m_ring_bell.exchange(true, std::memory_order_release)) {
		m_ring_bell.notify_one();
	}
	return true;
}

void AndroidAccessibilityManager::BackgroundWorkerLoop(std::stop_token stop_token) noexcept {
#ifdef __ANDROID__
	JavaVM* localJvm = nullptr;
	jobject announcerObjGlobal = nullptr;
	jmethodID midAnnounceLocal = nullptr;
	jmethodID midStopLocal = nullptr;

	{
		std::lock_guard lock(m_init_mutex);
		localJvm = m_jvm;
		if (m_announcerObj) announcerObjGlobal = m_announcerObj;
		midAnnounceLocal = m_midAnnounce;
		midStopLocal = m_midStop;
	}

	if (!localJvm || !announcerObjGlobal) return;

	ScopedAttachmentGuard jni(localJvm);
	JNIEnv* env = jni.GetEnv();
	if (!env) return;
#endif

	while (!stop_token.stop_requested()) {
		size_t current_tail = m_tail.load(std::memory_order_acquire);
		AsyncSpeechTask& task = m_ring_queue[current_tail & RING_MASK];
		
		size_t seq = task.sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1);

		if (difference != 0) {
			m_ring_bell.store(false, std::memory_order_release);
			
			seq = task.sequence.load(std::memory_order_acquire);
			if (static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail + 1) != 0) {
				while (!m_ring_bell.load(std::memory_order_acquire) && !stop_token.stop_requested()) {
					m_ring_bell.wait(false, std::memory_order_acquire);
				}
			} else {
				m_ring_bell.store(true, std::memory_order_release);
			}
			if (stop_token.stop_requested()) [[unlikely]] return;
			continue;
		}

		const char* const localTextPtr = task.text.data();
		const TaskType localType = task.type;
		const bool localInterrupt = task.interrupt;

#ifdef __ANDROID__
		if (localType == TaskType::Stop) {
			if (midStopLocal) {
				env->CallVoidMethod(announcerObjGlobal, midStopLocal);
				CheckAndClearException(env, "CallVoidMethod: stop (via TaskType::Stop)");
			}
		} else if (localType == TaskType::Speak) {
			if (localInterrupt && midStopLocal) {
				env->CallVoidMethod(announcerObjGlobal, midStopLocal);
				CheckAndClearException(env, "CallVoidMethod: stop (via interruption trigger)");
			}
			if (midAnnounceLocal) {
				jstring javaString = env->NewStringUTF(localTextPtr);
				if (javaString) {
					env->CallVoidMethod(announcerObjGlobal, midAnnounceLocal, javaString, static_cast<jboolean>(localInterrupt ? JNI_TRUE : JNI_FALSE));
					CheckAndClearException(env, "CallVoidMethod: announce");
					env->DeleteLocalRef(javaString);
				}
			}
		}
#else
		(void)localTextPtr; (void)localType; (void)localInterrupt;
#endif

		m_tail.store(current_tail + 1, std::memory_order_release);
		task.sequence.store(current_tail + RING_BUFFER_SIZE, std::memory_order_release);
	}
}

} // namespace Sral
