#include <windows.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../Dep/nvda_control.h"

#define COMMAND_BUFFER_SIZE 64000
#define UTF8_CONVERSION_BUFFER_SIZE 4
#define HISTORY_MAX_DEPTH 50

static volatile LONG g_Running = 0;

static void on_exit(void) {
	printf("Exiting...\n");
	(void)InterlockedExchange(&g_Running, 0);
}

static BOOL WINAPI ConsoleHandler(DWORD signal) {
	switch (signal) {
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
		on_exit();
		return TRUE;
	default:
		return FALSE;
	}
}
static void clear_current_line(HANDLE hStdout) {
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

static char* g_History[HISTORY_MAX_DEPTH] = {0};
static int g_HistoryCount = 0;
static int g_HistoryIndex = 0;

static void history_push(const char* const cmd) {
	if (!cmd || strlen(cmd) == 0) {
		return;
	}

	if (g_HistoryCount > 0 && strcmp(g_History[g_HistoryCount - 1], cmd) == 0) {
		return;
	}

	if (g_HistoryCount < HISTORY_MAX_DEPTH) {
		g_History[g_HistoryCount] = _strdup(cmd);
		if (g_History[g_HistoryCount]) {
			g_HistoryCount++;
		}
	}
	else {
		if (g_History[0]) {
			free(g_History[0]);
		}
		for (int m = 1; m < HISTORY_MAX_DEPTH; m++) {
			g_History[m - 1] = g_History[m];
		}
		g_History[HISTORY_MAX_DEPTH - 1] = _strdup(cmd);
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
int main(void) {
	char* const command_buffer = (char*)malloc(COMMAND_BUFFER_SIZE);
	if (!command_buffer) {
		fprintf(stderr, "Fatal error: Allocation failure for input buffer.\n");
		return -1;
	}

	size_t* const byte_sizes = (size_t*)malloc(COMMAND_BUFFER_SIZE * sizeof(size_t));
	if (!byte_sizes) {
		fprintf(stderr, "Fatal error: Allocation failure for size tracking array.\n");
		free(command_buffer);
		return -1;
	}

	const int connection_result = nvda_connect();
	if (connection_result == -1) {
		printf("Failed to connect to NVDA named pipe.\n");
		free(byte_sizes);
		free(command_buffer);
		return -1;
	}

	(void)SetConsoleOutputCP(CP_UTF8);
	(void)SetConsoleCP(CP_UTF8);

	const HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	const HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD oldMode = 0;
	if (hStdin != INVALID_HANDLE_VALUE && hStdin != NULL) {
		(void)GetConsoleMode(hStdin, &oldMode);
		(void)SetConsoleMode(hStdin, oldMode & (DWORD)(~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT)));

	}

	printf("Welcome to NVDA Controller Extended Console!\n");
	printf("To find commands and expected arguments, see the NVDAControlEx addon documentation\n");

	(void)SetConsoleCtrlHandler(ConsoleHandler, TRUE);
	(void)InterlockedExchange(&g_Running, 1);
	while (InterlockedCompareExchange(&g_Running, 0, 0) == 1) {
		(void)putchar('>');
		(void)fflush(stdout);

		memset(command_buffer, 0, COMMAND_BUFFER_SIZE);
		memset(byte_sizes, 0, COMMAND_BUFFER_SIZE * sizeof(size_t));

		size_t buffer_idx = 0;
		size_t total_glyphs = 0;

		g_HistoryIndex = g_HistoryCount;

		while ((InterlockedCompareExchange(&g_Running, 0, 0) == 1) && (buffer_idx < COMMAND_BUFFER_SIZE - 1)) {
			INPUT_RECORD ir;
			DWORD count = 0;

			if (hStdin == INVALID_HANDLE_VALUE || hStdin == NULL) {
				Sleep(10);
				continue;
			}

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
					if (g_HistoryCount == 0) {
						continue;
					}

					if (vKey == VK_UP) {
						if (g_HistoryIndex > 0)
							g_HistoryIndex--;
					}
					else {
						if (g_HistoryIndex < g_HistoryCount)
							g_HistoryIndex++;
					}

					clear_current_line(hStdout);
					(void)putchar('>');
					(void)fflush(stdout);

					memset(command_buffer, 0, COMMAND_BUFFER_SIZE);
					memset(byte_sizes, 0, COMMAND_BUFFER_SIZE * sizeof(size_t));
					total_glyphs = 0;
					buffer_idx = 0;

					if (g_HistoryIndex < g_HistoryCount) {
						const char* const historical_cmd = g_History[g_HistoryIndex];
						const size_t cmd_len = strlen(historical_cmd);

						if (cmd_len < COMMAND_BUFFER_SIZE - 1) {
							memcpy(command_buffer, historical_cmd, cmd_len);
							buffer_idx = cmd_len;

							size_t c_idx = 0;
							while (c_idx < cmd_len) {
								size_t bytes_in_glyph = 1;
								unsigned char lead = (unsigned char)historical_cmd[c_idx];

								if ((lead & 0x80) == 0x00)
									bytes_in_glyph = 1;
								else if ((lead & 0xE0) == 0xC0)
									bytes_in_glyph = 2;
								else if ((lead & 0xF0) == 0xE0)
									bytes_in_glyph = 3;
								else if ((lead & 0xF8) == 0xF0)
									bytes_in_glyph = 4;

								if (c_idx + bytes_in_glyph > cmd_len) {
									bytes_in_glyph = cmd_len - c_idx;
								}

								if (total_glyphs < COMMAND_BUFFER_SIZE) {
									byte_sizes[total_glyphs++] = bytes_in_glyph;
								}
								c_idx += bytes_in_glyph;
							}
							printf("%s", command_buffer);
						}
					}
					(void)fflush(stdout);
					continue;
				}
				if (wch == L'\r') {
					break;
				}

				if (wch == L'\b') {
					if (total_glyphs > 0) {
						total_glyphs--;
						const size_t last_glyph_bytes = byte_sizes[total_glyphs];

						for (size_t b_idx = 0; b_idx < last_glyph_bytes; b_idx++) {
							if (buffer_idx > 0) {
								buffer_idx--;
								command_buffer[buffer_idx] = '\0';
							}
						}

						CONSOLE_SCREEN_BUFFER_INFO csbi;
						if (GetConsoleScreenBufferInfo(hStdout, &csbi)) {
							if (csbi.dwCursorPosition.X > 1) {
								csbi.dwCursorPosition.X--;
								(void)SetConsoleCursorPosition(hStdout, csbi.dwCursorPosition);
								(void)putchar(' ');
								(void)SetConsoleCursorPosition(hStdout, csbi.dwCursorPosition);
							}
						}
						else {
							printf("\b \b");
						}
						(void)fflush(stdout);
					}
					continue;
				}

				if (wch < 32 && wch != L'\t') {
					if (vKey == VK_ESCAPE) {
						on_exit();
						break;
					}
					(void)MessageBeep(MB_ICONERROR);
					continue;
				}

				char utf8Buf[UTF8_CONVERSION_BUFFER_SIZE] = {0};
				const int bytesWritten =
					WideCharToMultiByte(CP_UTF8, 0, &wch, 1, utf8Buf, UTF8_CONVERSION_BUFFER_SIZE, NULL, NULL);

				if (bytesWritten > 0 && bytesWritten <= UTF8_CONVERSION_BUFFER_SIZE) {
					if (buffer_idx + (size_t)bytesWritten < COMMAND_BUFFER_SIZE - 1) {
						if (total_glyphs < COMMAND_BUFFER_SIZE) {
							byte_sizes[total_glyphs++] = (size_t)bytesWritten;
						}
						for (int b_idx = 0; b_idx < bytesWritten; b_idx++) {
							command_buffer[buffer_idx++] = utf8Buf[b_idx];
							(void)putchar(utf8Buf[b_idx]);
						}
						(void)fflush(stdout);
					}
				}
			}
		}

		command_buffer[buffer_idx] = '\0';

		if ((InterlockedCompareExchange(&g_Running, 0, 0) == 1) && strlen(command_buffer) > 0) {
			history_push(command_buffer);
			if (nvda_send_command(command_buffer) == -1) {
				printf("\nFailed to send command: %s\n", command_buffer);
			}
			else {
				printf("\r\n");
			}
			(void)fflush(stdout);
		}
	}

	if (hStdin != INVALID_HANDLE_VALUE && hStdin != NULL) {
		(void)SetConsoleMode(hStdin, oldMode);
	}

	nvda_disconnect();
	free_history();
	free(byte_sizes);
	free(command_buffer);
	return 0;
}
