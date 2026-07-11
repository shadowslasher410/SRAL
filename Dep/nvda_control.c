#include "nvda_control.h"
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdalign.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RING_BUFFER_SIZE    128
#define RING_BUFFER_MASK    (RING_BUFFER_SIZE - 1)
#define MAX_COMMAND_LEN     2048

#if defined(__GNUC__) || defined(__clang__)
    #define BS_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define BS_LIKELY(x)   __builtin_expect(!!(x), 1)
#else
    #define BS_UNLIKELY(x) (x)
    #define BS_LIKELY(x)   (x)
#endif

#define SLOT_EMPTY   0
#define SLOT_BUSY    1
#define SLOT_READY   2

typedef struct {
    char data[MAX_COMMAND_LEN];
    alignas(64) _Atomic int state;
} NvdaCommandPacket;

alignas(64) static NvdaCommandPacket g_RingBuffer[RING_BUFFER_SIZE];
alignas(64) static _Atomic size_t g_RingHead = 0;
alignas(64) static _Atomic size_t g_RingTail = 0;

static HANDLE g_hWorkerThread = NULL;
static HANDLE g_hWorkEvent = NULL;
static _Atomic bool g_WorkerRunning = false;
static HANDLE g_hNvdaPipe = INVALID_HANDLE_VALUE;

static inline bool is_handle_valid(HANDLE h) {
    return (h != INVALID_HANDLE_VALUE && h != NULL);
}

