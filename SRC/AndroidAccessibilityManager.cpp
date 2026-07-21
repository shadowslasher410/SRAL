#include "AndroidAccessibilityManager.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <string_view>
#include <thread>

#include "AndroidContext.h"

#ifdef __ANDROID__
#include <android/log.h>
#include <jni.h>
#define LOG_TAG "SRAL_AndroidAccessibility"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <iostream>
#define LOGE(...) std::cerr << "[SRAL Error] " << __VA_ARGS__ << "\n"
#endif

namespace Sral {
enum class TaskType : uint8_t { Speak, Stop };

struct alignas(CACHE_LINE_SIZE) AsyncSpeechTask {
	std::array<char, 512> text;
	std::atomic<size_t> sequence;
	TaskType type;
	bool interrupt;
};

struct AndroidAccessibilityManager::Impl {
	static constexpr size_t RING_BUFFER_SIZE = 128;
	static constexpr size_t RING_MASK = RING_BUFFER_SIZE - 1;
	alignas(CACHE_LINE_SIZE) std::atomic<size_t> head{0};
	alignas(CACHE_LINE_SIZE) std::array<AsyncSpeechTask, RING_BUFFER_SIZE> ringQueue;
	alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail{0};
	std::mutex bellMutex;
	std::condition_variable bellCond;
	bool ringBell{false};
	std::atomic<bool> stopRequested{false};

	alignas(CACHE_LINE_SIZE) std::mutex initMutex;
	std::atomic<bool> initialized{false};
	std::thread workerThread;

	JavaVM* jvm = nullptr;
	jclass announcerClass = nullptr;
	jobject announcerObj = nullptr;
	jmethodID constructor = nullptr;
	jmethodID midIsActive = nullptr;
	jmethodID midAnnounce = nullptr;
	jmethodID midStop = nullptr;
	jmethodID midShutdown = nullptr;

