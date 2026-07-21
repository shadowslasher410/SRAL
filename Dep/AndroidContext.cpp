#include "AndroidContext.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#ifdef __ANDROID__
#include <android/log.h>
#include <pthread.h>
#define LOG_TAG "SRAL_AndroidContext"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <iostream>
#define LOGE(...) std::cerr << "[SRAL Error] " << __VA_ARGS__ << "\n"
#endif

#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3))
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

namespace Sral {
namespace {

std::atomic<JavaVM*> g_vm{nullptr};
std::atomic<jweak> g_weak_activity{nullptr};
std::mutex g_context_mutex;
std::atomic<uint64_t> g_context_epoch{0};
std::atomic<uint32_t> g_readers_count{0};

#ifdef __ANDROID__
constexpr jint TARGET_JNI_VERSION = JNI_VERSION_1_6;
#else
constexpr jint TARGET_JNI_VERSION = 0x00010006;
constexpr jint JNI_OK = 0;
constexpr jint JNI_EDETACHED = -2;
constexpr jboolean JNI_TRUE = 1;
#endif

struct ThreadJniCache {
	JNIEnv* env{nullptr};
	JavaVM* bound_vm{nullptr};
	uint64_t epoch{0};
};

inline thread_local ThreadJniCache t_jni_cache{};

struct ReaderGuard {
	ReaderGuard() noexcept { g_readers_count.fetch_add(1, std::memory_order_seq_cst); }
	~ReaderGuard() noexcept { g_readers_count.fetch_sub(1, std::memory_order_seq_cst); }
	ReaderGuard(const ReaderGuard&) = delete;
	ReaderGuard& operator=(const ReaderGuard&) = delete;
};

#ifdef __ANDROID__
pthread_key_t g_detach_key;
std::once_flag g_thread_key_once;

void PthreadThreadDetacher(void* value) {
	if (value) {
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
#endif

inline void RegisterThreadForDetachment(JavaVM* vm) noexcept {
#ifdef __ANDROID__
	std::call_once(g_thread_key_once, InitializePthreadKey);
	pthread_setspecific(g_detach_key, reinterpret_cast<const void*>(vm));
#else
	(void)vm;
#endif
}

inline void YieldProcessor() noexcept {
#if defined(__aarch64__) || defined(__arm__)
	asm volatile("yield" ::: "memory");
#else
	std::this_thread::yield();
#endif
}

} // namespace

ScopedAttachmentGuard::ScopedAttachmentGuard(JavaVM* vm) noexcept
	: vm_(vm), env_(nullptr), must_detach_(false), has_local_frame_(false) {
	if (!vm_)
		return;

	void* env_ptr = nullptr;
	jint res = vm_->GetEnv(&env_ptr, TARGET_JNI_VERSION);
	if (res == JNI_OK) {
		env_ = reinterpret_cast<JNIEnv*>(env_ptr);
	}
	else if (res == JNI_EDETACHED) {
		if (vm_->AttachCurrentThreadAsDaemon(reinterpret_cast<JNIEnv**>(&env_ptr), nullptr) == JNI_OK) {
			env_ = reinterpret_cast<JNIEnv*>(env_ptr);
			must_detach_ = true;
		}
	}

	if (env_) {
		has_local_frame_ = (env_->PushLocalFrame(16) == JNI_OK);
	}
}

ScopedAttachmentGuard::~ScopedAttachmentGuard() noexcept {
	if (env_ && has_local_frame_) {
		env_->PopLocalFrame(nullptr);
	}
	if (vm_ && must_detach_) {
		vm_->DetachCurrentThread();
	}
}

ScopedLocalRef::~ScopedLocalRef() noexcept {
#ifdef __ANDROID__
	if (LIKELY(env_ && ref_)) {
		env_->DeleteLocalRef(ref_);
	}
#endif
}

ScopedLocalRef::ScopedLocalRef(ScopedLocalRef&& other) noexcept : env_(other.env_), ref_(other.ref_) {
	other.ref_ = nullptr;
	other.env_ = nullptr;
}

ScopedLocalRef& ScopedLocalRef::operator=(ScopedLocalRef&& other) noexcept {
	if (LIKELY(this != &other)) {
		JNIEnv* old_env = env_;
		jobject old_ref = ref_;
		env_ = other.env_;
		ref_ = other.ref_;
		other.ref_ = nullptr;
		other.env_ = nullptr;
#ifdef __ANDROID__
		if (old_env && old_ref) {
			old_env->DeleteLocalRef(old_ref);
		}
#else
		(void)old_env;
		(void)old_ref;
#endif
	}
	return *this;
}

jobject ScopedLocalRef::release() noexcept {
	jobject retained_ref = ref_;
	ref_ = nullptr;
	env_ = nullptr;
	return retained_ref;
}

bool SetAndroidJNIEnv(JNIEnv* env) noexcept {
	if (UNLIKELY(!env))
		return false;
#ifdef __ANDROID__
	JavaVM* new_vm{nullptr};
	if (UNLIKELY(env->GetJavaVM(&new_vm) != JNI_OK || !new_vm))
		return false;

	jweak old_activity = nullptr;
	JavaVM* old_vm_to_clean = nullptr;
	uint64_t current_epoch = 0;

	{
		std::lock_guard<std::mutex> lock{g_context_mutex};
		JavaVM* current_vm = g_vm.load(std::memory_order_relaxed);

		if (current_vm == new_vm) {
			current_epoch = g_context_epoch.load(std::memory_order_relaxed);
			t_jni_cache = ThreadJniCache{.env = env, .bound_vm = new_vm, .epoch = current_epoch};
			return true;
		}

		old_activity = g_weak_activity.load(std::memory_order_relaxed);
		if (old_activity && current_vm) {
			old_vm_to_clean = current_vm;
			g_weak_activity.store(static_cast<jweak>(nullptr), std::memory_order_seq_cst);
		}
		g_vm.store(new_vm, std::memory_order_release);
		current_epoch = g_context_epoch.fetch_add(1, std::memory_order_release) + 1;
	}

	t_jni_cache = ThreadJniCache{.env = env, .bound_vm = new_vm, .epoch = current_epoch};

	if (old_vm_to_clean && old_activity) {
		while (g_readers_count.load(std::memory_order_seq_cst) > 0) {
			YieldProcessor();
		}
		ScopedAttachmentGuard old_vm_guard{old_vm_to_clean};
		JNIEnv* old_env = old_vm_guard.GetEnv();
		if (LIKELY(old_env != nullptr)) {
			old_env->DeleteWeakGlobalRef(old_activity);
		}
	}
	return true;
#else
	(void)env;
	return false;
#endif
}

bool SetAndroidActivity(jobject activity) noexcept {
	if (UNLIKELY(!activity))
		return false;
#ifdef __ANDROID__
	JavaVM* vm = g_vm.load(std::memory_order_acquire);
	if (UNLIKELY(!vm))
		return false;

	JNIEnv* env = GetAndroidJNIEnv();
	if (UNLIKELY(!env))
		return false;

	jweak new_weak = env->NewWeakGlobalRef(activity);
	if (UNLIKELY(!new_weak))
		return false;

	std::lock_guard<std::mutex> lock{g_context_mutex};
	if (UNLIKELY(g_vm.load(std::memory_order_acquire) != vm)) {
		env->DeleteWeakGlobalRef(new_weak);
		return false;
	}

	jweak old_activity = g_weak_activity.exchange(new_weak, std::memory_order_release);
	if (old_activity) {
		env->DeleteWeakGlobalRef(old_activity);
	}
	return true;
#else
	(void)activity;
	return false;
#endif
}

void ClearAndroidContext() noexcept {
	JavaVM* vm_to_clean = nullptr;
	jweak old_activity = nullptr;

	{
		std::lock_guard<std::mutex> lock{g_context_mutex};
		vm_to_clean = g_vm.exchange(nullptr, std::memory_order_acq_rel);
		if (!vm_to_clean)
			return;

		old_activity = g_weak_activity.exchange(static_cast<jweak>(nullptr), std::memory_order_seq_cst);
		g_context_epoch.fetch_add(1, std::memory_order_release);
	}

	t_jni_cache = ThreadJniCache{};

	if (old_activity) {
		while (g_readers_count.load(std::memory_order_seq_cst) > 0) {
			YieldProcessor();
		}
		ScopedAttachmentGuard attachment_guard{vm_to_clean};
		JNIEnv* env = attachment_guard.GetEnv();
		if (LIKELY(env != nullptr)) {
			env->DeleteWeakGlobalRef(old_activity);
		}
	}
}

JNIEnv* GetAndroidJNIEnv() noexcept {
	const uint64_t initial_epoch = g_context_epoch.load(std::memory_order_relaxed);

	if (LIKELY(t_jni_cache.env != nullptr)) {
		if (LIKELY(t_jni_cache.epoch == initial_epoch)) {
			return t_jni_cache.env;
		}
		std::atomic_thread_fence(std::memory_order_acquire);
	}

	JavaVM* vm = g_vm.load(std::memory_order_acquire);
	if (UNLIKELY(!vm)) {
		t_jni_cache = ThreadJniCache{nullptr, nullptr, initial_epoch};
		return nullptr;
	}

#ifdef __ANDROID__
	void* env_ptr{nullptr};
	jint status = vm->GetEnv(&env_ptr, TARGET_JNI_VERSION);

	auto try_cache_env = [vm](void* ptr, uint64_t start_epoch) mutable noexcept -> JNIEnv* {
		auto* env = reinterpret_cast<JNIEnv*>(ptr);
		const uint64_t post_epoch = g_context_epoch.load(std::memory_order_relaxed);
		if (LIKELY(start_epoch == post_epoch)) {
			t_jni_cache = ThreadJniCache{.env = env, .bound_vm = vm, .epoch = start_epoch};
		}
		return env;
	};

	if (LIKELY(status == JNI_OK)) {
		return try_cache_env(env_ptr, initial_epoch);
	}

	if (status == JNI_EDETACHED) {
		status = vm->AttachCurrentThreadAsDaemon(reinterpret_cast<JNIEnv**>(&env_ptr), nullptr);
		if (LIKELY(status == JNI_OK)) {
			RegisterThreadForDetachment(vm);
			return try_cache_env(env_ptr, initial_epoch);
		}
	}
#endif
	return nullptr;
}

ScopedLocalRef GetAndroidActivity() noexcept {
	JNIEnv* env = GetAndroidJNIEnv();
	if (UNLIKELY(!env))
		return ScopedLocalRef{};

#ifdef __ANDROID__
	ReaderGuard reader_lifetime_guard;

	jweak snapshot_weak = g_weak_activity.load(std::memory_order_seq_cst);
	if (UNLIKELY(!snapshot_weak)) {
		return ScopedLocalRef{};
	}

	if (env->IsSameObject(snapshot_weak, static_cast<jobject>(nullptr)) == JNI_TRUE) {
		return ScopedLocalRef{};
	}

	jobject raw_local_ref = env->NewLocalRef(snapshot_weak);
	if (!raw_local_ref)
		return ScopedLocalRef{};

	if (env->IsSameObject(raw_local_ref, static_cast<jobject>(nullptr)) == JNI_TRUE) {
		env->DeleteLocalRef(raw_local_ref);
		return ScopedLocalRef{};
	}

	return ScopedLocalRef{env, raw_local_ref};
#else
	return ScopedLocalRef{};
#endif
}

JavaVM* GetAndroidJavaVM() noexcept {
	return g_vm.load(std::memory_order_acquire);
}

} // namespace Sral
