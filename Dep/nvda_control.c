/*
 * NVDA Control Source File
 *
 * Copyright (c) 2025 [m1maker]
 */

#include "nvda_control.h"

#include <windows.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static HANDLE g_hNvda = INVALID_HANDLE_VALUE;
static SRWLOCK g_NvdaSrwLock = SRWLOCK_INIT;

#define RECURSION_LIMIT 3

static inline bool is_handle_valid(HANDLE h) {
	return (h != INVALID_HANDLE_VALUE && h != NULL && h != (HANDLE)0);
}

_Ret_maybenull_z_ static char* escape_and_format_command(
	_In_z_ const char* prefix, _In_z_ const char* text, _In_z_ const char* suffix) {

	const char* input_text = text ? text : "";
	const size_t prefix_len = prefix ? strlen(prefix) : 0;
	const size_t suffix_len = suffix ? strlen(suffix) : 0;
	const size_t text_len = strlen(input_text);

	const size_t max_allocation = prefix_len + (text_len * 2) + suffix_len + 16;
	if (max_allocation < 16) {
		return NULL;
	}

	char* command = (char*)malloc(max_allocation);
	if (command == NULL) {
		return NULL;
	}

	size_t j = 0;
	if (prefix && prefix_len > 0 && prefix_len < max_allocation) {
		for (size_t i = 0; i < prefix_len && j < max_allocation - 1; i++) {
			command[j++] = prefix[i];
		}
	}

	for (size_t i = 0; input_text[i] != '\0'; i++) {
		if (input_text[i] == '"') {
			if (j >= max_allocation - 2) {
				break;
			}
			command[j++] = '\\';
			command[j++] = '"';
		}
		else {
			if (j >= max_allocation - 1) {
				break;
			}
			command[j++] = input_text[i];
		}
	}

	if (suffix && suffix_len > 0) {
		for (size_t i = 0; i < suffix_len && j < max_allocation - 1; i++) {
			command[j++] = suffix[i];
		}
	}

	if (j < max_allocation) {
		command[j] = '\0';
	}
	else {
		command[max_allocation - 1] = '\0';
	}
	return command;
}