	void BackgroundWorkerLoop() noexcept;
	void UninitializeInternal(JNIEnv* env) noexcept;
};

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

AndroidAccessibilityManager::AndroidAccessibilityManager() : m_impl(std::make_unique<Impl>()) {
	for (size_t i = 0; i < Impl::RING_BUFFER_SIZE; ++i) {
		m_impl->ringQueue[i].sequence.store(i, std::memory_order_relaxed);
	}
}

AndroidAccessibilityManager::~AndroidAccessibilityManager() {
	Uninitialize();
}

bool AndroidAccessibilityManager::Initialize() {
	std::lock_guard<std::mutex> lock(m_impl->initMutex);
	if (m_impl->initialized.load(std::memory_order_acquire))
		return true;

#ifdef __ANDROID__
	m_impl->jvm = Sral::GetAndroidJavaVM();
	if (!m_impl->jvm) [[unlikely]] {
		LOGE("Accessibility Initialization Failed: JavaVM context tracking pointer is NULL.");
		return false;
	}

	ScopedAttachmentGuard jni(m_impl->jvm);
	JNIEnv* env = jni.GetEnv();
	if (!env) [[unlikely]]
		return false;

	ScopedLocalRef activityRef = GetAndroidActivity();
	jobject activity = activityRef.get();
	if (!activity) [[unlikely]]
		return false;

	m_impl->UninitializeInternal(env);

	jclass localClass = env->FindClass("org/sral/AndroidAccessibilityManagerHelper");
	if (!localClass || env->ExceptionCheck()) [[unlikely]] {
		env->ExceptionClear();
		LOGE("Accessibility Initialization Failed: Missing class org/sral/AndroidAccessibilityManagerHelper.");
		return false;
	}

	m_impl->announcerClass = static_cast<jclass>(env->NewGlobalRef(localClass));
	env->DeleteLocalRef(localClass);

	m_impl->constructor = env->GetMethodID(
		m_impl->announcerClass, "<init>", "(Landroid/content/Context;Landroidx/lifecycle/LifecycleOwner;)V");
	m_impl->midIsActive = env->GetMethodID(m_impl->announcerClass, "isActive", "()Z");
	m_impl->midAnnounce = env->GetMethodID(m_impl->announcerClass, "announce", "(Ljava/lang/String;Z)V");
	m_impl->midStop = env->GetMethodID(m_impl->announcerClass, "stop", "()V");
	m_impl->midShutdown = env->GetMethodID(m_impl->announcerClass, "shutdown", "()V");

	if (!m_impl->constructor || !m_impl->midIsActive || !m_impl->midAnnounce || !m_impl->midStop ||
		!m_impl->midShutdown) [[unlikely]] {
		m_impl->UninitializeInternal(env);
		return false;
	}

	jobject localObj =
		env->NewObject(m_impl->announcerClass, m_impl->constructor, activity, static_cast<jobject>(nullptr));
	if (!localObj || env->ExceptionCheck()) [[unlikely]] {
		env->ExceptionClear();
		m_impl->UninitializeInternal(env);
		return false;
	}

	m_impl->announcerObj = env->NewGlobalRef(localObj);
	env->DeleteLocalRef(localObj);
#endif

	m_impl->stopRequested.store(false, std::memory_order_release);
	m_impl->initialized.store(true, std::memory_order_release);
	m_impl->workerThread = std::thread([this]() { m_impl->BackgroundWorkerLoop(); });
	return true;
}

bool AndroidAccessibilityManager::Uninitialize() {
	std::thread threadToJoin;
	{
		std::lock_guard<std::mutex> lock(m_impl->initMutex);
		if (!m_impl->initialized.load(std::memory_order_acquire))
			return true;

		m_impl->stopRequested.store(true, std::memory_order_release);
		{
			std::lock_guard<std::mutex> bellLock(m_impl->bellMutex);
			m_impl->ringBell = true;
		}
		m_impl->bellCond.notify_all();

		threadToJoin = std::move(m_impl->workerThread);
		m_impl->initialized.store(false, std::memory_order_release);
	}

	if (threadToJoin.joinable()) {
		threadToJoin.join();
	}

#ifdef __ANDROID__
	if (m_impl->jvm) {
		ScopedAttachmentGuard jni(m_impl->jvm);
		JNIEnv* env = jni.GetEnv();
		if (env) {
			m_impl->UninitializeInternal(env);
		}
		m_impl->jvm = nullptr;
	}
#endif
	return true;
}

void AndroidAccessibilityManager::Impl::UninitializeInternal(JNIEnv* env) noexcept {
#ifdef __ANDROID__
	if (announcerObj) {
		if (midShutdown) {
			env->CallVoidMethod(announcerObj, midShutdown);
			if (env->ExceptionCheck())
				env->ExceptionClear();
		}
		env->DeleteGlobalRef(announcerObj);
		announcerObj = nullptr;
	}
	if (announcerClass) {
		env->DeleteGlobalRef(announcerClass);
		announcerClass = nullptr;
	}
#else
	(void)env;
#endif
	constructor = nullptr;
	midIsActive = nullptr;
	midAnnounce = nullptr;
	midStop = nullptr;
	midShutdown = nullptr;
}

bool AndroidAccessibilityManager::Speak(const char* speech_text, bool interrupt) {
	std::string_view text_view(speech_text ? speech_text : "");
	if (!m_impl->initialized.load(std::memory_order_acquire)) {
		std::lock_guard<std::mutex> lock(m_impl->initMutex);
		if (!m_impl->initialized.load(std::memory_order_acquire) && !Initialize())
			return false;
	}

	if (m_impl->stopRequested.load(std::memory_order_acquire))
		return false;

	AsyncSpeechTask* task = nullptr;
	size_t ticket = m_impl->head.load(std::memory_order_relaxed);
	while (true) {
		task = &m_impl->ringQueue[ticket & Impl::RING_MASK];
		size_t seq = task->sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

		if (difference == 0) {
			if (m_impl->head.compare_exchange_weak(ticket, ticket + 1, std::memory_order_relaxed)) {
				break;
			}
		}
		else if (difference < 0) {
			return false;
		}
		else {
			ticket = m_impl->head.load(std::memory_order_relaxed);
		}
	}

	size_t max_copy = (std::min)(static_cast<size_t>(text_view.size()), static_cast<size_t>(task->text.size() - 1));
	std::memcpy(task->text.data(), text_view.data(), max_copy);
	task->text[max_copy] = '\0';
	task->type = TaskType::Speak;
	task->interrupt = interrupt;

	task->sequence.store(ticket + 1, std::memory_order_release);
	{
		std::lock_guard<std::mutex> bellLock(m_impl->bellMutex);
		m_impl->ringBell = true;
	}
	m_impl->bellCond.notify_one();
	return true;
}

bool AndroidAccessibilityManager::StopSpeech() {
	if (!m_impl->initialized.load(std::memory_order_acquire))
		return false;

	AsyncSpeechTask* task = nullptr;
	size_t ticket = m_impl->head.load(std::memory_order_relaxed);
	while (true) {
		task = &m_impl->ringQueue[ticket & Impl::RING_MASK];
		size_t seq = task->sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

		if (difference == 0) {
			if (m_impl->head.compare_exchange_weak(ticket, ticket + 1, std::memory_order_relaxed)) {
				break;
			}
		}
		else {
			if (difference < 0)
				return false;
			ticket = m_impl->head.load(std::memory_order_relaxed);
		}
	}

	task->text[0] = '\0';
	task->type = TaskType::Stop;
	task->interrupt = true;

	task->sequence.store(ticket + 1, std::memory_order_release);

	{
		std::lock_guard<std::mutex> bellLock(m_impl->bellMutex);
		m_impl->ringBell = true;
	}
	m_impl->bellCond.notify_one();
	return true;
}

void AndroidAccessibilityManager::Impl::BackgroundWorkerLoop() noexcept {
#ifdef __ANDROID__
	JavaVM* localJvm = nullptr;
	jobject announcerObjGlobal = nullptr;
	jmethodID midAnnounceLocal = nullptr;
	jmethodID midStopLocal = nullptr;

	{
		std::lock_guard<std::mutex> lock(initMutex);
		localJvm = jvm;
		if (announcerObj)
			announcerObjGlobal = announcerObj;
		midAnnounceLocal = midAnnounce;
		midStopLocal = midStop;
	}

	if (!localJvm || !announcerObjGlobal)
		return;

	ScopedAttachmentGuard jni(localJvm);
	JNIEnv* env = jni.GetEnv();
	if (!env)
		return;
#endif

	while (!stopRequested.load(std::memory_order_acquire)) {
		size_t currentTail = tail.load(std::memory_order_acquire);
		AsyncSpeechTask& task = ringQueue[currentTail & Impl::RING_MASK];
		size_t seq = task.sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(currentTail + 1);

		if (difference != 0) {
			std::unique_lock<std::mutex> bellLock(bellMutex);
			while (!stopRequested.load(std::memory_order_acquire)) {
				size_t checkSeq = task.sequence.load(std::memory_order_acquire);
				if (static_cast<intptr_t>(checkSeq) - static_cast<intptr_t>(currentTail + 1) == 0) {
					break;
				}
				ringBell = false;
				bellCond.wait(bellLock);
			}

			if (stopRequested.load(std::memory_order_acquire)) [[unlikely]]
				return;
			continue;
		}

		if (!initialized.load(std::memory_order_relaxed)) [[unlikely]]
			return;

		const char* const localTextPtr = task.text.data();
		const TaskType localType = task.type;
		const bool localInterrupt = task.interrupt;

#ifdef __ANDROID__
		if (localType == TaskType::Stop) {
			if (midStopLocal) {
				env->CallVoidMethod(announcerObjGlobal, midStopLocal);
				CheckAndClearException(env, "CallVoidMethod: stop (via TaskType::Stop)");
			}
		}
		else if (localType == TaskType::Speak) {
			if (localInterrupt && midStopLocal) {
				env->CallVoidMethod(announcerObjGlobal, midStopLocal);
				CheckAndClearException(env, "CallVoidMethod: stop (via interruption trigger)");
			}
			if (midAnnounceLocal) {
				jstring javaString = env->NewStringUTF(localTextPtr);
				if (javaString) {
					env->CallVoidMethod(announcerObjGlobal,
						midAnnounceLocal,
						javaString,
						static_cast<jboolean>(localInterrupt ? JNI_TRUE : JNI_FALSE));
					CheckAndClearException(env, "CallVoidMethod: announce");
					env->DeleteLocalRef(javaString);
				}
			}
		}
#else
		(void)localTextPtr;
		(void)localType;
		(void)localInterrupt;
#endif

		tail.store(currentTail + 1, std::memory_order_release);
		task.sequence.store(currentTail + Impl::RING_BUFFER_SIZE, std::memory_order_release);
	}
}

bool AndroidAccessibilityManager::GetActive() {
	if (!m_impl->initialized.load(std::memory_order_acquire))
		return false;

#ifdef __ANDROID__
	if (!m_impl->jvm || !m_impl->announcerObj || !m_impl->midIsActive)
		return false;

	ScopedAttachmentGuard jni(m_impl->jvm);
	JNIEnv* env = jni.GetEnv();
	if (!env)
		return false;

	jboolean result = env->CallBooleanMethod(m_impl->announcerObj, m_impl->midIsActive);
	if (CheckAndClearException(env, "CallBooleanMethod: isActive"))
		return false;

	return result == JNI_TRUE;
#else
	return false;
#endif
}

} // namespace Sral