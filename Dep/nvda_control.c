#define WIN32_LEAN_AND_MEAN

#include "nvda_control.h"

#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <winerror.h>

#if defined(_MSC_VER) && !defined(__clang__)
    #include <intrin.h>
    typedef volatile long           bs_atomic_int;
    typedef volatile char           bs_atomic_bool;
    #define bs_atomic_load_int(ptr)        (*(ptr))
    #define bs_atomic_load_bool(ptr)       (*(ptr))
    #define bs_atomic_store_int(ptr, val)  _InterlockedExchange((volatile long*)(ptr), (long)(val))
    #define bs_atomic_store_bool(ptr, val) _InterlockedExchange8((volatile char*)(ptr), (char)(val))
#else
    #include <stdatomic.h>
    typedef _Atomic int    bs_atomic_int;
    typedef _Atomic bool   bs_atomic_bool;

    #define bs_atomic_load_int(ptr)        atomic_load_explicit((ptr), memory_order_acquire)
    #define bs_atomic_load_bool(ptr)       atomic_load_explicit((ptr), memory_order_acquire)
    #define bs_atomic_store_int(ptr, val)  atomic_store_explicit((ptr), (val), memory_order_release)
    #define bs_atomic_store_bool(ptr, val) atomic_store_explicit((ptr), (val), memory_order_release)
#endif

enum {
    MAX_COMMAND_LEN  = 2048,
    SLOT_EMPTY       = 0,
    SLOT_BUSY        = 1,
    SLOT_READY       = 2
};

#if defined(__GNUC__) || defined(__clang__)
    #define BS_HOT __attribute__((hot))
    #define BS_NOINLINE __attribute__((noinline))
    #define BS_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define BS_LIKELY(x) __builtin_expect(!!(x), 1)
#else
    #define BS_HOT
    #define BS_NOINLINE __declspec(noinline)
    #define BS_UNLIKELY(x) (x)
    #define BS_LIKELY(x) (x)
#endif


typedef struct {
	char data[MAX_COMMAND_LEN];
	bs_atomic_int state;
} NvdaSingleSlotChannel;

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(NvdaSingleSlotChannel) == 2052, "Structure memory layout must equal exactly 2052 bytes.");
#endif

alignas(64) static NvdaSingleSlotChannel g_Channel = { .state = SLOT_EMPTY };

static HANDLE g_hWorkerThread = NULL;
static HANDLE g_hWorkEvent = NULL;
static bs_atomic_bool g_WorkerRunning = false;
static HANDLE g_hNvdaPipe = INVALID_HANDLE_VALUE;

static inline bool is_handle_valid(HANDLE h) {
	return (h != INVALID_HANDLE_VALUE && h != NULL);
}

BS_NOINLINE
static void escape_and_format_direct(char* restrict const dest,
	const char* restrict const prefix,
	const char* restrict const text,
	const char* restrict const suffix) {
	size_t j = 0;

	if (prefix) {
		const size_t len = strlen(prefix);
		if (len < MAX_COMMAND_LEN - 1) {
			memcpy(dest, prefix, len);
			j += len;
		}
	}
	if (text) {
		for (size_t i = 0; text[i] != '\0'; i++) {
			if (BS_UNLIKELY(j >= (MAX_COMMAND_LEN - 3)))
				break;
			const char c = text[i];
			if (c == '"' || c == '\\') {
				dest[j++] = '\\';
			}
			dest[j++] = c;
		}
	}
	if (suffix) {
		const size_t len = strlen(suffix);
		if (j + len < MAX_COMMAND_LEN - 1) {
			memcpy(dest + j, suffix, len);
			j += len;
		}
	}
	dest[j] = '\0';
}

static int nvda_connect_internal(void) {
	if (is_handle_valid(g_hNvdaPipe)) {
		CloseHandle(g_hNvdaPipe);
	}
	g_hNvdaPipe = INVALID_HANDLE_VALUE;

	HANDLE h_new = CreateFileW(NVDA_PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (!is_handle_valid(h_new)) {
		return -1;
	}

	g_hNvdaPipe = h_new;
	return 0;
}

static void nvda_process_pipe_command(const char* const command) {
	const size_t command_len = strlen(command);
	if (BS_UNLIKELY(command_len == 0 || command_len > UINT_MAX))
		return;

	for (int attempt = 0; attempt <= 2; attempt++) {
		if (!is_handle_valid(g_hNvdaPipe)) {
			if (nvda_connect_internal() == -1) {
				Sleep(5);
				continue;
			}
		}

		DWORD bytesWritten = 0;
		if (WriteFile(g_hNvdaPipe, command, (DWORD)command_len, &bytesWritten, NULL) && bytesWritten == command_len) {
			return;
		}

		CloseHandle(g_hNvdaPipe);
		g_hNvdaPipe = INVALID_HANDLE_VALUE;
		Sleep(5);
	}
}

/*
    THE RUNTIME CONSUMER WORKER LOOP:
    Safely consumes speech updates utilizing a Single-Fetch Verification strategy
    to permanently protect against teardown phase data drops.
*/
BS_HOT
static DWORD WINAPI nvda_worker_thread_proc(LPVOID lpParam) {
	(void)lpParam;

	while (true) {
#if defined(_MSC_VER) && !defined(__clang__)
		_ReadWriteBarrier(); // Enforce strict C17 visual ordering fences
		int current_state = (int)bs_atomic_load_int(&g_Channel.state);
#else
		int current_state = atomic_load_explicit(&g_Channel.state, memory_order_acquire);
#endif

		if (current_state != SLOT_READY) {
#if defined(_MSC_VER) && !defined(__clang__)
			if (!bs_atomic_load_bool(&g_WorkerRunning)) {
				_ReadWriteBarrier();
				// Catches final incoming packets at shutdown threshold branchlessly
				if ((int)bs_atomic_load_int(&g_Channel.state) != SLOT_READY) {
					break;
				}
				continue;
			}
#else
			if (!atomic_load_explicit(&g_WorkerRunning, memory_order_acquire)) {
				if (atomic_load_explicit(&g_Channel.state, memory_order_acquire) != SLOT_READY) {
					break;
				}
				continue;
			}
#endif
			if (WaitForSingleObject(g_hWorkEvent, 10) == WAIT_TIMEOUT) {
				continue;
			}
			continue;
		}

		nvda_process_pipe_command(g_Channel.data);
		bs_atomic_store_int(&g_Channel.state, SLOT_EMPTY);
	}

	if (is_handle_valid(g_hNvdaPipe)) {
		CloseHandle(g_hNvdaPipe);
		g_hNvdaPipe = INVALID_HANDLE_VALUE;
	}
	return 0;
}
