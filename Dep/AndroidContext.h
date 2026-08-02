#ifndef ANDROIDCONTEXT_H_
#define ANDROIDCONTEXT_H_
#pragma once

#include <cstdint>

#if defined(__ANDROID__)
#include <jni.h>
#else
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

class [[nodiscard]] ScopedAttachmentGuard final {
public:
    explicit ScopedAttachmentGuard() noexcept = delete;
    explicit ScopedAttachmentGuard(JavaVM* const vm) noexcept;
    ~ScopedAttachmentGuard() noexcept;

    ScopedAttachmentGuard(const ScopedAttachmentGuard&) = delete;
    ScopedAttachmentGuard& operator=(const ScopedAttachmentGuard&) = delete;
    ScopedAttachmentGuard(ScopedAttachmentGuard&&) noexcept = delete;
    ScopedAttachmentGuard& operator=(ScopedAttachmentGuard&&) noexcept = delete;

    [[nodiscard]] JNIEnv* GetEnv() const noexcept { return env_; }

private:
    JavaVM* vm_;
    JNIEnv* env_;
    bool must_detach_;
    bool has_local_frame_;
};

class [[nodiscard]] ScopedLocalRef final {
public:
    ScopedLocalRef() noexcept = default;
    explicit ScopedLocalRef(JNIEnv* const env, jobject const ref) noexcept : env_(env), ref_(ref) {}
    ~ScopedLocalRef() noexcept;

    ScopedLocalRef(const ScopedLocalRef&) = delete;
    ScopedLocalRef& operator=(const ScopedLocalRef&) = delete;

    ScopedLocalRef(ScopedLocalRef&& other) noexcept;
    ScopedLocalRef& operator=(ScopedLocalRef&& other) noexcept;

    jobject release() noexcept;
    [[nodiscard]] jobject get() const noexcept { return ref_; }
    [[nodiscard]] explicit operator bool() const noexcept { return ref_ != nullptr; }

private:
    JNIEnv* env_{nullptr};
    jobject ref_{nullptr};
};

[[nodiscard]] bool SetAndroidJNIEnv(JNIEnv* const env) noexcept;
[[nodiscard]] bool SetAndroidActivity(jobject const activity) noexcept;
void ClearAndroidContext() noexcept;

[[nodiscard]] JNIEnv* GetAndroidJNIEnv() noexcept;
[[nodiscard]] ScopedLocalRef GetAndroidActivity() noexcept;
[[nodiscard]] JavaVM* GetAndroidJavaVM() noexcept;

} // namespace Sral

#endif // ANDROIDCONTEXT_H_