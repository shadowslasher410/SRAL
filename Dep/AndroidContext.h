#ifndef ANDROIDCONTEXT_H_
#define ANDROIDCONTEXT_H_
#pragma once

#include <cstdint>

#if defined(__ANDROID__)
#include <jni.h>
#define SRAL_NODISCARD __attribute__((warn_unused_result))
#else
#define SRAL_NODISCARD

using jint = int32_t;
using jboolean = uint8_t;
using jobject = void*;
using jweak = void*;
using jclass = void*;
using jstring = void*;
using jmethodID = void*;

struct JNIEnv {
	jint GetJavaVM(struct JavaVM**) noexcept { return 0; }
	void DeleteLocalRef(jobject) noexcept {}
	void DeleteWeakGlobalRef(jweak) noexcept {}
	jobject NewLocalRef(jweak) noexcept { return nullptr; }
	jboolean IsSameObject(jobject, jobject) noexcept { return 1; }
	jint PushLocalFrame(jint) noexcept { return 0; }
	void PopLocalFrame(jobject) noexcept {}
};

struct JavaVM {
	jint GetEnv(void**, jint) noexcept { return 0; }
	jint AttachCurrentThreadAsDaemon(JNIEnv**, void*) noexcept { return 0; }
	jint DetachCurrentThread() noexcept { return 0; }
};
#endif

namespace Sral {

class SRAL_NODISCARD ScopedAttachmentGuard final {
public:
	explicit ScopedAttachmentGuard() noexcept;
	explicit ScopedAttachmentGuard(JavaVM* vm) noexcept;
	~ScopedAttachmentGuard() noexcept;

	ScopedAttachmentGuard(const ScopedAttachmentGuard&) = delete;
	ScopedAttachmentGuard& operator=(const ScopedAttachmentGuard&) = delete;
	ScopedAttachmentGuard(ScopedAttachmentGuard&&) noexcept = delete;
	ScopedAttachmentGuard& operator=(ScopedAttachmentGuard&&) noexcept = delete;

	SRAL_NODISCARD JNIEnv* GetEnv() const noexcept { return env_; }

private:
	JavaVM* vm_;
	JNIEnv* env_;
	bool must_detach_;
	bool has_local_frame_;
};

class SRAL_NODISCARD ScopedLocalRef final {
public:
	ScopedLocalRef() noexcept = default;
	explicit ScopedLocalRef(JNIEnv* env, jobject ref) noexcept : env_(env), ref_(ref) {}
	~ScopedLocalRef() noexcept;

	ScopedLocalRef(const ScopedLocalRef&) = delete;
	ScopedLocalRef& operator=(const ScopedLocalRef&) = delete;

	ScopedLocalRef(ScopedLocalRef&& other) noexcept;
	ScopedLocalRef& operator=(ScopedLocalRef&& other) noexcept;

	SRAL_NODISCARD jobject release() noexcept;
	SRAL_NODISCARD jobject get() const noexcept { return ref_; }
	SRAL_NODISCARD explicit operator bool() const noexcept { return ref_ != nullptr; }

private:
	JNIEnv* env_{nullptr};
	jobject ref_{nullptr};
};

SRAL_NODISCARD bool SetAndroidJNIEnv(JNIEnv* env) noexcept;
SRAL_NODISCARD bool SetAndroidActivity(jobject activity) noexcept;
void ClearAndroidContext() noexcept;
SRAL_NODISCARD JNIEnv* GetAndroidJNIEnv() noexcept;
SRAL_NODISCARD ScopedLocalRef GetAndroidActivity() noexcept;
SRAL_NODISCARD JavaVM* GetAndroidJavaVM() noexcept;

} // namespace Sral

#endif // ANDROIDCONTEXT_H_
