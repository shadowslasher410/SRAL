#ifndef SRAL_ANDROID_CONTEXT_H_
#define SRAL_ANDROID_CONTEXT_H_
#pragma once

#if defined(__ANDROID__)
#include <jni.h>

// Shared Android JNI context for SRAL engines.
//
// Native code on Android cannot obtain a JNIEnv* or the app's Activity on its
// own — the host application must provide them. The host sets them via
// SRAL_SetEngineParameter using SRAL_PARAM_ANDROID_JNI_ENV and
// SRAL_PARAM_ANDROID_ACTIVITY (see SRAL.h), which forward into the setters
// below. Engine implementations (e.g. AndroidTextToSpeech, future
// AndroidTalkBack) retrieve the values through the accessor functions.
//
// SetAndroidJNIEnv must be called before SetAndroidActivity, since creating
// a global ref for the activity requires a valid JNIEnv*.

namespace Sral {

/**
 * @brief Captures the JavaVM* from the provided env.
 * @param env The current JNI environment.
 * @return True if the JavaVM handle was successfully isolated.
 */
bool SetAndroidJNIEnv(JNIEnv* env) noexcept;

/**
 * @brief Stores a global ref to activity.
 * Requires SetAndroidJNIEnv to have been called first.
 * @param activity The jobject handle pointing to the Host Android Activity.
 * @return True if the global reference was safely instantiated.
 */
bool SetAndroidActivity(jobject activity) noexcept;

/**
 * @brief Releases the global ref to activity and clears cached JVM bindings.
 * Called by SRAL_Uninitialize. Completely thread-safe.
 */
void ClearAndroidContext() noexcept;

/**
 * @brief Returns a JNIEnv* valid on the calling thread, attaching it if necessary.
 * @return A thread-local valid JNIEnv*, or nullptr on terminal lookup failure.
 */
[[nodiscard]] JNIEnv* GetAndroidJNIEnv() noexcept;

/**
 * @brief Returns the Activity global ref.
 * @return The active global reference jobject, or nullptr if unassigned.
 */
[[nodiscard]] jobject GetAndroidActivity() noexcept;

} // namespace Sral

#endif // defined(__ANDROID__)

#endif // SRAL_ANDROID_CONTEXT_H_
