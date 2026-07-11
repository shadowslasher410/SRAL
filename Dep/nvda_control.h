/*
 * NVDA Control Header File
 *
 * Copyright (c) 2025 [m1maker]
 * This header file defines the interface for communicating with the NVDA (NonVisual Desktop Access)
 * screen reader via a named pipe
 *
 * Prerequisites:
 * - The NVDAControlEx add-on must be installed in NVDA for this interface to function correctly.
 * https://github.com/m1maker/NVDAControlEx
 *   This add-on enhances the NVDA API, allowing for more advanced control and communication features.
 *
 * Usage:
 * To use this API, include this header file in your source code and link against the nvda_control.c
 * implementation that handles the actual communication with NVDA. Ensure that the NVDAControlEx add-on
 * is installed and enabled.
 */


#ifndef NVDA_CONTROL_H
#define NVDA_CONTROL_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Unified packet-oriented Named Pipe descriptor matching official addon specifications */
#define NVDA_PIPE_NAME L"\\\\.\\pipe\\NVDAControlPipe"

/*Symbol punctuation level enumeration constraints*/
enum nvda_symbol_level {
	NVDA_SYMBOL_LEVEL_NONE = 0,
	NVDA_SYMBOL_LEVEL_SOME = 100,
	NVDA_SYMBOL_LEVEL_MOST = 200,
	NVDA_SYMBOL_LEVEL_ALL = 300,
	NVDA_SYMBOL_LEVEL_CHAR = 1000,
	NVDA_SYMBOL_LEVEL_UNCHANGED = -1
};

/**
 * @brief Connects to the NVDA named pipe.
 * @return 0 if successful, or -1 on failure.
 */
int nvda_connect(void);

/**
 * @brief Disconnects from the NVDA named pipe.
 */
void nvda_disconnect(void);

/**
 * @brief Sends a pre-formatted command directly to the NVDA named pipe.
 * @param command The raw command string to transmit.
 * @return 0 on success, -1 on failure.
 */
int nvda_send_command(const char* command);

/**
 * @brief Sends a "speak" command to NVDA.
 * @param text The text payload phrase to speak.
 * @param symbol_level Punctuation level for speech.
 * @return 0 on success, -1 on failure.
 */
int nvda_speak(const char* text, int symbol_level);

/**
 * @brief Sends a "speakSpelling" command to NVDA.
 * @param text The text characters to spell out explicitly.
 * @param locale The locale language selection descriptor for speech spelling.
 * @param use_character_descriptions Force NVDA to describe each character.
 * @return 0 on success, -1 on failure.
 */
int nvda_speak_spelling(const char* text, const char* locale, int use_character_descriptions);

/**
 * @brief Sends a "speakSsml" command to NVDA.
 * @param ssml The raw SSML string snippet to speak.
 * @param symbol_level Punctuation level for speech.
 * @return 0 on success, -1 on failure.
 */
int nvda_speak_ssml(const char* ssml, int symbol_level);

/**
 * @brief Sends a "pauseSpeech" command to NVDA.
 * @param pause Pass 1 to pause speech stream, or 0 to resume it.
 * @return 0 on success, -1 on failure.
 */
int nvda_pause_speech(int pause);

/**
 * @brief Sends a "cancelSpeech" command to clear active speech channels instantly.
 * @return 0 on success, -1 on failure.
 */
int nvda_cancel_speech(void);

/**
 * @brief Sends a "braille" command to update physical braille display lines.
 * @param text The text translation phrase to map down to the display cells.
 * @return 0 on success, -1 on failure.
 */
int nvda_braille(const char* text);

/**
 * @brief Non-blocking transactional probe confirming if the extension is alive.
 * @return 0 if alive, or -1 on connectivity failures.
 */
int nvda_active(void);

#ifdef __cplusplus
}
#endif

#endif /* NVDA_CONTROL_H */
