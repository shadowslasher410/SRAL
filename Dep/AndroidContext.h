#ifndef ANDROIDCONTEXT_H_
#define ANDROIDCONTEXT_H_
#pragma once

#include <cstdint>

#if defined(__ANDROID__)
#include <jni.h>
#else
using jint = int32_t;
using jboolean = uint8_t;

struct _jobject;
struct _jarray;

typedef struct _jobject* jobject;
typedef struct _jobject* jweak;
typedef struct _jobject* jclass;
typedef struct _jobject* jstring;
typedef void* jmethodID;

struct JNIEnv;
struct JavaVM;
#endif

namespace Sral {
class [[nodiscard]] ScopedAttachmentGuard final {
public:
	explicit ScopedAttachmentGuard(JavaVM* vm) noexcept;
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
};

/**
 * @brief An RAII wrapper that automatically manages the lifecycle of a JNI local reference.
 *
 * Ensures that DeleteLocalRef is executed immediately when the wrapper scope terminates,
 * preventing local reference table exhaustion on heavy background loops.
 */
class [[nodiscard]] ScopedLocalRef final {
public:
	ScopedLocalRef() noexcept = default;
	explicit ScopedLocalRef(JNIEnv* env, jobject ref) noexcept : env_(env), ref_(ref) {}
	~ScopedLocalRef() noexcept;

	// Local references are bound to their stack frame scope and cannot be copied
	ScopedLocalRef(const ScopedLocalRef&) = delete;
	ScopedLocalRef& operator=(const ScopedLocalRef&) = delete;

	// Supports explicit ownership transfers via C++ move semantics
	ScopedLocalRef(ScopedLocalRef&& other) noexcept;
	ScopedLocalRef& operator=(ScopedLocalRef&& other) noexcept;

	/**
	 * @brief Relinquishes ownership of the wrapped local reference without freeing it.
	 * @return The raw JNI jobject reference.
	 */
	[[nodiscard]] jobject release() noexcept;

	/**
	 * @brief Accesses the raw underlying local reference pointer.
	 */
	[[nodiscard]] jobject get() const noexcept { return ref_; }

	/**
	 * @brief Syntactic sugar to quickly check if the underlying reference is non-null.
	 */
	[[nodiscard]] explicit operator bool() const noexcept { return ref_ != nullptr; }

private:
	JNIEnv* env_{nullptr};
	jobject ref_{nullptr};
};

/**
 * @brief Registers or updates the global Java Virtual Machine instance.
 * @param env An active JNIEnv pointer belonging to the initialization thread.
 * @return True if the VM was extracted and assigned successfully, false otherwise.
 */
[[nodiscard]] bool SetAndroidJNIEnv(JNIEnv* env) noexcept;

/**
 * @brief Registers the current global Android Activity context.
 * @param activity A local or global reference to the active Android Activity.
 * @return True if a weak reference tracking point was safely instantiated.
 */
[[nodiscard]] bool SetAndroidActivity(jobject activity) noexcept;

/**
 * @brief Flushes all stored global states, clears references, and disconnects tracking handles.
 */
void ClearAndroidContext() noexcept;

/**
 * @brief Fetches or attaches a thread-specific execution pointer for the active JNI environment.
 * @return A valid JNIEnv pointer, or nullptr if the JVM is uninitialized or thread attachment fails.
 */
[[nodiscard]] JNIEnv* GetAndroidJNIEnv() noexcept;

/**
 * @brief Resolves and secures a safe local instance copy of the active Android Activity.
 * @return A ScopedLocalRef wrapping the activity. Check via explicit bool cast for null validation.
 */
[[nodiscard]] ScopedLocalRef GetAndroidActivity() noexcept;

/**
 * @brief Retrieves the globally registered Java Virtual Machine pointer.
 * @return The underlying JavaVM pointer, or nullptr if not set.
 */
[[nodiscard]] JavaVM* GetAndroidJavaVM() noexcept;

} // namespace Sral

#endif // ANDROIDCONTEXT_H_
