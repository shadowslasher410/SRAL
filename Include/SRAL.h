/*
   SRAL (Screen Reader Abstraction Library)
   Copyright (c) 2024-2025 [m1maker]


   @file SRAL.h
   @brief This header file defines the Screen Reader Abstraction Library (SRAL).

   SRAL provides a unified interface for interacting with various speech engines.
   It abstracts the differences between multiple speech engines, allowing developers to
   implement accessibility features in their applications without needing to handle the
   specifics of each engine.

   This library supports multiple speech engines and offers a variety of features,
   enabling applications to provide auditory feedback, braille output, and control
   over speech parameters such as rate and volume.

*/
#ifndef SRAL_H_
#define SRAL_H_
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#include <concepts>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#else
#include <stdbool.h>
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#define SRAL_API EMSCRIPTEN_KEEPALIVE
#elif defined(_WIN32)
#if defined(SRAL_EXPORT)
#define SRAL_API __declspec(dllexport)
#elif defined(SRAL_STATIC)
#define SRAL_API
#else
#define SRAL_API __declspec(dllimport)
#endif
#else
#define SRAL_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
#define SRAL_LIKELY(x) (x) [[likely]]
#define SRAL_UNLIKELY(x) (x) [[unlikely]]
#else
#if defined(__GNUC__) || defined(__clang__)
#define SRAL_LIKELY(x) __builtin_expect(!!(x), 1)
#define SRAL_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define SRAL_LIKELY(x) (x)
#define SRAL_UNLIKELY(x) (x)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @enum SRAL_Engines
 * @brief Defines unique bit flags representing various accessibility engines.
 *
 * These flags identify different screen readers, speech synthesis engines, or
 * accessibility frameworks across various operating systems. They can be
 * bitwise OR'ed (|) together to build exclusion engine masks.
 */
#ifdef __cplusplus
enum SRAL_Engines : uint32_t {
#else
enum SRAL_Engines {
#endif
	/** @brief No specific engine identified or engine is unknown. */
	SRAL_ENGINE_NONE = 0,

	/* Windows Screen Readers */
	/** @brief NonVisual Desktop Access (NVDA) open-source screen reader for Windows. */
	SRAL_ENGINE_NVDA = 1U << 1,
	/** @brief Job Access With Speech (JAWS) commercial screen reader for Windows. */
	SRAL_ENGINE_JAWS = 1U << 2,
	/** @brief Zhengdu Screen Reader (ZDSR) specialized screen reader for Windows. */
	SRAL_ENGINE_ZDSR = 1U << 3,
	/** @brief Microsoft Narrator, the built-in screen reader for Windows systems. */
	SRAL_ENGINE_NARRATOR = 1U << 4,

	/* Windows Accessibility Frameworks */
	/** @brief Microsoft UI Automation (UIA) notification bridge framework for Windows. */
	SRAL_ENGINE_UIA = 1U << 5,

	/* Windows Speech Synthesis Engines */
	/** @brief Microsoft Speech API (SAPI) for text-to-speech rendering on Windows. */
	SRAL_ENGINE_SAPI = 1U << 6,

	/* Linux Speech Synthesis Engines */
	/** @brief Speech Dispatcher, the standard central speech synthesis daemon on Linux. */
	SRAL_ENGINE_SPEECH_DISPATCHER = 1U << 7,

	/** @brief Orca, the standard open-source desktop screen reader for Linux environments. */
	SRAL_ENGINE_ORCA = 1U << 8,

	/* Apple Screen Readers (macOS, iOS, etc.) */
	/** @brief Apple VoiceOver, the integrated system screen reader on macOS and iOS. */
	SRAL_ENGINE_VOICE_OVER = 1U << 9,

	/* Apple Speech Synthesis Engines */
	/** @brief Legacy Cocoa NSSpeechSynthesizer engine for macOS text-to-speech. */
	SRAL_ENGINE_NS_SPEECH = 1U << 10,
	/** @brief Modern AVFoundation AVSpeechSynthesizer framework for Apple platforms. */
	SRAL_ENGINE_AV_SPEECH = 1U << 11,

	/* Android Speech Synthesis Engines */
	/** @brief Android AccessibilityManager driving screen readers such as TalkBack. */
	SRAL_ENGINE_ANDROID_ACCESSIBILITY_MANAGER = 1U << 12,
	/** @brief Native Android Text-To-Speech rendering subsystem engine layer. */
	SRAL_ENGINE_ANDROID_TEXT_TO_SPEECH = 1U << 13,

	/* Chrome OS / Browser */
	/** @brief ChromeVox accessibility engine browser extension & ChromeOS native service. */
	SRAL_ENGINE_CHROMEVOX = 1U << 14,

