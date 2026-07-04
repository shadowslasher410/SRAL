#if defined(__ANDROID__)

#include "AndroidTextToSpeech.h"

#include <android/log.h>
#include <concepts>
#include <jni.h>
#include <mutex>

#include "../Dep/AndroidContext.h"
#include "../Include/SRAL.h"

#define LOG_TAG "SRAL_AndroidTTS"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace Sral {

template <typename T>
concept SralParameterType = std::integral<std::remove_pointer_t<std::decay_t<T>>>;

struct AndroidTextToSpeech::Impl final {
	std::mutex mutex;
	JavaVM* jvm{nullptr};
	jclass speechClass{nullptr};
	jobject speechObj{nullptr};

	jmethodID constructor{nullptr};
	jmethodID midIsActive{nullptr};
	jmethodID midIsSpeaking{nullptr};
	jmethodID midSpeak{nullptr};
	jmethodID midSilence{nullptr};
	jmethodID m_midShutdown{nullptr};

	uint64_t cachedVolume{100};
	uint64_t cachedRate{100};

	[[nodiscard]] JNIEnv* GetValidEnv() const noexcept {
		if (!jvm) {
			return nullptr;
		}

		JNIEnv* local_env = nullptr;
		const jint env_status = jvm->GetEnv(reinterpret_cast<void**>(&local_env), JNI_VERSION_1_6);

		if (env_status == JNI_EDETACHED) {
			if (jvm->AttachCurrentThread(&local_env, nullptr) != JNI_OK) {
				return nullptr;
			}
		}
		return local_env;
	}

	bool CheckAndClearException(JNIEnv* const local_env, const char* const msg) const noexcept {
		if (local_env && local_env->ExceptionCheck()) {
			LOGE("JNI Exception encountered: %s", msg ? msg : "Unknown Context");
			local_env->ExceptionClear();
			return true;
		}
		return false;
	}

	void ClearGlobalReferences(JNIEnv* const local_env) noexcept {
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
	}
};

AndroidTextToSpeech::AndroidTextToSpeech() : m_impl(std::make_unique<Impl>()) {}

