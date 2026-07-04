#if defined(__ANDROID__)
#include "AndroidContext.h"

#include <atomic>
#include <concepts>
#include <jni.h>
#include <memory>
#include <mutex>
#include <pthread.h>

namespace Sral {
namespace {

std::atomic<JavaVM*> g_vm{nullptr};
jweak g_weak_activity{nullptr};
std::mutex g_context_mutex;
pthread_key_t g_detach_key;
std::once_flag g_key_once_flag;
std::atomic<bool> g_key_initialized{false};

} // namespace

class [[nodiscard]] ScopedLocalRef final {
public:
	explicit ScopedLocalRef() noexcept : env_(nullptr), ref_(nullptr) {}
	explicit ScopedLocalRef(JNIEnv* env, jobject ref) noexcept : env_(env), ref_(ref) {}

	~ScopedLocalRef() noexcept {
		if (env_ && ref_) [[likely]] {
			env_->DeleteLocalRef(ref_);
		}
	}

	ScopedLocalRef(const ScopedLocalRef&) = delete;
	ScopedLocalRef& operator=(const ScopedLocalRef&) = delete;

	ScopedLocalRef(ScopedLocalRef&& other) noexcept : env_(other.env_), ref_(other.ref_) {
		other.ref_ = nullptr;
		other.env_ = nullptr;
	}

	ScopedLocalRef& operator=(ScopedLocalRef&& other) noexcept {
		if (this != &other) [[likely]] {
			JNIEnv* old_env = env_;
			jobject old_ref = ref_;

			env_ = other.env_;
			ref_ = other.ref_;

			other.ref_ = nullptr;
			other.env_ = nullptr;

			if (old_env && old_ref) {
				old_env->DeleteLocalRef(old_ref);
			}
		}
		return *this;
	}

	[[nodiscard]] jobject get() const noexcept { return ref_; }

	jobject release() noexcept {
		jobject tmp = ref_;
		ref_ = nullptr;
		env_ = nullptr;
		return tmp;
	}

	explicit operator bool() const noexcept { return ref_ != nullptr; }

private:
	JNIEnv* env_;
	jobject ref_;
};

namespace {

void PthreadThreadDetacher(void* value) noexcept {
	if (!value) [[unlikely]]
		return;

	auto* vm = static_cast<JavaVM*>(value);
	vm->DetachCurrentThread();
}

void InitializePthreadKeyOnce() noexcept {
	if (pthread_key_create(&g_detach_key, PthreadThreadDetacher) == 0) {
		g_key_initialized.store(true, std::memory_order::release);
	}
}

bool EnforceThreadTokenRegistration(JavaVM* vm) noexcept {
	std::call_once(g_key_once_flag, InitializePthreadKeyOnce);
	if (!g_key_initialized.load(std::memory_order::acquire)) [[unlikely]] {
		return false;
	}

	if (pthread_getspecific(g_detach_key) != nullptr) [[likely]] {
		return true;
	}

	if (pthread_setspecific(g_detach_key, vm) == 0) {
		return true;
	}

	return false;
}

class [[nodiscard]] ScopedCleanupAttachmentGuard final {
public:
	explicit ScopedCleanupAttachmentGuard(JavaVM* vm) noexcept : vm_(vm), env_(nullptr), must_detach_(false) {
		if (!vm_) [[unlikely]]
			return;

		jint status = vm_->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6);
		if (status == JNI_EDETACHED) {
			if (vm_->AttachCurrentThread(&env_, nullptr) == JNI_OK) {
				must_detach_ = true;
			}
		}
	}

	~ScopedCleanupAttachmentGuard() noexcept {
		if (must_detach_ && vm_) [[likely]] {
			vm_->DetachCurrentThread();
		}
	}

	ScopedCleanupAttachmentGuard(const ScopedCleanupAttachmentGuard&) = delete;
	ScopedCleanupAttachmentGuard& operator=(const ScopedCleanupAttachmentGuard&) = delete;
	ScopedCleanupAttachmentGuard(ScopedCleanupAttachmentGuard&&) noexcept = delete;
	ScopedCleanupAttachmentGuard& operator=(ScopedCleanupAttachmentGuard&&) noexcept = delete;

	[[nodiscard]] JNIEnv* GetEnv() const noexcept { return env_; }

private:
	JavaVM* vm_;
	JNIEnv* env_;
	bool must_detach_;
};

} // namespace