	/* Cross-Platform Framework Infrastructure */
	/** @brief AccessKit unified static/shared library bridge layer for multi-target layouts. */
	SRAL_ENGINE_ACCESSKIT = 1U << 15
};

/**
 * @enum SRAL_EngineCategory
 * @brief Broad categories an engine can belong to.
 *
 * Each engine reports its own category (see SRAL_GetEngineCategory).
 *
 * Unlike SRAL_Engines, these values are NOT bit flags; an engine has exactly
 * one category.
 */
#ifdef __cplusplus
enum SRAL_EngineCategory : uint32_t {
#else
typedef enum SRAL_EngineCategory {
#endif
	/** @brief Category unknown, or the engine is not available/initialized. */
	SRAL_ENGINE_CATEGORY_UNKNOWN = 0,
	/** @brief A screen reader (e.g. NVDA, JAWS, ZDSR, VoiceOver). */
	SRAL_ENGINE_CATEGORY_SCREEN_READER,
	/** @brief A pure text-to-speech synthesizer (e.g. SAPI, Speech Dispatcher, NSSpeech, AVSpeech, Android
	   TextToSpeech). */
	SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE,
	/** @brief An accessibility provider that drives whatever assistive tech is consuming it (e.g. UIA, Android
	   AccessibilityManager). */
	SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER
#ifdef __cplusplus
};
#else
} SRAL_EngineCategory;
#endif

/**
 * @brief Enumeration of supported features in the engines.
 *
 * This enumeration defines the features supported by various speech engines.
 * It uses a bitmask approach where each feature is represented by a unique bit.
 */
#ifdef __cplusplus
enum SRAL_SupportedFeatures : uint32_t
#else
enum SRAL_SupportedFeatures
#endif
{
	/** Engine can convert text to speech output. */
	SRAL_SUPPORTS_SPEECH = (1U << 1),

	/** Engine can convert text to Braille output. */
	SRAL_SUPPORTS_BRAILLE = (1U << 2),

	/** Engine allows modifying the speaking speed. */
	SRAL_SUPPORTS_SPEECH_RATE = (1U << 3),

	/** Engine allows modifying the speech audio volume. */
	SRAL_SUPPORTS_SPEECH_VOLUME = (1U << 4),

	/** Engine supports switching between different voices or languages. */
	SRAL_SUPPORTS_SELECT_VOICE = (1U << 5),

	/** Engine supports pausing and resuming active speech playback. */
	SRAL_SUPPORTS_PAUSE_SPEECH = (1U << 6),

	/** Engine can parse Speech Synthesis Markup Language (SSML). */
	SRAL_SUPPORTS_SSML = (1U << 7),

	/** Engine supports rendering speech into an in-memory audio buffer. */
	SRAL_SUPPORTS_SPEAK_TO_MEMORY = (1U << 8),

	/** Engine has a dedicated mode for spelling out text letter by letter. */
	SRAL_SUPPORTS_SPELLING = (1U << 9)
};

/**
 * @enum SRAL_EngineParams
 * @brief Enumeration of engine parameters.
 */
#ifdef __cplusplus
enum SRAL_EngineParams : uint32_t {
#else
enum SRAL_EngineParams {
#endif
	/** @brief Speaking speed or rate of the speech engine. */
	SRAL_PARAM_SPEECH_RATE,

	/** @brief Audio volume level of the speech engine. */
	SRAL_PARAM_SPEECH_VOLUME,

	/** @brief Index of the currently selected voice. */
	SRAL_PARAM_VOICE_INDEX,

	/** @brief Detailed properties or metadata of the selected voice. */
	SRAL_PARAM_VOICE_PROPERTIES,

	/** @brief Total number of available voices in the engine. */
	SRAL_PARAM_VOICE_COUNT,

	/** @brief Level or verbosity of symbol/punctuation pronunciation. */
	SRAL_PARAM_SYMBOL_LEVEL,

	/** @brief (Windows Only) Threshold value for trimming silence or audio in SAPI engines. */
	SRAL_PARAM_SAPI_TRIM_THRESHOLD,

	/** @brief Toggle to enable or disable letter-by-letter spelling mode. */
	SRAL_PARAM_ENABLE_SPELLING,

	/** @brief Toggle to use descriptive text for single characters (e.g., "A as in Alpha"). */
	SRAL_PARAM_USE_CHARACTER_DESCRIPTIONS,

	/** @brief (Windows only) Specific configuration flag for NVDA integration. */
	SRAL_PARAM_NVDA_IS_CONTROL_EX,

	/** @brief Read-only flag indicating if the speech engine is currently paused. */
	SRAL_PARAM_ENGINE_IS_PAUSED,

	/**
	 * @brief (Android only) Set the JNIEnv* used by Android engines.
	 * Must be set via SRAL_SetEngineParameter before SRAL_Initialize.
	 * Value is a JNIEnv* cast to void*.
	 */
	SRAL_PARAM_ANDROID_JNI_ENV,

