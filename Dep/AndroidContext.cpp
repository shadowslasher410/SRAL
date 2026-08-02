#include "AndroidContext.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <version>

#ifdef __ANDROID__
#include <android/log.h>
#include <pthread.h>
#define LOG_TAG "SRAL_AndroidContext"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <iostream>
#define LOGE(...) std::cerr << "[SRAL Error] " << __VA_ARGS__ << "\n"
#endif

#if defined(__cpp_lib_hardware_interference_size) && !defined(__APPLE__)
using std::hardware_destructive_interference_size;
#else
#if defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64) || defined(TARGET_CPU_ARM64)
static constexpr size_t hardware_destructive_interference_size = 128;
#else
static constexpr size_t hardware_destructive_interference_size = 64;
#endif
#endif

namespace Sral {
namespace {

#ifdef __ANDROID__
alignas(hardware_destructive_interference_size) std::atomic<JavaVM*> g_vm{nullptr};
alignas(hardware_destructive_interference_size) std::atomic<jweak> g_weak_activity{nullptr};

alignas(hardware_destructive_interference_size) std::mutex g_context_mutex;
std::atomic<uint64_t> g_context_epoch{0};
std::atomic<uint32_t> g_readers_count{0};

constexpr jint TARGET_JNI_VERSION = JNI_VERSION_1_6;

struct ThreadJniCache {
	JNIEnv* env{nullptr};
	JavaVM* bound_vm{nullptr};
	uint64_t epoch{0};
};

inline thread_local ThreadJniCache t_jni_cache{};

struct ReaderGuard {
	ReaderGuard() noexcept { g_readers_count.fetch_add(1, std::memory_order_acquire); }
	~ReaderGuard() noexcept { g_readers_count.fetch_sub(1, std::memory_order_release); }
	ReaderGuard(const ReaderGuard&) = delete;
	ReaderGuard& operator=(const ReaderGuard&) = delete;
};

pthread_key_t g_detach_key;
std::once_flag g_thread_key_once;

void PthreadThreadDetacher(void* value) noexcept {
	if (value) [[likely]] {
		JavaVM* attached_vm = reinterpret_cast<JavaVM*>(value);
		void* dummy{nullptr};
		if (attached_vm->GetEnv(&dummy, TARGET_JNI_VERSION) == JNI_OK) {
			attached_vm->DetachCurrentThread();
		}
	}
}

void InitializePthreadKey() noexcept {
	pthread_key_create(&g_detach_key, PthreadThreadDetacher);
}

inline void RegisterThreadForDetachment(JavaVM* const vm) noexcept {
	std::call_once(g_thread_key_once, InitializePthreadKey);
	pthread_setspecific(g_detach_key, reinterpret_cast<const void*>(vm));
}

inline void YieldProcessor() noexcept {
#if defined(__aarch64__) || defined(__arm__)
	asm volatile("yield" ::: "memory");
#else
	std::this_thread::yield();
#endif
}

} // namespace

ScopedAttachmentGuard::ScopedAttachmentGuard(JavaVM* const vm) noexcept
	: vm_(vm), env_(nullptr), must_detach_(false), has_local_frame_(false) {
	if (!vm_) [[unlikely]]
		return;

	void* env_ptr = nullptr;
	const jint res = vm_->GetEnv(&env_ptr, TARGET_JNI_VERSION);
	if (res == JNI_OK) {
		env_ = reinterpret_cast<JNIEnv*>(env_ptr);
	}
	else if (res == JNI_EDETACHED) {
		if (vm_->AttachCurrentThreadAsDaemon(reinterpret_cast<JNIEnv**>(&env_ptr), nullptr) == JNI_OK) {
			env_ = reinterpret_cast<JNIEnv*>(env_ptr);
			must_detach_ = true;
		}
	}

	if (env_) [[likely]] {
		has_local_frame_ = (env_->PushLocalFrame(16) == JNI_OK);
	}
}

ScopedAttachmentGuard::~ScopedAttachmentGuard() noexcept {
	if (env_ && has_local_frame_) [[likely]] {
		env_->PopLocalFrame(nullptr);
	}
	if (vm_ && must_detach_) {
		vm_->DetachCurrentThread();
	}
}

ScopedLocalRef::~ScopedLocalRef() noexcept {
	if (env_ && ref_) [[likely]] {
		env_->DeleteLocalRef(ref_);
	}
}

ScopedLocalRef::ScopedLocalRef(ScopedLocalRef&& other) noexcept : env_(other.env_), ref_(other.ref_) {
	other.ref_ = nullptr;
	other.env_ = nullptr;
}

ScopedLocalRef& ScopedLocalRef::operator=(ScopedLocalRef&& other) noexcept {
	if (this != &other) [[likely]] {
		JNIEnv* const old_env = env_;
		jobject const old_ref = ref_;
		env_ = other.env_;
		ref_ = other.ref_;
		other.ref_ = nullptr;
		other.env_ = nullptr;
		if (old_env && old_ref) [[likely]] {
			old_env->DeleteLocalRef(old_ref);
		}
	}
	return *this;
}

jobject ScopedLocalRef::release() noexcept {
	jobject const retained_ref = ref_;
	ref_ = nullptr;
	env_ = nullptr;
	return retained_ref;
}

bool SetAndroidJNIEnv(JNIEnv* const env) noexcept {
	if (!env) [[unlikely]]
		return false;

	JavaVM* new_vm{nullptr};
	if (env->GetJavaVM(&new_vm) != JNI_OK || !new_vm) [[unlikely]]
		return false;

	jweak old_activity = nullptr;
	JavaVM* old_vm_to_clean = nullptr;
	uint64_t current_epoch = 0;

	{
		std::lock_guard<std::mutex> lock{g_context_mutex};
		JavaVM* const current_vm = g_vm.load(std::memory_order_relaxed);

		if (current_vm == new_vm) {
			current_epoch = g_context_epoch.load(std::memory_order_relaxed);
			t_jni_cache = ThreadJniCache{.env = env, .bound_vm = new_vm, .epoch = current_epoch};
			return true;
		}

		old_activity = g_weak_activity.load(std::memory_order_relaxed);
		if (old_activity && current_vm) {
			old_vm_to_clean = current_vm;
			g_weak_activity.store(static_cast<jweak>(nullptr), std::memory_order_release);
		}
		g_vm.store(new_vm, std::memory_order_release);
		current_epoch = g_context_epoch.fetch_add(1, std::memory_order_release) + 1;
	}

	t_jni_cache = ThreadJniCache{.env = env, .bound_vm = new_vm, .epoch = current_epoch};

	if (old_vm_to_clean && old_activity) {
		while (g_readers_count.load(std::memory_order_acquire) > 0) {
			YieldProcessor();
		}
		ScopedAttachmentGuard old_vm_guard{old_vm_to_clean};
		JNIEnv* const old_env = old_vm_guard.GetEnv();
		if (old_env != nullptr) [[likely]] {
			old_env->DeleteWeakGlobalRef(old_activity);
		}
	}
	return true;
}

bool SetAndroidActivity(jobject const activity) noexcept {
	if (!activity) [[unlikely]]
		return false;

	JavaVM* const vm = g_vm.load(std::memory_order_acquire);
	if (!vm) [[unlikely]]
		return false;

	JNIEnv* const env = GetAndroidJNIEnv();
	if (!env) [[unlikely]]
		return false;

	jweak const new_weak = env->NewWeakGlobalRef(activity);
	if (!new_weak) [[unlikely]]
		return false;

	std::lock_guard<std::mutex> lock{g_context_mutex};
	if (g_vm.load(std::memory_order_acquire) != vm) [[unlikely]] {
		env->DeleteWeakGlobalRef(new_weak);
		return false;
	}

	jweak const old_activity = g_weak_activity.exchange(new_weak, std::memory_order_release);
	if (old_activity) {
		env->DeleteWeakGlobalRef(old_activity);
	}
	return true;
}

void ClearAndroidContext() noexcept {
	JavaVM* vm_to_clean = nullptr;
	jweak old_activity = nullptr;

	{
		std::lock_guard<std::mutex> lock{g_context_mutex};
		vm_to_clean = g_vm.exchange(nullptr, std::memory_order_acq_rel);
		if (!vm_to_clean)
			return;

		old_activity = g_weak_activity.exchange(static_cast<jweak>(nullptr), std::memory_order_release);
		g_context_epoch.fetch_add(1, std::memory_order_release);
	}

	t_jni_cache = ThreadJniCache{};

	if (old_activity) {
		while (g_readers_count.load(std::memory_order_acquire) > 0) {
			YieldProcessor();
		}
		ScopedAttachmentGuard attachment_guard{vm_to_clean};
		JNIEnv* const env = attachment_guard.GetEnv();
		if (env != nullptr) [[likely]] {
			env->DeleteWeakGlobalRef(old_activity);
		}
	}
}

JNIEnv* GetAndroidJNIEnv() noexcept {
	const uint64_t initial_epoch = g_context_epoch.load(std::memory_order_relaxed);

#ifdef __ANDROID__
	if (t_jni_cache.env != nullptr) [[likely]] {
		if (t_jni_cache.epoch == initial_epoch) [[likely]] {
			return t_jni_cache.env;
		}
		std::atomic_thread_fence(std::memory_order_acquire);
	}

	JavaVM* const vm = g_vm.load(std::memory_order_acquire);
	if (!vm) [[unlikely]] {
		t_jni_cache = ThreadJniCache{nullptr, nullptr, initial_epoch};
		return nullptr;
	}

	void* env_ptr{nullptr};
	jint status = vm->GetEnv(&env_ptr, TARGET_JNI_VERSION);

	auto try_cache_env = [vm](void* const ptr, const uint64_t start_epoch) noexcept -> JNIEnv* {
		auto* const env = reinterpret_cast<JNIEnv*>(ptr);
		const uint64_t post_epoch = g_context_epoch.load(std::memory_order_relaxed);
		if (start_epoch == post_epoch) [[likely]] {
			t_jni_cache = ThreadJniCache{.env = env, .bound_vm = vm, .epoch = start_epoch};
		}
		return env;
	};

	if (status == JNI_OK) [[likely]] {
		return try_cache_env(env_ptr, initial_epoch);
	}

	if (status == JNI_EDETACHED) {
		status = vm->AttachCurrentThreadAsDaemon(reinterpret_cast<JNIEnv**>(&env_ptr), nullptr);
		if (status == JNI_OK) [[likely]] {
			RegisterThreadForDetachment(vm);
			return try_cache_env(env_ptr, initial_epoch);
		}
	}
	return nullptr;
#else
	(void)initial_epoch;
	return nullptr;
#endif
}

ScopedLocalRef GetAndroidActivity() noexcept {
#ifdef __ANDROID__
	JNIEnv* const env = GetAndroidJNIEnv();
	if (!env) [[unlikely]] {
		return ScopedLocalRef{};
	}

	ReaderGuard const reader_lifetime_guard;

	jweak const snapshot_weak = g_weak_activity.load(std::memory_order_acquire);
	if (!snapshot_weak) [[unlikely]] {
		return ScopedLocalRef{};
	}

	if (env->IsSameObject(snapshot_weak, nullptr) == JNI_TRUE) {
		return ScopedLocalRef{};
	}

	jobject const raw_local_ref = env->NewLocalRef(snapshot_weak);
	if (!raw_local_ref) [[unlikely]] {
		return ScopedLocalRef{};
	}

	if (env->IsSameObject(raw_local_ref, nullptr) == JNI_TRUE) {
		env->DeleteLocalRef(raw_local_ref);
		return ScopedLocalRef{};
	}

	return ScopedLocalRef{env, raw_local_ref};
#else
	return ScopedLocalRef{};
#endif
}

JavaVM* GetAndroidJavaVM() noexcept {
#ifdef __ANDROID__
	return g_vm.load(std::memory_order_acquire);
#else
	return nullptr;
#endif
}

} // namespace Sral

#endif