bool SetAndroidJNIEnv(JNIEnv* env) noexcept {
	if (!env) [[unlikely]]
		return false;

	JavaVM* new_vm{nullptr};
	if (env->GetJavaVM(&new_vm) != JNI_OK || !new_vm) [[unlikely]] {
		return false;
	}

	JavaVM* current_vm = g_vm.load(std::memory_order::acquire);
	if (current_vm == new_vm) [[likely]] {
		return true;
	}

	bool old_vm_detach_required = false;

	JNIEnv* old_env = nullptr;
	if (current_vm) {
		jint status = current_vm->GetEnv(reinterpret_cast<void**>(&old_env), JNI_VERSION_1_6);
		if (status == JNI_EDETACHED) {
			if (current_vm->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&old_env), nullptr) == JNI_OK) {
				old_vm_detach_required = true;
			}
		}
	}

	{
		std::lock_guard lock{g_context_mutex};

		JavaVM* rechecked_vm = g_vm.load(std::memory_order::relaxed);
		if (rechecked_vm == new_vm) {
			if (old_vm_detach_required && current_vm) {
				current_vm->DetachCurrentThread();
			}
			return true;
		}

		if (g_weak_activity) {
			if (rechecked_vm == current_vm && old_env) {
				old_env->DeleteWeakGlobalRef(g_weak_activity);
			}
			g_weak_activity = nullptr;
		}

		g_vm.store(new_vm, std::memory_order::release);
	}

	if (old_vm_detach_required && current_vm) {
		current_vm->DetachCurrentThread();
	}

	return true;
}

bool SetAndroidActivity(jobject activity) noexcept {
	if (!activity) [[unlikely]]
		return false;

	JavaVM* vm = g_vm.load(std::memory_order::acquire);
	if (!vm)
		return false;

	JNIEnv* env = GetAndroidJNIEnv();
	if (!env) [[unlikely]]
		return false;

	jobject raw_local = env->NewLocalRef(activity);
	if (!raw_local) [[unlikely]]
		return false;

	ScopedLocalRef local_activity{env, raw_local};

	std::lock_guard lock{g_context_mutex};
	if (g_vm.load(std::memory_order::acquire) != vm) {
		return false;
	}

	if (g_weak_activity) {
		env->DeleteWeakGlobalRef(g_weak_activity);
		g_weak_activity = nullptr;
	}

	g_weak_activity = env->NewWeakGlobalRef(local_activity.get());
	return g_weak_activity != nullptr;
}

void ClearAndroidContext() noexcept {
	JavaVM* vm = g_vm.exchange(nullptr, std::memory_order::acq_rel);
	if (!vm)
		return;

	if (g_key_initialized.load(std::memory_order::acquire)) {
		pthread_setspecific(g_detach_key, nullptr);
	}

	{
		ScopedCleanupAttachmentGuard attachment_guard{vm};
		std::lock_guard lock{g_context_mutex};
		JNIEnv* env = attachment_guard.GetEnv();

		if (env && g_weak_activity) {
			env->DeleteWeakGlobalRef(g_weak_activity);
		}
		g_weak_activity = nullptr;
	}
}

JNIEnv* GetAndroidJNIEnv() noexcept {
	JavaVM* vm = g_vm.load(std::memory_order::acquire);
	if (!vm) [[unlikely]]
		return nullptr;

	JNIEnv* env{nullptr};
	jint status = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);

	if (status == JNI_OK) [[likely]] {
		if (EnforceThreadTokenRegistration(vm)) [[likely]] {
			if (g_vm.load(std::memory_order::acquire) != nullptr) [[likely]] {
				return env;
			}
		}
		else {
			return nullptr;
		}
	}

	if (status == JNI_EDETACHED || status != JNI_OK) {
		bool must_detach = false;
		if (status == JNI_EDETACHED) {
			status = vm->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&env), nullptr);
			if (status == JNI_OK) [[likely]] {
				must_detach = true;
				if (EnforceThreadTokenRegistration(vm)) [[likely]] {
					if (g_vm.load(std::memory_order::acquire) != nullptr) [[likely]] {
						return env;
					}
				}
			}
		}

		if (must_detach) {
			if (g_key_initialized.load(std::memory_order::acquire)) {
				pthread_setspecific(g_detach_key, nullptr);
			}
			vm->DetachCurrentThread();
		}
	}
	return nullptr;
}

ScopedLocalRef GetAndroidActivity() noexcept {
	JNIEnv* env = GetAndroidJNIEnv();
	if (!env) [[unlikely]]
		return ScopedLocalRef{};

	std::lock_guard lock{g_context_mutex};

	if (g_vm.load(std::memory_order::acquire) == nullptr) {
		return ScopedLocalRef{};
	}

	jweak current_weak_activity = g_weak_activity;
	if (!current_weak_activity) {
		return ScopedLocalRef{};
	}

	jobject raw_local_ref = env->NewLocalRef(current_weak_activity);
	if (!raw_local_ref) {
		return ScopedLocalRef{};
	}

	if (env->IsSameObject(raw_local_ref, nullptr) == JNI_TRUE) {
		env->DeleteLocalRef(raw_local_ref);
		return ScopedLocalRef{};
	}

	return ScopedLocalRef{env, raw_local_ref};
}

} // namespace Sral
#endif // defined(__ANDROID__)