	/**
	 * @brief (Android only) Set the Activity (jobject) used by Android engines.
	 * Must be set via SRAL_SetEngineParameter before SRAL_Initialize.
	 * Value is a jobject cast to void*.
	 */
	SRAL_PARAM_ANDROID_ACTIVITY
};

/**
 * @struct SRAL_VoiceInfo
 * @brief Voice information values.
 */
typedef struct SRAL_VoiceInfo {
	/** @brief The display name or identifier of the voice. */
	const char* name;

	/** @brief The language/locale code of the voice (e.g., "en-US", "es-ES"). */
	const char* language;

	/** @brief The gender classification of the voice (e.g., "male", "female", "neutral"). */
	const char* gender;

	/** @brief The creator, company, or vendor of the voice engine. */
	const char* vendor;

	/** @brief The unique zero-based internal index identifying this specific voice. */
	int index;
} SRAL_VoiceInfo;

/**
 * Functions for memory management.
 */

/**
 * @brief Allocate the memory.
 * @param size The size in bytes.
 * @return a pointer to the allocated buffer, if allocation was successful, false otherwise.
 * The caller is responsable to free the memory
 */
[[nodiscard]] SRAL_API void* SRAL_malloc(size_t size) noexcept;

/**
 * @brief Free the allocated memory.
 * @param memory A pointer to the allocated memory.
 */
SRAL_API void SRAL_free(void* memory) noexcept;

/**
 * Functions for interacting with the currently available and active engine (auto update).
 * However, for example, if the engine
 * SAPI or UIAutomation is active, then screen readers,
 * such as Jaws or NVDA, will take priority.
 * Note that engines excluded by calling SRAL_Initialize
 * or SRAL_SetEnginesExclude will not be considered here.
 */

/**
 * @brief Speak the given text.
 * @param text A pointer to the text string to be spoken.
 * @param interrupt A flag indicating whether to interrupt the current speech.
 * @return true if speaking was successful, false otherwise.
 */
[[nodiscard]] SRAL_API bool SRAL_Speak(const char* text, bool interrupt) noexcept;

/**
* @brief Speak the given text into memory.
* @param text A pointer to the text string to be spoken.
* @param buffer_size A pointer to uint64_t to write PCM buffer size.
* @param channels A pointer to int to write PCM channel count.
* @param sample_rate A pointer to int to write PCM sample rate in HZ.
* @param bits_per_sample A pointer to int to write PCM bit size (floating point or signed integer).


* @return a pointer to the PCM buffer if speaking was successful, false otherwise.
* The caller is responsable to free the memory
*/
[[nodiscard]] SRAL_API void* SRAL_SpeakToMemory(
	const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample) noexcept;

/**
 * @brief Speak the given text using SSML tags.
 * @param SSML A pointer to the valid SSML string to be spoken.
 * @param interrupt A flag indicating whether to interrupt the current speech.
 * @return true if speaking was successful, false otherwise.
 */
[[nodiscard]] SRAL_API bool SRAL_SpeakSsml(const char* ssml, bool interrupt) noexcept;

/**
 * @brief Output text to a Braille display.
 * @param text A pointer to the text string to be output in Braille.
 * @return true if Braille output was successful, false otherwise.
 */
[[nodiscard]] SRAL_API bool SRAL_Braille(const char* text) noexcept;

/**
 * @brief Output text using all currently supported speech engine methods.
 * @param text A pointer to the text string to be output.
 * @param interrupt A flag indicating whether to interrupt speech.
 * @return true if output was successful, false otherwise.
 */
[[nodiscard]] SRAL_API bool SRAL_Output(const char* text, bool interrupt) noexcept;

/**
 * @brief Stop speech if it is active.
 * @return true if speech was stopped successfully, false otherwise.
 */
SRAL_API bool SRAL_StopSpeech(void) noexcept;

/**
 * @brief Pause speech if it is active and the current speech engine supports this.
 * @return true if speech was paused successfully, false otherwise.
 */
SRAL_API bool SRAL_PauseSpeech(void) noexcept;

/**
 * @brief Resume speech if it was active and the current speech engine supports this.
 * @return true if speech was resumed successfully, false otherwise.
 */
SRAL_API bool SRAL_ResumeSpeech(void) noexcept;

/**
 * @brief Get status, does this engine speak now.
 * @return true if the engine is currently speaking, false otherwise.
 */
[[nodiscard]] SRAL_API bool SRAL_IsSpeaking(void) noexcept;

/**
 * @brief Get the current speech engine in use.
 * @return The identifier of the current speech engine defined by the SRAL_Engines enumeration.
 */
[[nodiscard]] SRAL_API int SRAL_GetCurrentEngine(void) noexcept;

/**
 * @brief Get features supported by the specified engine.
 * @param engine The identifier of the engine to query. Defaults to 0 (current engine).
 * @return An integer representing the features supported by the engine defined by the SRAL_SupportedFeatures
 * enumeration.
 */
[[nodiscard]] SRAL_API int SRAL_GetEngineFeatures(int engine) noexcept;

/**
 * Engine parameters
 *Pointers to the value of parameters can be integer, logical/boolean (the same as int), and also
 * could also be a const SRAL_VoiceInfo* array if we are getting a list of voices and it's properties.
 * The caller is responsable to free SRAL_VoiceInfo array.
 */

/**
* @brief Set the parameter for the specified speech engine.
* @param engine The engine to set the param for.
* @param param The desired parameter.
* @param value A pointer to desired value.

* @return true if the parameter was set successfully, false otherwise.
*/
SRAL_API bool SRAL_SetEngineParameter(int engine, int param, const void* value) noexcept;

/**
* @brief Get the parameter for the specified speech engine.
* @param engine The engine to get the param for.
* @param value An out pointer to write value.

* @return true if the parameter was retrieved successfully, false otherwise.
*/
SRAL_API bool SRAL_GetEngineParameter(int engine, int param, void* value) noexcept;

/**
 * @brief Initialize the library and optionally exclude certain engines.
 * @param engines_exclude A bitmask specifying engines to exclude from auto update. Defaults to 0 (include all).
 * @return true if initialization was successful, false otherwise.
 */
[[nodiscard]] SRAL_API bool SRAL_Initialize(int engines_exclude) noexcept;

/**
 * @brief Uninitialize the library, freeing resources.
 */
SRAL_API void SRAL_Uninitialize(void) noexcept;

/**
 * Extended functions to perform operations with specific speech engines.
 * Excluded engines when calling SRAL Initialize or SRAL_SetEnginesExclude will always work here,
 * as they are excluded from auto update.
 */

/**
 * @brief Speak the given text with the specified engine.
 * @param engine The engine to use for speaking.
 * @param text A pointer to the text string to be spoken.
 * @param interrupt A flag indicating whether to interrupt the current speech.
 * @return true if speaking was successful, false otherwise.
 */
[[nodiscard]] SRAL_API bool SRAL_SpeakEx(int engine, const char* text, bool interrupt) noexcept;

/**
* @brief Speak the given text into memory with the specified engine.
* @param engine The engine to use for speaking.
* @param text A pointer to the text string to be spoken.
* @param buffer_size A pointer to uint64_t to write PCM buffer size.
* @param channels A pointer to int to write PCM channel count.
* @param sample_rate A pointer to int to write PCM sample rate in HZ.
* @param bits_per_sample A pointer to int to write PCM bit size (floating point or signed integer).

* @return a pointer to the PCM buffer if speaking was successful, false otherwise.
* The caller is responsable to free the memory
*/
[[nodiscard]] SRAL_API void* SRAL_SpeakToMemoryEx(int engine,
	const char* text,
	uint64_t* buffer_size,
	int* channels,
	int* sample_rate,
	int* bits_per_sample) noexcept;

/**
 * @brief Speak the given text with the specified engine and using SSML tags.
 * @param engine The engine to use for speaking.
 * @param ssml A pointer to the valid SSML string to be spoken.
 * @param interrupt A flag indicating whether to interrupt the current speech.
 * @return true if speaking was successful, false otherwise.
 */
[[nodiscard]] SRAL_API bool SRAL_SpeakSsmlEx(int engine, const char* ssml, bool interrupt) noexcept;

/**
 * @brief Output text to a Braille display using the specified engine.
 * @param engine The engine to use for Braille display output.
 * @param text A pointer to the text string to be output in Braille display.
 * @return true if Braille output was successful, false otherwise.
 */
[[nodiscard]] SRAL_API bool SRAL_BrailleEx(int engine, const char* text) noexcept;

/**
 * @brief Output text using the specified engine.
 * @param engine The engine to use for output.
 * @param text A pointer to the text string to be output.
 * @param interrupt A flag indicating whether to interrupt the current speech.
 * @return true if output was successful, false otherwise.
 */
[[nodiscard]] SRAL_API bool SRAL_OutputEx(int engine, const char* text, bool interrupt) noexcept;

/**
 * @brief Stop speech for the specified engine.
 * @param engine The engine to stop speech for.
 * @return true if speech was stopped successfully, false otherwise.
 */
SRAL_API bool SRAL_StopSpeechEx(int engine) noexcept;

/**
 * @brief Pause speech for the specified engine.
 * @param engine The engine to pause speech for.
 * @return true if speech was paused successfully, false otherwise.
 */
SRAL_API bool SRAL_PauseSpeechEx(int engine) noexcept;

/**
 * @brief Resume speech for the specified engine.
 * @param engine The engine to resume speech for.
 * @return true if speech was resumed successfully, false otherwise.
 */
SRAL_API bool SRAL_ResumeSpeechEx(int engine) noexcept;

/**
 * @brief Get status, does this engine speak now.
 * @param engine The engine to get speaking status for.
 * @return true if the engine is currently speaking, false otherwise.
 */
[[nodiscard]] SRAL_API bool SRAL_IsSpeakingEx(int engine) noexcept;

/**
 * @brief Check if the library has been initialized.
 * @return true if the library is initialized, false otherwise.
 */
[[nodiscard]] SRAL_API bool SRAL_IsInitialized(void) noexcept;

/**
 * @brief Delays the next speech or output operation by a given time.
 * @param time The duration to delay in milliseconds.
 */
SRAL_API void SRAL_Delay(int time) noexcept;

/**
 * @brief Schedules a text speech operation to be spoken after a specified delay time on the current engine.
 * @param time Delay time in milliseconds.
 * @param text The text string payload phrase to speak.
 * @param interrupt Flag indicating whether to flush active speaking channels.
 * @return true if successfully scheduled, false otherwise.
 */
[[nodiscard]] SRAL_API bool SRAL_DelayOutput(int time, const char* text, bool interrupt) noexcept;

/**
 * @brief Schedules a text speech operation to be spoken after a specified delay time on a specific engine.
 * @param engine Target engine token identifier.
 * @param time Delay time in milliseconds.
 * @param text The text string payload phrase to speak.
 * @param interrupt Flag indicating whether to flush active speaking channels.
 * @return true if successfully scheduled, false otherwise.
 */
[[nodiscard]] SRAL_API bool SRAL_DelayOutputEx(int engine, int time, const char* text, bool interrupt) noexcept;

/**
 *@brief Install speech interruption and pause keyboard hooks for speech engines other than screen readers, such as
 * Microsoft SAPI 5 or SpeechDispatcher. These hooks work globally in any window. Ctrl - Interrupt, Shift - Pause.
 * @return true if the hooks are successfully installed, false otherwise.
 */
SRAL_API bool SRAL_RegisterKeyboardHooks(void) noexcept;

/**
 *@brief Uninstall speech interruption and pause keyboard hooks.
 */
SRAL_API void SRAL_UnregisterKeyboardHooks(void) noexcept;

/**
 * @brief Get all available engines for the current platform.
 * @return Bitmask with available engines.
 */
[[nodiscard]] SRAL_API int SRAL_GetAvailableEngines(void) noexcept;

/**
 * @brief Get all active engines that can be used.
 * @return Bitmask with active engines.
 */
[[nodiscard]] SRAL_API int SRAL_GetActiveEngines(void) noexcept;

/**
 * @brief Get the category of a specific engine.
 *
 * The category is reported by the engine itself (see SRAL_EngineCategory). The
 * library must be initialized; the engine is resolved against the engines
 * instantiated on the current platform, so an engine that is not available here
 * (or an unknown identifier) returns SRAL_ENGINE_CATEGORY_UNKNOWN.
 *
 * @param engine An SRAL_Engines identifier.
 * @return The engine's SRAL_EngineCategory.
 */
[[nodiscard]] SRAL_API SRAL_EngineCategory SRAL_GetEngineCategory(int engine) noexcept;

/**
 * @brief Get the bitmask of engines that are pure text-to-speech synthesizers
 * (e.g., SAPI, Speech Dispatcher, NSSpeech, AVSpeech, Android TextToSpeech).
 *
 * The mask is derived at runtime from each available engine's category
 * (SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE), so it reflects the engines
 * instantiated on the current platform and requires the library to be
 * initialized (returns 0 otherwise).
 *
 * Intended use: pass to SRAL_SetEnginesExclude when the application wants to
 * opt out of TTS output (for instance, only speaking through a screen reader
 * unless the user has enabled an in-app TTS option).
 *
 * @return Bitmask of TTS engines defined by the SRAL_Engines enumeration.
 */
[[nodiscard]] SRAL_API int SRAL_GetTTSEngines(void) noexcept;

/**
 * @brief Get the bitmask of engines that represent assistive technology
 * (screen readers and the accessibility providers that drive them, e.g.,
 * NVDA, JAWS, ZDSR, UIA, VoiceOver, Android AccessibilityManager).
 *
 * The mask is derived at runtime from each available engine's category
 * (SRAL_ENGINE_CATEGORY_SCREEN_READER or SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER),
 * so it reflects the engines instantiated on the current platform and requires
 * the library to be initialized (returns 0 otherwise).
 *
 * When any of these engines is active, output is routed to the user's
 * configured assistive tech (which itself handles speech and braille
 * per the user's preferences).
 *
 * @return Bitmask of assistive-tech engines defined by the SRAL_Engines enumeration.
 */
[[nodiscard]] SRAL_API int SRAL_GetAssistiveTechEngines(void) noexcept;

/**
 * @brief Get name of the specified engine.
 * @param engine The identifier of the engine to query.
 * @return a pointer to the name.
 */
[[nodiscard]] SRAL_API const char* SRAL_GetEngineName(int engine) noexcept;

/**
 * @brief Set excludes for specified engines
 * @param engines_exclude A bitmask specifying engines to exclude from auto update. Defaults to 0 (include all).
 * @return true if excludes was successful set, false otherwise.
 */
SRAL_API bool SRAL_SetEnginesExclude(int engines_exclude) noexcept;

/**
 * @brief Get engines excluded from auto update.
 * @return bitmask with excluded engines if SRAL was successfully initialized, -1 otherwise.
 */
[[nodiscard]] SRAL_API int SRAL_GetEnginesExclude(void) noexcept;

#ifdef __cplusplus
} // extern "C"

namespace Sral {

[[nodiscard]] constexpr SRAL_Engines operator|(SRAL_Engines lhs, SRAL_Engines rhs) noexcept {
	return static_cast<SRAL_Engines>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}
[[nodiscard]] constexpr uint32_t operator&(SRAL_Engines lhs, SRAL_Engines rhs) noexcept {
	return static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs);
}

template <typename T>
concept NullTerminatedString = requires(T t) {
	{ t.c_str() } -> std::same_as<const char*>;
} || std::is_convertible_v<std::decay_t<T>, const char*> || std::is_same_v<std::decay_t<T>, std::string_view>;

struct [[nodiscard]] PCMBuffer {
	std::span<uint8_t> data{};
	int channels{0};
	int sample_rate{0};
	int bits_per_sample{0};

