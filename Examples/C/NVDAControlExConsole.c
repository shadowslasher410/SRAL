#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nvda_control.h"

enum {
	COMMAND_BUFFER_SIZE = 64000,
	HISTORY_MAX_DEPTH   = 50
};

#if defined(__GNUC__) || defined(__clang__)
	#define SRAL_INLINE     inline __attribute__((always_inline))
	#define SRAL_PURE       __attribute__((pure))
	#define SRAL_UNLIKELY(x) __builtin_expect(!!(x), 0)
	#define SRAL_LIKELY(x)   __builtin_expect(!!(x), 1)
#elif defined(_MSC_VER)
	#define SRAL_INLINE     __forceinline
	#define SRAL_PURE
	#define SRAL_UNLIKELY(x) (x)
	#define SRAL_LIKELY(x)   (x)
#else
	#define SRAL_INLINE     inline
	#define SRAL_PURE
	#define SRAL_UNLIKELY(x) (x)
	#define SRAL_LIKELY(x)   (x)
#endif

#if defined(_MSC_VER) && !defined(__clang__)
	#include <intrin.h>
	#define SRAL_ATOMIC_READ(var) (*(volatile LONG*)&(var))
	#define SRAL_ATOMIC_STORE(ptr, val) _InterlockedExchange((volatile long*)(ptr), (long)(val))
#else
	#include <stdatomic.h>
	#define SRAL_ATOMIC_READ(var) atomic_load_explicit((_Atomic int*)&(var), memory_order_acquire)
	#define SRAL_ATOMIC_STORE(ptr, val) atomic_store_explicit((_Atomic int*)(ptr), (val), memory_order_release)
#endif

static volatile LONG g_Running = 0;

static void on_exit_handler(void) {
	const HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD written;
	WriteFile(hStdout, "Exiting...\n", 11, &written, NULL);
	SRAL_ATOMIC_STORE(&g_Running, 0);
}

static BOOL WINAPI ConsoleHandler(DWORD signal) {
	if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
		on_exit_handler();
		return TRUE;
	}
	return FALSE;
}

static char* g_History[HISTORY_MAX_DEPTH] = {0};
static int g_HistoryCount = 0;
static int g_HistoryIndex = 0;

static void history_push(const char* const restrict cmd) {
	if (SRAL_UNLIKELY(!cmd || *cmd == '\0'))
		return;

	if (g_HistoryCount > 0 && strcmp(g_History[g_HistoryCount - 1], cmd) == 0)
		return;

	if (g_HistoryCount < HISTORY_MAX_DEPTH) {
		g_History[g_HistoryCount] = _strdup(cmd);
		if (g_History[g_HistoryCount]) {
			g_HistoryCount++;
		}
	}
	else {
		char* const old_lead = g_History[0];
		memmove(&g_History[0], &g_History[1], (HISTORY_MAX_DEPTH - 1) * sizeof(char*));
		g_History[HISTORY_MAX_DEPTH - 1] = _strdup(cmd);
		if (old_lead)
			free(old_lead);
	}
}

static void free_history(void) {
	for (int m = 0; m < g_HistoryCount; m++) {
		if (g_History[m]) {
			free(g_History[m]);
			g_History[m] = NULL;
		}
	}
	g_HistoryCount = 0;
}

static DWORD oldModeIn = 0;
static DWORD oldModeOut = 0;

static void enable_raw_mode(void) {
	const HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	const HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdin != INVALID_HANDLE_VALUE && hStdin != NULL) {
		GetConsoleMode(hStdin, &oldModeIn);
		SetConsoleMode(hStdin, oldModeIn & (DWORD)(~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT)));
	}
	if (hStdout != INVALID_HANDLE_VALUE && hStdout != NULL) {
		GetConsoleMode(hStdout, &oldModeOut);
		SetConsoleMode(hStdout, oldModeOut | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	}
}

static void disable_raw_mode(void) {
	const HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	if (hStdin != INVALID_HANDLE_VALUE && hStdin != NULL) {
		SetConsoleMode(hStdin, oldModeIn);
	}
}

static void clear_current_line(const HANDLE hStdout) {
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (GetConsoleScreenBufferInfo(hStdout, &csbi)) {
		csbi.dwCursorPosition.X = 0;
		(void)SetConsoleCursorPosition(hStdout, csbi.dwCursorPosition);

		DWORD written = 0;
		const DWORD length_to_clear = (DWORD)csbi.dwSize.X;
		(void)FillConsoleOutputCharacterW(hStdout, L' ', length_to_clear, csbi.dwCursorPosition, &written);
		(void)FillConsoleOutputAttribute(hStdout, csbi.wAttributes, length_to_clear, csbi.dwCursorPosition, &written);
	}
}