AndroidTextToSpeech::~AndroidTextToSpeech() {
	static_cast<void>(Uninitialize());
}
bool AndroidTextToSpeech::Initialize() {
	std::lock_guard<std::mutex> lock(m_impl->mutex);
	m_impl->jvm = GetAndroidJavaVM();
	if (!m_impl->jvm)
		return false;

	JNIEnv* const env = m_impl->GetValidEnv();
	if (!env)
		return false;

	const jobject activity = GetAndroidActivity();
	if (!activity)
		return false;

	m_impl->ClearGlobalReferences(env);

	jclass localClass = env->FindClass("org/sral/AndroidTTSHelper");
	if (!localClass || env->ExceptionCheck()) {
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
	m_impl->m_midShutdown = env->GetMethodID(m_impl->speechClass, "shutdown", "()V");

	if (!m_impl->constructor || !m_impl->midSpeak || !m_impl->midSilence || !m_impl->midIsActive ||
		!m_impl->m_midShutdown) {
		m_impl->ClearGlobalReferences(env);
		return false;
	}

	jobject localObj = env->NewObject(m_impl->speechClass, m_impl->constructor, activity);
	if (!localObj || m_impl->CheckAndClearException(env, "NewObject Failed")) {
		if (localObj)
			env->DeleteLocalRef(localObj);
		m_impl->ClearGlobalReferences(env);
		return false;
	}

	m_impl->speechObj = env->NewGlobalRef(localObj);
	env->DeleteLocalRef(localObj);
	return true;
}

bool AndroidTextToSpeech::Uninitialize() {
	std::lock_guard<std::mutex> lock(m_impl->mutex);
	JNIEnv* const env = m_impl->GetValidEnv();
	if (!env)
		return false;

	if (m_impl->speechObj && m_impl->m_midShutdown) {
		env->CallVoidMethod(m_impl->speechObj, m_impl->m_midShutdown);
		static_cast<void>(m_impl->CheckAndClearException(env, "shutdown"));
	}

	m_impl->ClearGlobalReferences(env);
	m_impl->jvm = nullptr;
	return true;
}

bool AndroidTextToSpeech::Speak(const char* const text, const bool interrupt) {
	const char* const target_text = text ? text : "";
	std::lock_guard<std::mutex> lock(m_impl->mutex);

	JNIEnv* const env = m_impl->GetValidEnv();
	if (!env || !m_impl->speechObj || !m_impl->midSpeak)
		return false;

	const jstring jtext = env->NewStringUTF(target_text);
	if (!jtext || m_impl->CheckAndClearException(env, "NewStringUTF")) {
		if (jtext)
			env->DeleteLocalRef(jtext);
		return false;
	}

	env->CallVoidMethod(m_impl->speechObj, m_impl->midSpeak, jtext, static_cast<jboolean>(interrupt));
	static_cast<void>(m_impl->CheckAndClearException(env, "speak"));
	env->DeleteLocalRef(jtext);
	return true;
}

bool AndroidTextToSpeech::StopSpeech() {
	std::lock_guard<std::mutex> lock(m_impl->mutex);
	JNIEnv* const env = m_impl->GetValidEnv();
	if (!env || !m_impl->speechObj || !m_impl->midSilence)
		return false;

	env->CallVoidMethod(m_impl->speechObj, m_impl->midSilence);
	return !m_impl->CheckAndClearException(env, "stop");
}

bool AndroidTextToSpeech::IsSpeaking() {
	std::lock_guard<std::mutex> lock(m_impl->mutex);
	JNIEnv* const env = m_impl->GetValidEnv();
	if (!env || !m_impl->speechObj || !m_impl->midIsSpeaking)
		return false;

	const jboolean speaking = env->CallBooleanMethod(m_impl->speechObj, m_impl->midIsSpeaking);
	if (m_impl->CheckAndClearException(env, "isSpeaking"))
		return false;
	return (speaking == JNI_TRUE);
}

bool AndroidTextToSpeech::GetActive() {
	std::lock_guard<std::mutex> lock(m_impl->mutex);
	JNIEnv* const env = m_impl->GetValidEnv();
	if (!env || !m_impl->speechObj || !m_impl->midIsActive)
		return false;

	const jboolean active = env->CallBooleanMethod(m_impl->speechObj, m_impl->midIsActive);
	if (m_impl->CheckAndClearException(env, "isActive"))
		return false;
	return (active == JNI_TRUE);
}
bool AndroidTextToSpeech::SetParameter(const int param, const void* const value) {
	if (!value)
		return false;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	JNIEnv* const env = m_impl->GetValidEnv();
	if (!env || !m_impl->speechObj)
		return false;

	switch (param) {
	case SRAL_PARAM_SPEECH_VOLUME: {
		if (!m_impl->midSetVolume)
			return false;
		const float volume = static_cast<float>(*reinterpret_cast<const int*>(value));
		m_impl->cachedVolume = static_cast<uint64_t>(volume);
		env->CallVoidMethod(m_impl->speechObj, m_impl->midSetVolume, static_cast<jfloat>(volume / 100.0f));
		return !m_impl->CheckAndClearException(env, "setVolume");
	}
	case SRAL_PARAM_SPEECH_RATE: {
		if (!m_impl->midSetRate)
			return false;
		const float rate = static_cast<float>(*reinterpret_cast<const int*>(value));
		m_impl->cachedRate = static_cast<uint64_t>(rate);
		env->CallVoidMethod(m_impl->speechObj, m_impl->midSetRate, static_cast<jfloat>(rate / 100.0f));
		return !m_impl->CheckAndClearException(env, "setSpeechRate");
	}
	default:
		return false;
	}
}

bool AndroidTextToSpeech::GetParameter(const int param, void* const value) {
	if (!value)
		return false;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	switch (param) {
	case SRAL_PARAM_SPEECH_VOLUME:
		*reinterpret_cast<int*>(value) = static_cast<int>(m_impl->cachedVolume);
		return true;
	case SRAL_PARAM_SPEECH_RATE:
		*reinterpret_cast<int*>(value) = static_cast<int>(m_impl->cachedRate);
		return true;
	default:
		return false;
	}
}

bool AndroidTextToSpeech::SpeakSsml(const char* const, const bool) {
	return false;
}

void* AndroidTextToSpeech::SpeakToMemory(const char* const, uint64_t* const, int* const, int* const, int* const) {
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

#endif // defined(__ANDROID__)