	PCMBuffer() noexcept = default;
	~PCMBuffer() noexcept {
		if SRAL_LIKELY (data.data() != nullptr) {
			::SRAL_free(static_cast<void*>(data.data()));
		}
	}

	PCMBuffer(const PCMBuffer&) = delete;
	PCMBuffer& operator=(const PCMBuffer&) = delete;

	PCMBuffer(PCMBuffer&& other) noexcept
		: data(other.data), channels(other.channels), sample_rate(other.sample_rate),
		  bits_per_sample(other.bits_per_sample) {
		other.data = {};
	}

	PCMBuffer& operator=(PCMBuffer&& other) noexcept {
		if SRAL_LIKELY (this != &other) {
			if (data.data())
				::SRAL_free(static_cast<void*>(data.data()));
			data = other.data;
			channels = other.channels;
			sample_rate = other.sample_rate;
			bits_per_sample = other.bits_per_sample;
			other.data = {};
		}
		return *this;
	}
};

struct [[nodiscard]] VoiceList {
	std::span<const SRAL_VoiceInfo> voices{};

	VoiceList() noexcept = default;
	~VoiceList() noexcept {
		if SRAL_LIKELY (voices.data() != nullptr) {
			::SRAL_free(const_cast<void*>(static_cast<const void*>(voices.data())));
		}
	}