SRAL_PURE static SRAL_INLINE size_t get_last_glyph_bytes(const char* const restrict buffer, const size_t current_len) {
	if (current_len == 0)
		return 0;
	size_t steps = 1;
	const unsigned char* const restrict ubuf = (const unsigned char*)buffer;
	while (steps <= 4 && steps <= current_len) {
		if ((ubuf[current_len - steps] & 0xC0) != 0x80) {
			return steps;
		}
		steps++;
	}
	return 1;
}
int main(void) {
	const HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	const HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD written;

	char* const restrict command_buffer = (char*)malloc(COMMAND_BUFFER_SIZE);
	if (SRAL_UNLIKELY(!command_buffer)) {
		WriteFile(GetStdHandle(STD_ERROR_HANDLE), "Fatal error: Allocation failure for input buffer.\n", 50, &written, NULL);
		return EXIT_FAILURE;
	}

	const int connection_result = nvda_connect();
	if (SRAL_UNLIKELY(connection_result == -1)) {
		WriteFile(hStdout, "Failed to connect to NVDA named pipe.\n", 38, &written, NULL);
		free(command_buffer);
		return EXIT_FAILURE;
	}

	(void)SetConsoleOutputCP(CP_UTF8);
	(void)SetConsoleCP(CP_UTF8);

	enable_raw_mode();

	const char welcome_msg[] = "Welcome to NVDA Controller Extended Console!\nTo find commands and expected arguments, "
							   "see the NVDAControlEx addon documentation\n";
	WriteFile(hStdout, welcome_msg, sizeof(welcome_msg) - 1, &written, NULL);

	(void)SetConsoleCtrlHandler(ConsoleHandler, TRUE);
	SRAL_ATOMIC_STORE(&g_Running, 1);
	
	const char prompt_char = '>';
	const char blank_char = ' ';

	while (SRAL_ATOMIC_READ(g_Running) == 1) {
		WriteFile(hStdout, &prompt_char, 1, &written, NULL);
		command_buffer[0] = '\0';
		size_t buffer_idx = 0;
		g_HistoryIndex = g_HistoryCount;

		while ((SRAL_ATOMIC_READ(g_Running) == 1) && (buffer_idx < COMMAND_BUFFER_SIZE - 1)) {
			INPUT_RECORD ir;
			DWORD count = 0;

			if (SRAL_UNLIKELY(hStdin == INVALID_HANDLE_VALUE || hStdin == NULL)) {
				Sleep(10);
				continue;
			}
			// Polling wait point
			if (WaitForSingleObject(hStdin, 100) == WAIT_TIMEOUT) {
				continue;
			}
			if (!ReadConsoleInputW(hStdin, &ir, 1, &count) || count == 0) {
				continue;
			}

			if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
				const WORD vKey = ir.Event.KeyEvent.wVirtualKeyCode;
				const WCHAR wch = ir.Event.KeyEvent.uChar.UnicodeChar;

				if (vKey == VK_UP || vKey == VK_DOWN) {
					if (g_HistoryCount == 0)
						continue;

					if (vKey == VK_UP) {
						if (g_HistoryIndex > 0)
							g_HistoryIndex--;
					}
					else {
						if (g_HistoryIndex < g_HistoryCount)
							g_HistoryIndex++;
					}

					clear_current_line(hStdout);
					WriteFile(hStdout, &prompt_char, 1, &written, NULL);

					buffer_idx = 0;
					command_buffer[0] = '\0';

					if (g_HistoryIndex < g_HistoryCount) {
						const char* const restrict historical_cmd = g_History[g_HistoryIndex];
						const size_t cmd_len = strlen(historical_cmd);

						if (SRAL_LIKELY(cmd_len < COMMAND_BUFFER_SIZE - 1)) {
							memcpy(command_buffer, historical_cmd, cmd_len);
							buffer_idx = cmd_len;
							command_buffer[buffer_idx] = '\0';
							WriteFile(hStdout, command_buffer, (DWORD)cmd_len, &written, NULL);
						}
					}
					continue;
				}

				if (wch == L'\r')
					break;

				if (wch == L'\b') {
					if (buffer_idx > 0) {
						const size_t last_glyph_bytes = get_last_glyph_bytes(command_buffer, buffer_idx);
						buffer_idx -= last_glyph_bytes;
						command_buffer[buffer_idx] = '\0';

						CONSOLE_SCREEN_BUFFER_INFO csbi;
						if (GetConsoleScreenBufferInfo(hStdout, &csbi)) {
							if (csbi.dwCursorPosition.X > 1) {
								csbi.dwCursorPosition.X--;
								(void)SetConsoleCursorPosition(hStdout, csbi.dwCursorPosition);
								WriteFile(hStdout, &blank_char, 1, &written, NULL);
								(void)SetConsoleCursorPosition(hStdout, csbi.dwCursorPosition);
							}
						}
						else {
							WriteFile(hStdout, "\b \b", 3, &written, NULL);
						}
					}
					continue;
				}

				if (wch < 32 && wch != L'\t') {
					if (vKey == VK_ESCAPE) {
						on_exit_handler();
						break;
					}
					(void)MessageBeep(MB_ICONERROR);
					continue;
				}
				char utf8Buf[4];
				const int bytesWritten = WideCharToMultiByte(CP_UTF8, 0, &wch, 1, utf8Buf, 4, NULL, NULL);

				if (bytesWritten > 0 && bytesWritten <= 4) {
					if (buffer_idx + (size_t)bytesWritten < COMMAND_BUFFER_SIZE - 1) {
						memcpy(&command_buffer[buffer_idx], utf8Buf, (size_t)bytesWritten);
						buffer_idx += (size_t)bytesWritten;
						WriteFile(hStdout, utf8Buf, (DWORD)bytesWritten, &written, NULL);
					}
				}
			}
		}

		command_buffer[buffer_idx] = '\0';

		if ((SRAL_ATOMIC_READ(g_Running) == 1) && buffer_idx > 0) {
			history_push(command_buffer);
			if (SRAL_UNLIKELY(nvda_send_command(command_buffer) == -1)) {
				printf("\nFailed to send command: %s\n", command_buffer);
			}
			else {
				WriteFile(hStdout, "\r\n", 2, &written, NULL);
			}
		}
	}

	disable_raw_mode();
	nvda_disconnect();
	free_history();
	free(command_buffer);
	return EXIT_SUCCESS;
}