static int nvda_connect_internal(void) {
	if (is_handle_valid(g_hNvda)) {
		CloseHandle(g_hNvda);
	}
	g_hNvda = INVALID_HANDLE_VALUE;

	HANDLE h_new = CreateFileW(NVDA_PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (!is_handle_valid(h_new)) {
		return -1;
	}

	g_hNvda = h_new;
	return 0;
}

int nvda_connect(void) {
	AcquireSRWLockExclusive(&g_NvdaSrwLock);
	int result = nvda_connect_internal();
	ReleaseSRWLockExclusive(&g_NvdaSrwLock);
	return result;
}

void nvda_disconnect(void) {
	AcquireSRWLockExclusive(&g_NvdaSrwLock);
	HANDLE h = g_hNvda;
	if (is_handle_valid(h)) {
		CloseHandle(h);
		g_hNvda = INVALID_HANDLE_VALUE;
	}
	ReleaseSRWLockExclusive(&g_NvdaSrwLock);
}

int nvda_active(void) {
	AcquireSRWLockExclusive(&g_NvdaSrwLock);

	if (!is_handle_valid(g_hNvda)) {
		if (nvda_connect_internal() == -1) {
			ReleaseSRWLockExclusive(&g_NvdaSrwLock);
			return -1;
		}
	}

	HANDLE h_pipe = g_hNvda;
	if (!is_handle_valid(h_pipe)) {
		g_hNvda = INVALID_HANDLE_VALUE;
		ReleaseSRWLockExclusive(&g_NvdaSrwLock);
		return -1;
	}

	DWORD pipeMode = PIPE_READMODE_MESSAGE | PIPE_NOWAIT;
	if (!SetNamedPipeHandleState(h_pipe, &pipeMode, NULL, NULL)) {
		CloseHandle(h_pipe);
		g_hNvda = INVALID_HANDLE_VALUE;
		ReleaseSRWLockExclusive(&g_NvdaSrwLock);
		return -1;
	}

	static const char* pingCommand = "active";
	DWORD bytesWritten = 0;
	DWORD bytesRead = 0;
	const size_t ping_len = strlen(pingCommand);

	BOOL result = WriteFile(h_pipe, pingCommand, (DWORD)ping_len, &bytesWritten, NULL);
	if (!result || bytesWritten != ping_len) {
		CloseHandle(h_pipe);
		g_hNvda = INVALID_HANDLE_VALUE;
		ReleaseSRWLockExclusive(&g_NvdaSrwLock);
		return -1;
	}

	char buffer[16];
	memset(buffer, 0, sizeof(buffer));

	result = ReadFile(h_pipe, buffer, (DWORD)(sizeof(buffer) - 1), &bytesRead, NULL);

	DWORD restoreMode = PIPE_READMODE_MESSAGE | PIPE_WAIT;
	if (is_handle_valid(h_pipe)) {
		SetNamedPipeHandleState(h_pipe, &restoreMode, NULL, NULL);
	}

	if (!result || bytesRead == 0 || bytesRead >= sizeof(buffer)) {
		DWORD lastError = GetLastError();
		if (lastError == ERROR_NO_DATA || lastError == ERROR_PIPE_LISTENING) {
			ReleaseSRWLockExclusive(&g_NvdaSrwLock);
			return -1;
		}
		if (is_handle_valid(h_pipe)) {
			CloseHandle(h_pipe);
			g_hNvda = INVALID_HANDLE_VALUE;
		}
		ReleaseSRWLockExclusive(&g_NvdaSrwLock);
		return -1;
	}

	buffer[bytesRead] = '\0';
	int active_status = (strcmp(buffer, "NVDA") == 0) ? 0 : -1;
	ReleaseSRWLockExclusive(&g_NvdaSrwLock);
	return active_status;
}

static int nvda_send_command_internal(const char* command) {
	if (command == NULL) {
		return -1;
	}

	for (int attempt = 0; attempt <= RECURSION_LIMIT; attempt++) {
		if (!is_handle_valid(g_hNvda)) {
			if (nvda_connect_internal() == -1) {
				Sleep(5);
				continue;
			}
		}

		HANDLE h_pipe = g_hNvda;
		if (!is_handle_valid(h_pipe)) {
			g_hNvda = INVALID_HANDLE_VALUE;
			Sleep(5);
			continue;
		}

		DWORD bytesWritten = 0;
		const size_t command_len = strlen(command);

		BOOL result = WriteFile(h_pipe, command, (DWORD)command_len, &bytesWritten, NULL);
		if (result && bytesWritten == command_len) {
			return 0;
		}

		CloseHandle(h_pipe);
		g_hNvda = INVALID_HANDLE_VALUE;
		Sleep(5);
	}
	return -1;
}

int nvda_send_command(const char* command) {
	if (command == NULL)
		return -1;
	AcquireSRWLockExclusive(&g_NvdaSrwLock);
	int result = nvda_send_command_internal(command);
	ReleaseSRWLockExclusive(&g_NvdaSrwLock);
	return result;
}

int nvda_speak(const char* text, int symbol_level) {
	char* suffix = (char*)malloc(64);
	if (!suffix)
		return -1;
	snprintf(suffix, 64, "\" 0 %d", symbol_level);

	char* command = escape_and_format_command("speak \"", text, suffix);
	free(suffix);

	if (command == NULL)
		return -1;
	int result = nvda_send_command(command);
	free(command);
	return result;
}

int nvda_speak_spelling(const char* text, const char* locale, int use_character_descriptions) {
	const char* safe_locale = locale ? locale : "";
	char* suffix = (char*)malloc(128);
	if (!suffix)
		return -1;
	snprintf(suffix, 128, "\" \"%s\" %d", safe_locale, use_character_descriptions);

	char* command = escape_and_format_command("speakSpelling \"", text, suffix);
	free(suffix);

	if (command == NULL)
		return -1;
	int result = nvda_send_command(command);
	free(command);
	return result;
}

int nvda_speak_ssml(const char* ssml, int symbol_level) {
	char* suffix = (char*)malloc(64);
	if (!suffix)
		return -1;
	snprintf(suffix, 64, "\" 0 %d", symbol_level);

	char* command = escape_and_format_command("speakSsml \"", ssml, suffix);
	free(suffix);

	if (command == NULL)
		return -1;
	int result = nvda_send_command(command);
	free(command);
	return result;
}

int nvda_pause_speech(int pause) {
	char* command = (char*)malloc(64);
	if (!command)
		return -1;
	snprintf(command, 64, "pauseSpeech %d", pause);

	int result = nvda_send_command(command);
	free(command);
	return result;
}

int nvda_cancel_speech(void) {
	return nvda_send_command("cancelSpeech");
}

int nvda_braille(const char* text) {
	char* command = escape_and_format_command("braille \"", text, "\"");
	if (command == NULL)
		return -1;

	int result = nvda_send_command(command);
	free(command);
	return result;
}

#ifdef __cplusplus
}
#endif