	VoiceList(const VoiceList&) = delete;
	VoiceList& operator=(const VoiceList&) = delete;

	constexpr VoiceList(VoiceList&& other) noexcept : voices(other.voices) { other.voices = {}; }
	constexpr VoiceList& operator=(VoiceList&& other) noexcept {
		if SRAL_LIKELY (this != &other) {
			if (voices.data())
				::SRAL_free(const_cast<void*>(static_cast<const void*>(voices.data())));
			voices = other.voices;
			other.voices = {};
		}
		return *this;
	}
};

namespace allocationbridges {
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
SRAL_API bool SafeSpeakAllocationBridge(std::string_view text, bool interrupt) noexcept;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
SRAL_API bool SafeSpeakSsmlAllocationBridge(std::string_view ssml, bool interrupt) noexcept;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
SRAL_API bool SafeBrailleAllocationBridge(std::string_view text) noexcept;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
SRAL_API bool SafeOutputAllocationBridge(std::string_view text, bool interrupt) noexcept;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
SRAL_API bool SafeSpeakExAllocationBridge(SRAL_Engines engine, std::string_view text, bool interrupt) noexcept;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
SRAL_API bool SafeSpeakSsmlExAllocationBridge(SRAL_Engines engine, std::string_view ssml, bool interrupt) noexcept;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
SRAL_API bool SafeBrailleExAllocationBridge(SRAL_Engines engine, std::string_view text) noexcept;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
SRAL_API bool SafeOutputExAllocationBridge(SRAL_Engines engine, std::string_view text, bool interrupt) noexcept;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
SRAL_API bool SafeDelayOutputAllocationBridge(int time, std::string_view text, bool interrupt) noexcept;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, cold))
#endif
SRAL_API bool SafeDelayOutputExAllocationBridge(
	SRAL_Engines engine, int time, std::string_view text, bool interrupt) noexcept;

