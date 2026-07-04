#include "AndroidAccessibilityManager.h"

#include <android/log.h>

#include "../Dep/AndroidContext.h"

#define LOG_TAG "SRAL_AndroidAccessibility"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace Sral {

class JniThreadGuard final {
public:
	explicit JniThreadGuard(JavaVM* vm) noexcept : m_vm(vm) {
		if (!m_vm)
			return;

		jint res = m_vm->GetEnv(reinterpret_cast<void**>(&m_env), JNI_VERSION_1_6);
		if (res == JNI_EDETACHED) {
#ifdef __ANDROID__
			res = m_vm->AttachCurrentThread(&m_env, nullptr);
#else
			res = m_vm->AttachCurrentThread(reinterpret_cast<void**>(&m_env), nullptr);
#endif
			if (res == JNI_OK) {
				m_attached = true;
			}
			else {
				LOGE("Critical: Failed to attach current native thread to the Java Virtual Machine.");
				m_env = nullptr;
			}
		}
	}

	~JniThreadGuard() noexcept {
		if (m_attached && m_vm) {
			m_vm->DetachCurrentThread();
		}
	}

	[[nodiscard]] JNIEnv* GetEnv() const noexcept { return m_env; }
	[[nodiscard]] bool IsValid() const noexcept { return m_env != nullptr; }

private:
	JavaVM* m_vm = nullptr;
	bool m_attached = false;
	JNIEnv* m_env = nullptr;
};

static bool CheckAndClearException(JNIEnv* env, const char* contextMessage) noexcept {
	if (env->ExceptionCheck()) {
		LOGE("JNI Exception intercepted during execution loop step: %s", contextMessage);
		env->ExceptionDescribe();
		env->ExceptionClear();
		return true;
	}
	return false;
}

AndroidAccessibilityManager::AndroidAccessibilityManager() = default;

AndroidAccessibilityManager::~AndroidAccessibilityManager() {
	Uninitialize();
}

AndroidAccessibilityManager::AndroidAccessibilityManager(AndroidAccessibilityManager&& other) noexcept {
	std::lock_guard lock(other.m_mutex);

	if (other.m_initialized) {
		LOGE("[AndroidAccessibilityManager] Critical: Refusing move-construction of an active manager instance.");
		return;
	}
	m_bound_window = other.m_bound_window;
	m_queue = std::move(other.m_queue);
	m_use_id_b = other.m_use_id_b;
	other.m_bound_window = nullptr;
}

AndroidAccessibilityManager& AndroidAccessibilityManager::operator=(AndroidAccessibilityManager&& other) noexcept {
	if (this != &other) {
		Uninitialize();
		std::scoped_lock lock(m_mutex, other.m_mutex);

		if (other.m_initialized) {
			LOGE("[AndroidAccessibilityManager] Critical: Refusing move-assignment of an active manager instance.");
			return *this;
		}
		m_bound_window = other.m_bound_window;
		m_queue = std::move(other.m_queue);
		m_use_id_b = other.m_use_id_b;
		other.m_bound_window = nullptr;
	}
	return *this;
}

bool AndroidAccessibilityManager::IsScreenReaderActive() noexcept {
	return true;
}

bool AndroidAccessibilityManager::Initialize() {
	std::lock_guard lock(m_mutex);
	if (m_initialized)
		return true;

	m_jvm = GetAndroidJavaVM();
	if (!m_jvm) {
		LOGE("Initialization Failed: Android JavaVM tracking context pointer is NULL.");
		return false;
	}

	JniThreadGuard jni(m_jvm);
	if (!jni.IsValid())
		return false;
	JNIEnv* localEnv = jni.GetEnv();
	jobject activityLocal = GetAndroidActivity();
	if (!activityLocal) {
		LOGE("Initialization Failed: Target android/app/Activity local context handle is NULL.");
		return false;
	}

	UninitializeInternal(localEnv);

	jclass localClass = localEnv->FindClass("org/sral/AndroidAccessibilityManagerHelper");
	if (!localClass || localEnv->ExceptionCheck()) {
		localEnv->ExceptionClear();
		LOGE("Initialization Failed: Unable to locate helper class 'org/sral/AndroidAccessibilityManagerHelper'.");
		return false;
	}

	m_announcerClass = static_cast<jclass>(localEnv->NewGlobalRef(localClass));
	localEnv->DeleteLocalRef(localClass);
	m_constructor = localEnv->GetMethodID(m_announcerClass, "<init>", "(Landroid/content/Context;)V");
	m_midIsActive = localEnv->GetMethodID(m_announcerClass, "isActive", "()Z");
	m_midAnnounce = localEnv->GetMethodID(m_announcerClass, "announce", "(Ljava/lang/String;Z)V");
	m_midStop = localEnv->GetMethodID(m_announcerClass, "stop", "()V");
	m_midShutdown = localEnv->GetMethodID(m_announcerClass, "shutdown", "()V");

	if (!m_constructor || !m_midIsActive || !m_midAnnounce || !m_midStop || !m_midShutdown) {
		LOGE("Initialization Failed: One or more target JNI Java Method ID mappings are missing.");
		UninitializeInternal(localEnv);
		return false;
	}

	jobject localObj = localEnv->NewObject(m_announcerClass, m_constructor, activityLocal);
	if (!localObj || localEnv->ExceptionCheck()) {
		localEnv->ExceptionClear();
		LOGE("Initialization Failed: Object constructor instantiation pass threw an exception.");
		UninitializeInternal(localEnv);
		return false;
	}

	m_announcerObj = localEnv->NewGlobalRef(localObj);
	localEnv->DeleteLocalRef(localObj);

	m_initialized = true;
	m_worker_thread = std::jthread(&AndroidAccessibilityManager::BackgroundWorkerLoop, this);
	return true;
}

