#ifndef NVDA_CONTROL_H
#define NVDA_CONTROL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define BS_RESTRICT     __restrict__
    #define BS_PURE         __attribute__((pure))
    #define BS_MUST_CHECK   __attribute__((warn_unused_result))
    #define BS_NONNULL_ALL  __attribute__((nonnull))
    #define BS_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
    #define BS_LEAF         __attribute__((leaf))
#elif defined(_MSC_VER)
    #define BS_RESTRICT     __restrict
    #define BS_PURE
    #define BS_MUST_CHECK   _Check_return_
    #define BS_NONNULL_ALL
    #define BS_NONNULL(...)
    #define BS_LEAF
#else
    #define BS_RESTRICT     restrict
    #define BS_PURE
    #define BS_MUST_CHECK
    #define BS_NONNULL_ALL
    #define BS_NONNULL(...)
    #define BS_LEAF
#endif

/* Unified packet-oriented Named Pipe descriptor matching official addon specifications */
#define NVDA_PIPE_NAME L"\\\\.\\pipe\\NVDAControlPipe"

/* Symbol punctuation level enumeration constraints */
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
BS_MUST_CHECK int nvda_connect(void) BS_LEAF;

/**
 * @brief Disconnects from the NVDA named pipe.
 */
void nvda_disconnect(void) BS_LEAF;

/**
 * @brief Sends a pre-formatted command directly to the NVDA named pipe.
 * @param command The raw command string to transmit.
 * @return 0 on success, -1 on failure.
 */
int nvda_send_command(const char* BS_RESTRICT const command) BS_NONNULL_ALL BS_LEAF;

/**
 * @brief Sends a "speak" command to NVDA.
 * @param text The text payload phrase to speak.
 * @param symbol_level Punctuation level for speech.
 * @return 0 on success, -1 on failure.
 */
int nvda_speak(const char* BS_RESTRICT const text, const int symbol_level) BS_NONNULL(1) BS_LEAF;

/**
 * @brief Sends a "speakSpelling" command to NVDA.
 * @param text The text characters to spell out explicitly.
 * @param locale The locale language selection descriptor for speech spelling.
 * @param use_character_descriptions Force NVDA to describe each character.
 * @return 0 on success, -1 on failure.
 */
int nvda_speak_spelling(
	const char* BS_RESTRICT const text, 
	const char* BS_RESTRICT const locale, 
	const int use_character_descriptions) BS_NONNULL(1) BS_LEAF;

/**
 * @brief Sends a "speakSsml" command to NVDA.
 * @param ssml The raw SSML string snippet to speak.
 * @param symbol_level Punctuation level for speech.
 * @return 0 on success, -1 on failure.
 */
int nvda_speak_ssml(const char* BS_RESTRICT const ssml, const int symbol_level) BS_NONNULL(1) BS_LEAF;

/**
 * @brief Sends a "pauseSpeech" command to NVDA.
 * @param pause Pass 1 to pause speech stream, or 0 to resume it.
 * @return 0 on success, -1 on failure.
 */
int nvda_pause_speech(const int pause) BS_LEAF;

/**
 * @brief Sends a "cancelSpeech" command to clear active speech channels instantly.
 * @return 0 on success, -1 on failure.
 */
int nvda_cancel_speech(void) BS_LEAF;

/**
 * @brief Sends a "braille" command to update physical braille display lines.
 * @param text The text translation phrase to map down to the display cells.
 * @return 0 on success, -1 on failure.
 */
int nvda_braille(const char* BS_RESTRICT const text) BS_NONNULL_ALL BS_LEAF;

/**
 * @brief Non-blocking transactional probe confirming if the extension is alive.
 * @return 0 if alive, or -1 on connectivity failures.
 */
BS_PURE BS_MUST_CHECK int nvda_active(void) BS_LEAF;

#ifdef __cplusplus
}
#endif

#endif /* NVDA_CONTROL_H */