[[nodiscard]] SRAL_API PCMBuffer DirectMemoryBridge(const char* text) noexcept;
[[nodiscard]] SRAL_API PCMBuffer DirectMemoryExBridge(SRAL_Engines engine, const char* text) noexcept;
[[nodiscard]] SRAL_API std::string_view GetEngineNameFastBridge(SRAL_Engines engine) noexcept;
} // namespace allocationbridges

template <NullTerminatedString T> inline bool Speak(const T& text, bool interrupt) noexcept {
	const char* c_str =
		(std::is_convertible_v<std::decay_t<T>, const char*>) ? static_cast<const char*>(text) : text.c_str();
	if SRAL_UNLIKELY (!c_str || *c_str == '\0')
		return false;
	return ::SRAL_Speak(c_str, interrupt);
}

inline bool Speak(std::string_view text, bool interrupt) noexcept {
	if SRAL_UNLIKELY (text.empty())
		return false;
	return allocationbridges::SafeSpeakAllocationBridge(text, interrupt);
}

template <NullTerminatedString T> [[nodiscard]] inline PCMBuffer SpeakToMemory(const T& text) noexcept {
	const char* c_str =
		(std::is_convertible_v<std::decay_t<T>, const char*>) ? static_cast<const char*>(text) : text.c_str();
	if SRAL_UNLIKELY (!c_str || *c_str == '\0')
		return PCMBuffer{};
	return allocationbridges::DirectMemoryBridge(c_str);
}

template <NullTerminatedString T> inline bool SpeakSsml(const T& ssml, bool interrupt) noexcept {
	const char* c_str =
		(std::is_convertible_v<std::decay_t<T>, const char*>) ? static_cast<const char*>(ssml) : ssml.c_str();
	if SRAL_UNLIKELY (!c_str || *c_str == '\0')
		return false;
	return ::SRAL_SpeakSsml(c_str, interrupt);
}

inline bool SpeakSsml(std::string_view ssml, bool interrupt) noexcept {
	if SRAL_UNLIKELY (ssml.empty())
		return false;
	return allocationbridges::SafeSpeakSsmlAllocationBridge(ssml, interrupt);
}

template <NullTerminatedString T> inline bool Braille(const T& text) noexcept {
	const char* c_str =
		(std::is_convertible_v<std::decay_t<T>, const char*>) ? static_cast<const char*>(text) : text.c_str();
	if SRAL_UNLIKELY (!c_str || *c_str == '\0')
		return false;
	return ::SRAL_Braille(c_str);
}