bool AndroidAccessibilityManager::Uninitialize() {
	std::jthread thread_to_join;
	{
		std::lock_guard lock(m_mutex);
		if (!m_initialized)
			return true;
		m_worker_thread.request_stop();
		std::queue<AsyncSpeechTask>().swap(m_queue);
		m_semaphore.release();
		thread_to_join = std::move(m_worker_thread);
		m_initialized = false;
	}

	if (thread_to_join.joinable()) {
		thread_to_join.join();
	}

	{
		std::lock_guard lock(m_mutex);
		if (m_jvm) {
			JniThreadGuard jni(m_jvm);
			if (jni.IsValid()) {
				UninitializeInternal(jni.GetEnv());
			}
			m_jvm = nullptr;
		}
	}
	return true;
}

void AndroidAccessibilityManager::UninitializeInternal(JNIEnv* localEnv) noexcept {
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

	m_constructor = nullptr;
	m_midIsActive = nullptr;
	m_midAnnounce = nullptr;
	m_midStop = nullptr;
	m_midShutdown = nullptr;
}

bool AndroidAccessibilityManager::GetActive() {
	std::lock_guard lock(m_mutex);
	if (!m_initialized || !m_jvm || !m_announcerObj || !m_midIsActive)
		return false;

	JniThreadGuard jni(m_jvm);
	if (!jni.IsValid())
		return false;
	JNIEnv* localEnv = jni.GetEnv();

	jboolean active = localEnv->CallBooleanMethod(m_announcerObj, m_midIsActive);
	if (CheckAndClearException(localEnv, "CallBooleanMethod: isActive")) {
		return false;
	}

	return active == JNI_TRUE;
}

bool AndroidAccessibilityManager::Speak(const char* text, bool interrupt) {
	if (!text)
		text = "";

	std::lock_guard lock(m_mutex);
	if (!m_initialized)
		return false;

	if (interrupt) {
		std::queue<AsyncSpeechTask>().swap(m_queue);
	}

	m_queue.push(AsyncSpeechTask{.type = TaskType::Speak, .text = std::string(text), .interrupt = interrupt});
	m_semaphore.release();
	return true;
}

bool AndroidAccessibilityManager::StopSpeech() {
	std::lock_guard lock(m_mutex);
	if (!m_initialized)
		return false;

	std::queue<AsyncSpeechTask>().swap(m_queue);
	m_queue.push(AsyncSpeechTask{.type = TaskType::Stop, .text = "", .interrupt = true});
	m_semaphore.release();
	return true;
}

void AndroidAccessibilityManager::BackgroundWorkerLoop(std::stop_token stop_token) noexcept {
	while (!stop_token.stop_requested()) {
		m_semaphore.acquire();

		if (stop_token.stop_requested()) {
			break;
		}

		AsyncSpeechTask task;
		JavaVM* local_jvm = nullptr;
		jobject local_obj = nullptr;
		jmethodID target_mid = nullptr;

		{
			std::lock_guard lock(m_mutex);

			if (m_queue.empty()) {
				continue;
			}

			task = std::move(m_queue.front());
			m_queue.pop();

			local_jvm = m_jvm;
			local_obj = m_announcerObj;

			if (task.type == TaskType::Speak) {
				target_mid = m_midAnnounce;
			}
			else if (task.type == TaskType::Stop) {
				target_mid = m_midStop;
			}
		}

		if (!local_jvm || !local_obj || !target_mid)
			continue;

		JniThreadGuard jni(local_jvm);
		if (!jni.IsValid())
			continue;
		JNIEnv* localEnv = jni.GetEnv();

		if (task.type == TaskType::Speak) {
			jstring jtext = localEnv->NewStringUTF(task.text.c_str());
			if (!jtext || localEnv->ExceptionCheck()) {
				localEnv->ExceptionClear();
				continue;
			}

			localEnv->CallVoidMethod(local_obj, target_mid, jtext, static_cast<jboolean>(task.interrupt));
			CheckAndClearException(localEnv, "CallVoidMethod: announce");

			localEnv->DeleteLocalRef(jtext);
		}
		else if (task.type == TaskType::Stop) {
			localEnv->CallVoidMethod(local_obj, target_mid);
			CheckAndClearException(localEnv, "CallVoidMethod: stop");
		}
	}
}

} // namespace Sral
