#include "AndroidContext.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

#ifdef __ANDROID__
#include <jni.h>
#else
#define JNI_VERSION_1_6 0x00010006
#define JNI_OK 0
#define JNI_EDETACHED (-2)
#define JNI_TRUE 1

struct JNIEnv {
	jint GetJavaVM(struct JavaVM**) noexcept { return JNI_OK; }
	void DeleteLocalRef(jobject) noexcept {}
	void DeleteWeakGlobalRef(jweak) noexcept {}
	jobject NewLocalRef(jweak) noexcept { return nullptr; }
	jboolean IsSameObject(jobject, jobject) noexcept { return static_cast<jboolean>(JNI_TRUE); }
};
struct JavaVM {
	jint GetEnv(void**, jint) noexcept { return JNI_OK; }
	jint AttachCurrentThreadAsDaemon(JNIEnv**, void*) noexcept { return JNI_OK; }
	jint DetachCurrentThread() noexcept { return JNI_OK; }
};
#endif

namespace Sral {

namespace {
std::atomic<JavaVM*> g_vm{nullptr};
std::atomic<jweak> g_weak_activity{nullptr};
std::mutex g_context_mutex;
std::atomic<uint64_t> g_context_epoch{0};
std::atomic<uint32_t> g_readers_count{0};

struct ThreadJniCache {
	JNIEnv* env{nullptr};
	JavaVM* bound_vm{nullptr};
	uint64_t epoch{0};
};

inline thread_local ThreadJniCache t_jni_cache{};

inline thread_local struct ThreadDetacher {
	JavaVM* attached_vm{nullptr};

	~ThreadDetacher() {
		if (attached_vm) [[unlikely]] {
			void* dummy{nullptr};
			if (attached_vm->GetEnv(&dummy, JNI_VERSION_1_6) == JNI_OK) {
				attached_vm->DetachCurrentThread();
			}
		}
	}
} t_thread_detacher;
} // namespace

ScopedLocalRef::~ScopedLocalRef() noexcept {
#ifdef __ANDROID__
	if (env_ && ref_) [[likely]] {
		env_->DeleteLocalRef(ref_);
	}
#endif
}

ScopedLocalRef::ScopedLocalRef(ScopedLocalRef&& other) noexcept : env_(other.env_), ref_(other.ref_) {
	other.ref_ = nullptr;
	other.env_ = nullptr;
}

ScopedLocalRef& ScopedLocalRef::operator=(ScopedLocalRef&& other) noexcept {
	if (this != &other) [[likely]] {
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
	if (!env) [[unlikely]]
		return false;
#ifdef __ANDROID__
	JavaVM* new_vm{nullptr};
	if (env->GetJavaVM(&new_vm) != JNI_OK || !new_vm) [[unlikely]]
		return false;

	jweak old_activity = nullptr;
	JavaVM* old_vm_to_clean = nullptr;
	uint64_t current_epoch = 0;

	{
		std::lock_guard lock{g_context_mutex};
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
#if defined(__aarch64__) || defined(__arm__)
			asm volatile("yield" ::: "memory");
#else
			(void)0;
#endif
		}
		ScopedAttachmentGuard old_vm_guard{old_vm_to_clean};
		JNIEnv* old_env = old_vm_guard.GetEnv();
		if (old_env) [[likely]] {
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
	if (!activity) [[unlikely]]
		return false;
#ifdef __ANDROID__
	JavaVM* vm = g_vm.load(std::memory_order_acquire);
	if (!vm) [[unlikely]]
		return false;

	JNIEnv* env = GetAndroidJNIEnv();
	if (!env) [[unlikely]]
		return false;

	jweak new_weak = env->NewWeakGlobalRef(activity);
	if (!new_weak) [[unlikely]]
		return false;

	std::lock_guard lock{g_context_mutex};
	if (g_vm.load(std::memory_order_acquire) != vm) [[unlikely]] {
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
		std::lock_guard lock{g_context_mutex};
		vm_to_clean = g_vm.exchange(nullptr, std::memory_order_acq_rel);
		if (!vm_to_clean)
			return;

		old_activity = g_weak_activity.exchange(static_cast<jweak>(nullptr), std::memory_order_seq_cst);
		g_context_epoch.fetch_add(1, std::memory_order_release);
	}

	t_jni_cache = ThreadJniCache{};

	if (old_activity) {
		while (g_readers_count.load(std::memory_order_seq_cst) > 0) {
#if defined(__aarch64__) || defined(__arm__)
			asm volatile("yield" ::: "memory");
#else
			(void)0;
#endif
		}

		ScopedAttachmentGuard attachment_guard{vm_to_clean};
		JNIEnv* env = attachment_guard.GetEnv();
		if (env) [[likely]] {
			env->DeleteWeakGlobalRef(old_activity);
		}
	}
}

JNIEnv* GetAndroidJNIEnv() noexcept {
	uint64_t initial_epoch = 0;

	if (t_jni_cache.env != nullptr) [[likely]] {
		initial_epoch = g_context_epoch.load(std::memory_order_relaxed);
		if (t_jni_cache.epoch == initial_epoch) [[likely]] {
			return t_jni_cache.env;
		}
		std::atomic_thread_fence(std::memory_order_acquire);
	}

	JavaVM* vm = g_vm.load(std::memory_order_acquire);
	if (!vm) [[unlikely]] {
		t_jni_cache = ThreadJniCache{nullptr, nullptr, g_context_epoch.load(std::memory_order_relaxed)};
		return nullptr;
	}

	initial_epoch = g_context_epoch.load(std::memory_order_relaxed);

#ifdef __ANDROID__
	JavaVM* vm_verify = g_vm.load(std::memory_order_relaxed);
	if (vm_verify != vm) [[unlikely]] {
		t_jni_cache = ThreadJniCache{};
		return nullptr;
	}

	void* env_ptr{nullptr};
	jint status = vm->GetEnv(&env_ptr, JNI_VERSION_1_6);

	auto try_cache_env = [initial_epoch, vm, cache_ptr = &t_jni_cache](void* ptr) mutable noexcept -> JNIEnv* {
		auto* env = reinterpret_cast<JNIEnv*>(ptr);
		const uint64_t post_epoch = g_context_epoch.load(std::memory_order_relaxed);
		if (initial_epoch == post_epoch) [[likely]] {
			*cache_ptr = ThreadJniCache{.env = env, .bound_vm = vm, .epoch = initial_epoch};
		}
		return env;
	};

	if (status == JNI_OK) [[likely]] {
		return try_cache_env(env_ptr);
	}

	if (status == JNI_EDETACHED) {
		status = vm->AttachCurrentThreadAsDaemon(reinterpret_cast<JNIEnv**>(&env_ptr), nullptr);
		if (status == JNI_OK) [[likely]] {
			t_thread_detacher.attached_vm = vm;
			return try_cache_env(env_ptr);
		}
	}
#endif
	return nullptr;
}

ScopedLocalRef GetAndroidActivity() noexcept {
	JNIEnv* env = GetAndroidJNIEnv();
	if (!env) [[unlikely]]
		return ScopedLocalRef{};

#ifdef __ANDROID__
	g_readers_count.fetch_add(1, std::memory_order_seq_cst);
	jweak snapshot_weak = g_weak_activity.load(std::memory_order_seq_cst);

	if (!snapshot_weak) [[unlikely]] {
		g_readers_count.fetch_sub(1, std::memory_order_seq_cst);
		return ScopedLocalRef{};
	}

	jweak revalidate_weak = g_weak_activity.load(std::memory_order_relaxed);
	if (snapshot_weak != revalidate_weak) [[unlikely]] {
		g_readers_count.fetch_sub(1, std::memory_order_seq_cst);
		return ScopedLocalRef{};
	}

	jobject raw_local_ref = env->NewLocalRef(snapshot_weak);
	g_readers_count.fetch_sub(1, std::memory_order_release);

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