inline bool Braille(std::string_view text) noexcept {
	if SRAL_UNLIKELY (text.empty())
		return false;
	return allocationbridges::SafeBrailleAllocationBridge(text);
}

template <NullTerminatedString T> inline bool Output(const T& text, bool interrupt) noexcept {
	const char* c_str =
		(std::is_convertible_v<std::decay_t<T>, const char*>) ? static_cast<const char*>(text) : text.c_str();
	if SRAL_UNLIKELY (!c_str || *c_str == '\0')
		return false;
	return ::SRAL_Output(c_str, interrupt);
}

inline bool Output(std::string_view text, bool interrupt) noexcept {
	if SRAL_UNLIKELY (text.empty())
		return false;
	return allocationbridges::SafeOutputAllocationBridge(text, interrupt);
}

[[nodiscard]] inline SRAL_Engines GetCurrentEngine() noexcept {
	return static_cast<SRAL_Engines>(::SRAL_GetCurrentEngine());
}

[[nodiscard]] inline uint32_t GetEngineFeatures(SRAL_Engines engine = SRAL_ENGINE_NONE) noexcept {
	return static_cast<uint32_t>(::SRAL_GetEngineFeatures(static_cast<int>(engine)));
}

template <typename ValueType>
inline bool SetEngineParameter(SRAL_Engines engine, SRAL_EngineParams param, const ValueType& value) noexcept {
	if constexpr (std::is_pointer_v<ValueType> || std::is_convertible_v<ValueType, const void*>) {
		return ::SRAL_SetEngineParameter(static_cast<int>(engine), static_cast<int>(param), value);
	}
	else {
		return ::SRAL_SetEngineParameter(
			static_cast<int>(engine), static_cast<int>(param), static_cast<const void*>(&value));
	}
}

template <typename ValueType>
inline bool GetEngineParameter(SRAL_Engines engine, SRAL_EngineParams param, ValueType& value) noexcept {
	return ::SRAL_GetEngineParameter(static_cast<int>(engine), static_cast<int>(param), static_cast<void*>(&value));
}

[[nodiscard]] inline VoiceList GetEngineVoiceList(SRAL_Engines engine) noexcept {
	void* out_ptr = nullptr;
	int count = 0;
	::SRAL_GetEngineParameter(static_cast<int>(engine), static_cast<int>(SRAL_PARAM_VOICE_COUNT), &count);
	if SRAL_UNLIKELY (count == 0)
		return VoiceList{};

	if SRAL_LIKELY (::SRAL_GetEngineParameter(
						static_cast<int>(engine), static_cast<int>(SRAL_PARAM_VOICE_PROPERTIES), &out_ptr)) {
		VoiceList result;
		result.voices =
			std::span<const SRAL_VoiceInfo>(static_cast<const SRAL_VoiceInfo*>(out_ptr), static_cast<size_t>(count));
		return result;
	}
	return VoiceList{};
}

inline bool Initialize(uint32_t engines_exclude_mask = 0) noexcept {
	return ::SRAL_Initialize(static_cast<int>(engines_exclude_mask));
}

template <NullTerminatedString T> inline bool SpeakEx(SRAL_Engines engine, const T& text, bool interrupt) noexcept {
	const char* c_str =
		(std::is_convertible_v<std::decay_t<T>, const char*>) ? static_cast<const char*>(text) : text.c_str();
	if SRAL_UNLIKELY (!c_str || *c_str == '\0')
		return false;
	return ::SRAL_SpeakEx(static_cast<int>(engine), c_str, interrupt);
}

inline bool SpeakEx(SRAL_Engines engine, std::string_view text, bool interrupt) noexcept {
	if SRAL_UNLIKELY (text.empty())
		return false;
	return allocationbridges::SafeSpeakExAllocationBridge(engine, text, interrupt);
}

template <NullTerminatedString T>
[[nodiscard]] inline PCMBuffer SpeakToMemoryEx(SRAL_Engines engine, const T& text) noexcept {
	const char* c_str =
		(std::is_convertible_v<std::decay_t<T>, const char*>) ? static_cast<const char*>(text) : text.c_str();
	if SRAL_UNLIKELY (!c_str || *c_str == '\0')
		return PCMBuffer{};
	return allocationbridges::DirectMemoryExBridge(engine, c_str);
}

template <NullTerminatedString T> inline bool SpeakSsmlEx(SRAL_Engines engine, const T& ssml, bool interrupt) noexcept {
	const char* c_str =
		(std::is_convertible_v<std::decay_t<T>, const char*>) ? static_cast<const char*>(ssml) : ssml.c_str();
	if SRAL_UNLIKELY (!c_str || *c_str == '\0')
		return false;
	return ::SRAL_SpeakSsmlEx(static_cast<int>(engine), c_str, interrupt);
}

inline bool SpeakSsmlEx(SRAL_Engines engine, std::string_view ssml, bool interrupt) noexcept {
	if SRAL_UNLIKELY (ssml.empty())
		return false;
	return allocationbridges::SafeSpeakSsmlExAllocationBridge(engine, ssml, interrupt);
}