static inline void escape_and_format_direct(
    char* restrict const dest,
    const char* restrict const prefix,
    const char* restrict const text,
    const char* restrict const suffix)
{
    size_t j = 0;
    if (prefix) {
        const size_t len = strlen(prefix);
        memcpy(dest + j, prefix, len);
        j += len;
    }
    if (text) {
        for (size_t i = 0; text[i] != '\0' && j < (MAX_COMMAND_LEN - 5); i++) {
            const char c = text[i];
            if (c == '"' || c == '\\') {
                dest[j++] = '\\';
                dest[j++] = c;
            } else {
                dest[j++] = c;
            }
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
    if (BS_UNLIKELY(command_len == 0 || command_len > UINT_MAX)) return;

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

static DWORD WINAPI nvda_worker_thread_proc(LPVOID lpParam) {
    (void)lpParam;
    
    while (true) {
        size_t head = atomic_load_explicit(&g_RingHead, memory_order_relaxed);
        size_t slot = head & RING_BUFFER_MASK;
        int current_state = atomic_load_explicit(&g_RingBuffer[slot].state, memory_order_acquire);

        if (current_state != SLOT_READY) {
            if (!atomic_load_explicit(&g_WorkerRunning, memory_order_acquire)) {
                size_t tail = atomic_load_explicit(&g_RingTail, memory_order_relaxed);
                if (head == tail) {
                    break;
                }
            }
            if (WaitForSingleObject(g_hWorkEvent, 10) == WAIT_TIMEOUT) {
                continue;
            }
            continue;
        }
        
        nvda_process_pipe_command(g_RingBuffer[slot].data);
        atomic_store_explicit(&g_RingBuffer[slot].state, SLOT_EMPTY, memory_order_release);
        atomic_store_explicit(&g_RingHead, head + 1, memory_order_release);
    }

    if (is_handle_valid(g_hNvdaPipe)) {
        CloseHandle(g_hNvdaPipe);
        g_hNvdaPipe = INVALID_HANDLE_VALUE;
    }
    return 0;
}

static int ring_buffer_push(const char* prefix, const char* text, const char* suffix) {
    if (BS_UNLIKELY(!atomic_load_explicit(&g_WorkerRunning, memory_order_relaxed))) {
        return -1; 
    }

    size_t tail = atomic_load_explicit(&g_RingTail, memory_order_relaxed);
    size_t slot;
    while (true) {
        size_t head = atomic_load_explicit(&g_RingHead, memory_order_acquire);
        if (BS_UNLIKELY((tail - head) >= RING_BUFFER_SIZE)) {
            return -1; 
        }

        slot = tail & RING_BUFFER_MASK;
        int expected_empty = SLOT_EMPTY; 
        if (atomic_compare_exchange_strong_explicit(&g_RingBuffer[slot].state, &expected_empty, SLOT_BUSY, memory_order_acquire, memory_order_relaxed)) {
            atomic_fetch_add_explicit(&g_RingTail, 1, memory_order_release);
            break;
        }
        tail = atomic_load_explicit(&g_RingTail, memory_order_relaxed);
    }

    escape_and_format_direct(g_RingBuffer[slot].data, prefix, text, suffix);
    atomic_store_explicit(&g_RingBuffer[slot].state, SLOT_READY, memory_order_release);
    
    SetEvent(g_hWorkEvent);
    return 0;
}

int nvda_connect(void) {
    if (atomic_load_explicit(&g_WorkerRunning, memory_order_relaxed)) {
        return 0;
    }

    atomic_store_explicit(&g_RingHead, 0, memory_order_release);
    atomic_store_explicit(&g_RingTail, 0, memory_order_release);
    
    for (int i = 0; i < RING_BUFFER_SIZE; i++) {
        atomic_store_explicit(&g_RingBuffer[i].state, SLOT_EMPTY, memory_order_release);
    }
    
    atomic_store_explicit(&g_WorkerRunning, true, memory_order_release);

    g_hWorkEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!g_hWorkEvent) {
        atomic_store_explicit(&g_WorkerRunning, false, memory_order_relaxed);
        return -1;
    }

    g_hWorkerThread = CreateThread(NULL, 0, nvda_worker_thread_proc, NULL, 0, NULL);
    if (!g_hWorkerThread) {
        CloseHandle(g_hWorkEvent);
        g_hWorkEvent = NULL;
        atomic_store_explicit(&g_WorkerRunning, false, memory_order_relaxed);
        return -1;
    }

    SetThreadPriority(g_hWorkerThread, THREAD_PRIORITY_ABOVE_NORMAL);
    return 0;
}

void nvda_disconnect(void) {
    if (!atomic_load_explicit(&g_WorkerRunning, memory_order_relaxed)) return;
    
    atomic_store_explicit(&g_WorkerRunning, false, memory_order_release);
    SetEvent(g_hWorkEvent);

    if (g_hWorkerThread) {
        WaitForSingleObject(g_hWorkerThread, INFINITE);
        CloseHandle(g_hWorkerThread);
        g_hWorkerThread = NULL;
    }

    if (g_hWorkEvent) {
        CloseHandle(g_hWorkEvent);
        g_hWorkEvent = NULL;
    }
}

int nvda_send_command(const char* command) {
    return ring_buffer_push(NULL, command, NULL);
}

int nvda_speak(const char* text, int symbol_level) {
    char suffix[64];
    snprintf(suffix, sizeof(suffix), "\" 0 %d", symbol_level);
    return ring_buffer_push("speak \"", text, suffix);
}

int nvda_speak_spelling(const char* text, const char* locale, int use_character_descriptions) {
    char suffix[128];
    snprintf(suffix, sizeof(suffix), "\" \"%s\" %d", locale ? locale : "", use_character_descriptions);
    return ring_buffer_push("speakSpelling \"", text, suffix);
}

int nvda_speak_ssml(const char* ssml, int symbol_level) {
    char suffix[64];
    snprintf(suffix, sizeof(suffix), "\" 0 %d", symbol_level);
    return ring_buffer_push("speakSsml \"", ssml, suffix);
}

int nvda_pause_speech(int pause) {
    char command[64];
    snprintf(command, sizeof(command), "pauseSpeech %d", pause);
    return ring_buffer_push(NULL, command, NULL);
}

int nvda_cancel_speech(void) {
    return ring_buffer_push(NULL, "cancelSpeech", NULL);
}

int nvda_braille(const char* text) {
    return ring_buffer_push("braille \"", text, "\"");
}

int nvda_active(void) {
    return atomic_load_explicit(&g_WorkerRunning, memory_order_acquire) ? 0 : -1;
}

#ifdef __cplusplus
}
#endif