template <NullTerminatedString T> inline bool BrailleEx(SRAL_Engines engine, const T& text) noexcept {
	const char* c_str =
		(std::is_convertible_v<std::decay_t<T>, const char*>) ? static_cast<const char*>(text) : text.c_str();
	if SRAL_UNLIKELY (!c_str || *c_str == '\0')
		return false;
	return ::SRAL_BrailleEx(static_cast<int>(engine), c_str);
}

inline bool BrailleEx(SRAL_Engines engine, std::string_view text) noexcept {
	if SRAL_UNLIKELY (text.empty())
		return false;
	return allocationbridges::SafeBrailleExAllocationBridge(engine, text);
}

template <NullTerminatedString T> inline bool OutputEx(SRAL_Engines engine, const T& text, bool interrupt) noexcept {
	const char* c_str =
		(std::is_convertible_v<std::decay_t<T>, const char*>) ? static_cast<const char*>(text) : text.c_str();
	if SRAL_UNLIKELY (!c_str || *c_str == '\0')
		return false;
	return ::SRAL_OutputEx(static_cast<int>(engine), c_str, interrupt);
}

inline bool OutputEx(SRAL_Engines engine, std::string_view text, bool interrupt) noexcept {
	if SRAL_UNLIKELY (text.empty())
		return false;
	return allocationbridges::SafeOutputExAllocationBridge(engine, text, interrupt);
}

inline bool StopSpeechEx(SRAL_Engines engine) noexcept {
	return ::SRAL_StopSpeechEx(static_cast<int>(engine));
}

inline bool PauseSpeechEx(SRAL_Engines engine) noexcept {
	return ::SRAL_PauseSpeechEx(static_cast<int>(engine));
}

inline bool ResumeSpeechEx(SRAL_Engines engine) noexcept {
	return ::SRAL_ResumeSpeechEx(static_cast<int>(engine));
}

[[nodiscard]] inline bool IsSpeakingEx(SRAL_Engines engine) noexcept {
	return ::SRAL_IsSpeakingEx(static_cast<int>(engine));
}

template <NullTerminatedString T>
[[nodiscard]] inline bool DelayOutput(int time, const T& text, bool interrupt) noexcept {
	const char* c_str =
		(std::is_convertible_v<std::decay_t<T>, const char*>) ? static_cast<const char*>(text) : text.c_str();
	if SRAL_UNLIKELY (!c_str || *c_str == '\0')
		return false;
	return ::SRAL_DelayOutput(time, c_str, interrupt);
}

[[nodiscard]] inline bool DelayOutput(int time, std::string_view text, bool interrupt) noexcept {
	if SRAL_UNLIKELY (text.empty())
		return false;
	return allocationbridges::SafeDelayOutputAllocationBridge(time, text, interrupt);
}

template <NullTerminatedString T>
[[nodiscard]] inline bool DelayOutputEx(SRAL_Engines engine, int time, const T& text, bool interrupt) noexcept {
	const char* c_str =
		(std::is_convertible_v<std::decay_t<T>, const char*>) ? static_cast<const char*>(text) : text.c_str();
	if SRAL_UNLIKELY (!c_str || *c_str == '\0')
		return false;
	return ::SRAL_DelayOutputEx(static_cast<int>(engine), time, c_str, interrupt);
}

[[nodiscard]] inline bool DelayOutputEx(SRAL_Engines engine, int time, std::string_view text, bool interrupt) noexcept {
	if SRAL_UNLIKELY (text.empty())
		return false;
	return allocationbridges::SafeDelayOutputExAllocationBridge(engine, time, text, interrupt);
}

inline bool RegisterKeyboardHooks() noexcept {
	return ::SRAL_RegisterKeyboardHooks();
}

inline void UnregisterKeyboardHooks() noexcept {
	::SRAL_UnregisterKeyboardHooks();
}

[[nodiscard]] inline uint32_t GetAvailableEngines() noexcept {
	return static_cast<uint32_t>(::SRAL_GetAvailableEngines());
}

[[nodiscard]] inline uint32_t GetActiveEngines() noexcept {
	return static_cast<uint32_t>(::SRAL_GetActiveEngines());
}

[[nodiscard]] inline SRAL_EngineCategory GetEngineCategory(SRAL_Engines engine) noexcept {
	return ::SRAL_GetEngineCategory(static_cast<int>(engine));
}

[[nodiscard]] inline uint32_t GetTTSEngines() noexcept {
	return static_cast<uint32_t>(::SRAL_GetTTSEngines());
}

[[nodiscard]] inline uint32_t GetAssistiveTechEngines() noexcept {
	return static_cast<uint32_t>(::SRAL_GetAssistiveTechEngines());
}

[[nodiscard]] inline std::string_view GetEngineName(SRAL_Engines engine) noexcept {
	return allocationbridges::GetEngineNameFastBridge(engine);
}

inline bool SetEnginesExclude(uint32_t engines_exclude_mask) noexcept {
	return ::SRAL_SetEnginesExclude(static_cast<int>(engines_exclude_mask));
}

[[nodiscard]] inline std::optional<uint32_t> GetEnginesExclude() noexcept {
	const int result = ::SRAL_GetEnginesExclude();
	if SRAL_UNLIKELY (result == -1) {
		return std::nullopt;
	}
	return static_cast<uint32_t>(result);
}
} // namespace Sral

#endif // __cplusplus
#endif // SRAL_H_