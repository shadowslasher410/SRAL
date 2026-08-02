#include <assert.h>
#include <inttypes.h> // RESOLVED: Required for cross-platform PRIu64 specifiers
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Platform Detection Engine
#if defined(_WIN32) || defined(_WIN64)
#define SRAL_PLATFORM_WINDOWS 1
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define SRAL_PLATFORM_IOS 1
#else
#define SRAL_PLATFORM_MACOS 1
#endif
#elif defined(__ANDROID__)
#define SRAL_PLATFORM_ANDROID 1
#elif defined(__linux__)
#define SRAL_PLATFORM_LINUX 1
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#include <threads.h>
#define SRAL_HAS_THREADS 1
#endif

#ifndef SRAL_STATIC
#define SRAL_STATIC
#endif
#include <SRAL.h>

static_assert(sizeof(int) >= 4, "SRAL masks require a minimum 32-bit integer width");

// Strict Compiler Optimization Mapping
#if defined(__GNUC__) || defined(__clang__)
#define SRAL_INLINE inline __attribute__((always_inline))
#define SRAL_PURE __attribute__((pure))
#define SRAL_COLD __attribute__((cold))
#define SRAL_LIKELY(x) __builtin_expect(!!(x), 1)
#define SRAL_UNLIKELY(x) __builtin_expect(!!(x), 0)
#elif defined(_MSC_VER)
#define SRAL_INLINE __forceinline
#define SRAL_PURE
#define SRAL_COLD
#define SRAL_LIKELY(x) (x)
#define SRAL_UNLIKELY(x) (x)
#else
#define SRAL_INLINE inline
#define SRAL_PURE
#define SRAL_COLD
#define SRAL_LIKELY(x) (x)
#define SRAL_UNLIKELY(x) (x)
#endif

static SRAL_INLINE void sleep_ms(unsigned int milliseconds) {
#if defined(SRAL_HAS_THREADS)
	struct timespec ts = {
		.tv_sec = (time_t)(milliseconds / 1000U), .tv_nsec = (long)((milliseconds % 1000U) * 1000000UL)};
	(void)thrd_sleep(&ts, NULL);
#elif defined(SRAL_PLATFORM_WINDOWS)
	Sleep((DWORD)milliseconds);
#else
	struct timespec ts = {
		.tv_sec = (time_t)(milliseconds / 1000U), .tv_nsec = (long)((milliseconds % 1000U) * 1000000UL)};
	(void)nanosleep(&ts, NULL);
#endif
}

static void prompt_user(const char* const message) {
	fwrite("\n>>> ", 1, 5, stdout);
	fputs(message ? message : "Prompt", stdout);
	const char prompt_tail[] = " (Press Enter to continue)...";
	fwrite(prompt_tail, 1, sizeof(prompt_tail) - 1, stdout);
	fflush(stdout);

	int c;
	while ((c = getchar()) != '\n' && c != EOF)
		;

	if (SRAL_UNLIKELY(c == EOF && feof(stdin))) {
		const char eof_msg[] = "EOF detected on stdin, continuing without prompt.\n";
		fwrite(eof_msg, 1, sizeof(eof_msg) - 1, stdout);
	}
}

#define TEST_SECTION(name)                                                                                             \
	printf("\n\n========================================\n"                                                            \
		   "  Testing: %s\n"                                                                                           \
		   "========================================\n",                                                               \
		(name))

#define CHECK(condition, success_msg, fail_msg)                                                                        \
	do {                                                                                                               \
		if (SRAL_LIKELY(condition)) {                                                                                  \
			fwrite("[SUCCESS] " success_msg "\n", 1, sizeof("[SUCCESS] " success_msg "\n") - 1, stdout);               \
		}                                                                                                              \
		else {                                                                                                         \
			fwrite("[FAILURE] " fail_msg "\n", 1, sizeof("[FAILURE] " fail_msg "\n") - 1, stdout);                     \
		}                                                                                                              \
	} while (0)

#define CHECK_SRAL(func_call, action_desc)                                                                             \
	do {                                                                                                               \
		if (SRAL_LIKELY(func_call)) {                                                                                  \
			printf("[SUCCESS] %s\n", (action_desc));                                                                   \
		}                                                                                                              \
		else {                                                                                                         \
			printf("[FAILURE] %s\n", (action_desc));                                                                   \
		}                                                                                                              \
	} while (0)

static void PrintEngineNames(int engineBitmask, const char* const message_title) {
	fputs(message_title ? message_title : "Engines", stdout);
	fwrite(":\n", 1, 2, stdout);

	if (SRAL_UNLIKELY(engineBitmask == SRAL_ENGINE_NONE)) {
		fwrite("  (None)\n\n", 1, 10, stdout);
		return;
	}

	uint32_t mask = (uint32_t)engineBitmask;
	while (mask > 0) {
		uint32_t lowest_bit;
#if defined(__GNUC__) || defined(__clang__)
		lowest_bit = (uint32_t)__builtin_ctz(mask);
#elif defined(_MSC_VER)
		unsigned long index;
		_BitScanForward(&index, mask);
		lowest_bit = (uint32_t)index;
#else
		static const uint8_t de_bruijn_table[] = {
			0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
		};
		lowest_bit = de_bruijn_table[((mask & -mask) * 0x077CB531U) >> 27];
#endif
		const uint32_t engine_val = 1U << lowest_bit;
		const char* const name = SRAL_GetEngineName((int)engine_val);
		printf("  - %s (0x%X)\n", name ? name : "Unknown Engine", (unsigned int)engine_val);

		mask &= mask - 1;
	}
	putchar('\n');
}

SRAL_PURE static const char* CategoryName(int category) {
	switch (category) {
	case SRAL_ENGINE_CATEGORY_SCREEN_READER:
		return "Screen Reader";
	case SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE:
		return "Text-To-Speech Engine";
	case SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER:
		return "Accessibility Provider";
	default:
		break;
	}
	return "Unknown";
}

SRAL_COLD static void print_supported_features(int features) {
	printf("Supported Features (0x%X):\n", (unsigned int)features);
	if (SRAL_UNLIKELY(features == 0)) {
		fwrite("  (None)\n\n", 1, 10, stdout);
		return;
	}

	if (features & SRAL_SUPPORTS_SPEECH)
		fwrite("  - SUPPORTS_SPEECH\n", 1, 20, stdout);
	if (features & SRAL_SUPPORTS_BRAILLE)
		fwrite("  - SUPPORTS_BRAILLE\n", 1, 21, stdout);
	if (features & SRAL_SUPPORTS_SPEECH_RATE)
		fwrite("  - SUPPORTS_SPEECH_RATE\n", 1, 25, stdout);
	if (features & SRAL_SUPPORTS_SPEECH_VOLUME)
		fwrite("  - SUPPORTS_SPEECH_VOLUME\n", 1, 27, stdout);
	if (features & SRAL_SUPPORTS_SELECT_VOICE)
		fwrite("  - SUPPORTS_SELECT_VOICE\n", 1, 26, stdout);
	if (features & SRAL_SUPPORTS_PAUSE_SPEECH)
		fwrite("  - SUPPORTS_PAUSE_SPEECH\n", 1, 26, stdout);
	if (features & SRAL_SUPPORTS_SSML)
		fwrite("  - SUPPORTS_SSML\n", 1, 18, stdout);
	if (features & SRAL_SUPPORTS_SPEAK_TO_MEMORY)
		fwrite("  - SUPPORTS_SPEAK_TO_MEMORY\n", 1, 29, stdout);
	if (features & SRAL_SUPPORTS_SPELLING)
		fwrite("  - SUPPORTS_SPELLING\n", 1, 22, stdout);
	putchar('\n');
}

int main(void) {
	static char block_buffer[4096];
	setvbuf(stdout, block_buffer, _IOFBF, sizeof(block_buffer));

	const char intro_msg[] = "SRAL Cross-Platform Verification Tester\n----------------------------------------\n";
	fwrite(intro_msg, 1, sizeof(intro_msg) - 1, stdout);

	TEST_SECTION("SRAL_IsInitialized (Before Initialization)");
	CHECK(!SRAL_IsInitialized(),
		"SRAL_IsInitialized correctly returns false before init.",
		"SRAL_IsInitialized returned true before init!");

	TEST_SECTION("SRAL_Initialize");
	int engines_to_exclude = SRAL_ENGINE_NONE;

#if defined(SRAL_PLATFORM_WINDOWS)
	engines_to_exclude = SRAL_ENGINE_UIA;
#endif

	const char* const initial_engine_name = SRAL_GetEngineName(engines_to_exclude);
	printf("Attempting to initialize SRAL, excluding engines: 0x%X (%s)\n",
		(unsigned int)engines_to_exclude,
		initial_engine_name ? initial_engine_name : "None");

	const bool init_ok = SRAL_Initialize(engines_to_exclude);
	if (SRAL_UNLIKELY(!init_ok)) {
		const char init_fail_msg[] = "[FAILURE] SRAL_Initialize failed. Exiting.\n";
		fwrite(init_fail_msg, 1, sizeof(init_fail_msg) - 1, stdout);
		fflush(stdout);
		return EXIT_FAILURE;
	}

	const char init_success_msg[] = "[SUCCESS] SRAL_Initialize successful.\n";
	fwrite(init_success_msg, 1, sizeof(init_success_msg) - 1, stdout);

	CHECK(SRAL_IsInitialized(),
		"SRAL_IsInitialized correctly returns true after init.",
		"SRAL_IsInitialized returned false after init!");

	TEST_SECTION("Engine Information");
	const int available_engines = SRAL_GetAvailableEngines();
	PrintEngineNames(available_engines, "Available Engines on this Platform");

	const int active_engines = SRAL_GetActiveEngines();
	PrintEngineNames(active_engines, "Currently Active/Usable Engines");

	const int tts_engines = SRAL_GetTTSEngines();
	PrintEngineNames(tts_engines, "TTS Engines (category)");

	const int at_engines = SRAL_GetAssistiveTechEngines();
	PrintEngineNames(at_engines, "Assistive-Tech Engines (category)");

	const char at_msg_prefix[] = "Assistive tech currently active: ";
	fwrite(at_msg_prefix, 1, sizeof(at_msg_prefix) - 1, stdout);
	if (active_engines & at_engines) {
		fwrite("yes\n\n", 1, 5, stdout);
	}
	else {
		fwrite("no\n\n", 1, 4, stdout);
	}

	CHECK((tts_engines & at_engines) == 0,
		"TTS and assistive-tech masks are disjoint.",
		"TTS and assistive-tech masks overlap!");

	const char cat_msg_header[] = "\nCategory of each available engine (SRAL_GetEngineCategory):\n";
	fwrite(cat_msg_header, 1, sizeof(cat_msg_header) - 1, stdout);

	for (uint32_t shift = 0; shift < 32; ++shift) {
		const uint32_t e_val = 1U << shift;
		if (e_val & (uint32_t)available_engines) {
			printf(
				"  - %s: %s\n", SRAL_GetEngineName((int)e_val), CategoryName((int)SRAL_GetEngineCategory((int)e_val)));
		}
	}
	putchar('\n');

	const int current_engine_id = SRAL_GetCurrentEngine();
	const char* const current_engine_name = SRAL_GetEngineName(current_engine_id);
	printf("Current Default Engine: %s (0x%X)\n",
		current_engine_name ? current_engine_name : "None/Unknown",
		(unsigned int)current_engine_id);

	const char enum_msg_header[] = "\nNames of all SRAL_Engines enum members (as per SRAL_GetEngineName):\n";
	fwrite(enum_msg_header, 1, sizeof(enum_msg_header) - 1, stdout);

	for (uint32_t shift = 0; shift < 32; ++shift) {
		const uint32_t e_val = 1U << shift;
		const char* const name = SRAL_GetEngineName((int)e_val);
		printf("  Engine ID 0x%X: %s\n",
			(unsigned int)e_val,
			name ? name : "(Name not defined or not a single engine ID)");
	}

	int specific_engine_for_ex_tests = SRAL_ENGINE_NONE;
	if (active_engines != SRAL_ENGINE_NONE) {
		for (uint32_t shift = 0; shift < 32; ++shift) {
			const uint32_t e_val = 1U << shift;
			if (((uint32_t)active_engines & e_val) && (int)e_val != current_engine_id) {
				specific_engine_for_ex_tests = (int)e_val;
				break;
			}
		}
		if (specific_engine_for_ex_tests == SRAL_ENGINE_NONE) {
			for (uint32_t shift = 0; shift < 32; ++shift) {
				const uint32_t e_val = 1U << shift;
				if (e_val & (uint32_t)active_engines) {
					specific_engine_for_ex_tests = (int)e_val;
					break;
				}
			}
		}
	}

	if (specific_engine_for_ex_tests != SRAL_ENGINE_NONE) {
		printf("\nWill use engine '%s' (0x%X) for specific engine (Ex) tests.\n",
			SRAL_GetEngineName(specific_engine_for_ex_tests),
			(unsigned int)specific_engine_for_ex_tests);
	}
	else {
		const char no_engine_msg[] =
			"\nNo specific engine distinct from default (or no active engines) for Ex tests.\n";
		fwrite(no_engine_msg, 1, sizeof(no_engine_msg) - 1, stdout);
	}

	TEST_SECTION("Keyboard Hooks");
	if (SRAL_RegisterKeyboardHooks()) {
		const char hook_ok_msg[] = "[SUCCESS] SRAL_RegisterKeyboardHooks registered.\n";
		fwrite(hook_ok_msg, 1, sizeof(hook_ok_msg) - 1, stdout);
		prompt_user("Keyboard hooks active. Test Ctrl=Interrupt, Shift=Pause/Resume now.");
	}
	else {
		const char hook_fail_msg[] =
			"[INFO] SRAL_RegisterKeyboardHooks failed or unsupported on this platform sandbox configuration.\n";
		fwrite(hook_fail_msg, 1, sizeof(hook_fail_msg) - 1, stdout);
	}

	TEST_SECTION("SRAL_GetEngineFeatures");
	printf("Features for Current Default Engine (%s):\n", current_engine_name ? current_engine_name : "None");

	const int current_engine_features = SRAL_GetEngineFeatures(SRAL_ENGINE_NONE);
	print_supported_features(current_engine_features);

	const int specific_features =
		(specific_engine_for_ex_tests != SRAL_ENGINE_NONE) ? SRAL_GetEngineFeatures(specific_engine_for_ex_tests) : 0;

	if (specific_engine_for_ex_tests != SRAL_ENGINE_NONE) {
		printf("Features for Specific Engine selected for Ex tests (%s):\n",
			SRAL_GetEngineName(specific_engine_for_ex_tests));
		print_supported_features(specific_features);
	}

	if (current_engine_features & SRAL_SUPPORTS_SPEECH) {
		TEST_SECTION("SRAL_Speak (Default Engine)");
		CHECK_SRAL(
			SRAL_Speak("Testing SRAL Speak, not interrupting previous speech.", false), "SRAL_Speak (no interrupt)");
		sleep_ms(2000);
		CHECK_SRAL(SRAL_Speak("Testing SRAL Speak, interrupting previous speech.", true), "SRAL_Speak (interrupt)");
		sleep_ms(2000);

		if (specific_engine_for_ex_tests != SRAL_ENGINE_NONE) {
			TEST_SECTION("SRAL_SpeakEx (Specific Engine)");
			if (specific_features & SRAL_SUPPORTS_SPEECH) {
				CHECK_SRAL(SRAL_SpeakEx(specific_engine_for_ex_tests, "Testing SRAL SpeakEx, not interrupting.", false),
					"SRAL_SpeakEx (no interrupt)");
				sleep_ms(2000);
				CHECK_SRAL(SRAL_SpeakEx(specific_engine_for_ex_tests, "Testing SRAL SpeakEx, interrupting.", true),
					"SRAL_SpeakEx (interrupt)");
				sleep_ms(2000);
			}
		}
	}

	if (current_engine_features & SRAL_SUPPORTS_SSML) {
		TEST_SECTION("SRAL_SpeakSsml (Default Engine)");
		const char ssml_test[] = "<speak>This is <prosody pitch='150%'>SSML</prosody> text.</speak>";
		CHECK_SRAL(SRAL_SpeakSsml(ssml_test, true), "SRAL_SpeakSsml");
		sleep_ms(3000);

		if (specific_engine_for_ex_tests != SRAL_ENGINE_NONE) {
			TEST_SECTION("SRAL_SpeakSsmlEx (Specific Engine)");
			if (specific_features & SRAL_SUPPORTS_SSML) {
				CHECK_SRAL(SRAL_SpeakSsmlEx(specific_engine_for_ex_tests, ssml_test, true), "SRAL_SpeakSsmlEx");
				sleep_ms(3000);
			}
		}
	}

	if (current_engine_features & SRAL_SUPPORTS_SPEAK_TO_MEMORY) {
		TEST_SECTION("SRAL_SpeakToMemory (Default Engine)");
		uint64_t buffer_size = 0;
		int channels = 0, sample_rate = 0, bits_per_sample = 0;

		void* const pcm_buffer = SRAL_SpeakToMemory(
			"Testing speak to memory audio synthesis.", &buffer_size, &channels, &sample_rate, &bits_per_sample);

		if (pcm_buffer) {
			const char mem_success_msg[] = "[SUCCESS] SRAL_SpeakToMemory successful.\n";
			fwrite(mem_success_msg, 1, sizeof(mem_success_msg) - 1, stdout);
			printf("  Buffer Size: %" PRIu64 " bytes\n"
				   "  Channels: %d, Sample Rate: %d Hz, Bits: %d\n",
				buffer_size,
				channels,
				sample_rate,
				bits_per_sample);
			SRAL_free(pcm_buffer);
		}

		if (specific_engine_for_ex_tests != SRAL_ENGINE_NONE) {
			TEST_SECTION("SRAL_SpeakToMemoryEx (Specific Engine)");
			if (specific_features & SRAL_SUPPORTS_SPEAK_TO_MEMORY) {
				void* const pcm_buffer_ex = SRAL_SpeakToMemoryEx(specific_engine_for_ex_tests,
					"Testing speak to memory ex.",
					&buffer_size,
					&channels,
					&sample_rate,
					&bits_per_sample);
				if (pcm_buffer_ex) {
					const char mem_ex_success_msg[] = "[SUCCESS] SRAL_SpeakToMemoryEx successful.\n";
					fwrite(mem_ex_success_msg, 1, sizeof(mem_ex_success_msg) - 1, stdout);
					SRAL_free(pcm_buffer_ex);
				}
			}
		}
	}

	if (current_engine_features & SRAL_SUPPORTS_BRAILLE) {
		TEST_SECTION("SRAL_Braille (Default Engine)");
		prompt_user("Prepare to check Braille display output terminal lines.");
		CHECK_SRAL(SRAL_Braille("Testing SRAL Braille output."), "SRAL_Braille");

		if (specific_engine_for_ex_tests != SRAL_ENGINE_NONE) {
			TEST_SECTION("SRAL_BrailleEx (Specific Engine)");
			if (specific_features & SRAL_SUPPORTS_BRAILLE) {
				CHECK_SRAL(
					SRAL_BrailleEx(specific_engine_for_ex_tests, "Testing SRAL Braille Ex output."), "SRAL_BrailleEx");
			}
		}
	}

	TEST_SECTION("SRAL_Output (Default Engine)");
	CHECK_SRAL(SRAL_Output("Testing SRAL Output, not interrupting.", false), "SRAL_Output (no interrupt)");
	sleep_ms(2000);
	CHECK_SRAL(SRAL_Output("Testing SRAL Output, interrupting now.", true), "SRAL_Output (interrupt)");
	sleep_ms(2000);

	if (current_engine_features & SRAL_SUPPORTS_SPEECH) {
		TEST_SECTION("Speech Control (Default Engine)");
		const char long_speech[] =
			"This is a moderately long sentence designed to test structural pausing functionality.";
		(void)SRAL_Speak(long_speech, true);
		prompt_user("Speech started. Press Enter to attempt PAUSE (if supported).");

		const char status_prefix[] = "IsSpeaking status: ";
		fwrite(status_prefix, 1, sizeof(status_prefix) - 1, stdout);

		static const char* const bool_strings[] = {"false\n", "true\n"};
		const char* status_str = bool_strings[SRAL_IsSpeaking() ? 1 : 0];
		fputs(status_str, stdout);

		if (current_engine_features & SRAL_SUPPORTS_PAUSE_SPEECH) {
			CHECK_SRAL(SRAL_PauseSpeech(), "SRAL_PauseSpeech");
			prompt_user("Speech Paused. Press Enter to attempt RESUME.");
			CHECK_SRAL(SRAL_ResumeSpeech(), "SRAL_ResumeSpeech");
			prompt_user("Speech Resumed. Press Enter to STOP.");
		}
		CHECK_SRAL(SRAL_StopSpeech(), "SRAL_StopSpeech");
		sleep_ms(500);
	}

	TEST_SECTION("SRAL Engine Parameters (Default Engine)");

	if (current_engine_features & SRAL_SUPPORTS_SPEECH_RATE) {
		const char rate_msg_header[] = "\nTesting SPEECH_RATE (Default Engine):\n";
		fwrite(rate_msg_header, 1, sizeof(rate_msg_header) - 1, stdout);

		int original_rate = 0;
		if (SRAL_GetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_SPEECH_RATE, &original_rate)) {
			int new_rate = (original_rate <= 90) ? (original_rate + 10) : (original_rate - 10);
			if (new_rate < 0)
				new_rate = 0;
			if (new_rate > 100)
				new_rate = 100;

			if (SRAL_SetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_SPEECH_RATE, &new_rate)) {
				int fetched_rate = 0;
				(void)SRAL_GetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_SPEECH_RATE, &fetched_rate);

				CHECK(abs(fetched_rate - new_rate) <= 5,
					"Rate set/get matches close enough",
					"Significant rate mismatch");
				(void)SRAL_SetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_SPEECH_RATE, &original_rate);
			}
		}
	}

	if (current_engine_features & SRAL_SUPPORTS_SPEECH_VOLUME) {
		const char vol_msg_header[] = "\nTesting SPEECH_VOLUME (Default Engine):\n";
		fwrite(vol_msg_header, 1, sizeof(vol_msg_header) - 1, stdout);

		int original_volume = 0;
		if (SRAL_GetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_SPEECH_VOLUME, &original_volume)) {
			int new_volume = (original_volume <= 90) ? (original_volume + 10) : (original_volume - 10);
			if (new_volume < 0)
				new_volume = 0;
			if (new_volume > 100)
				new_volume = 100;
			if (SRAL_SetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_SPEECH_VOLUME, &new_volume)) {
				int fetched_volume = 0;
				(void)SRAL_GetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_SPEECH_VOLUME, &fetched_volume);
				CHECK(abs(fetched_volume - new_volume) <= 5,
					"Volume set/get matches close enough",
					"Significant volume mismatch");
				(void)SRAL_SetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_SPEECH_VOLUME, &original_volume);
			}
		}
	}
	if (current_engine_features & SRAL_SUPPORTS_SELECT_VOICE) {
		const char voice_msg_header[] = "\nTesting VOICE parameters (Default Engine):\n";
		fwrite(voice_msg_header, 1, sizeof(voice_msg_header) - 1, stdout);
		int voice_count = 0;
		if (SRAL_GetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_VOICE_COUNT, &voice_count) && voice_count > 0) {
			SRAL_VoiceInfo voice_scratch;
			SRAL_VoiceInfo* voice_infos = &voice_scratch;
			bool dynamic_alloc = false;

			if ((size_t)voice_count > 1) {
				voice_infos = (SRAL_VoiceInfo*)SRAL_malloc((size_t)voice_count * sizeof(SRAL_VoiceInfo));
				dynamic_alloc = true;
			}

			if (SRAL_LIKELY(voice_infos != NULL)) {
				if (SRAL_GetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_VOICE_PROPERTIES, voice_infos)) {
					int original_voice_index = -1;
					(void)SRAL_GetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_VOICE_INDEX, &original_voice_index);

					if (voice_count > 1) {
						const int new_voice_index = (original_voice_index + 1) % voice_count;
						if (SRAL_SetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_VOICE_INDEX, &new_voice_index)) {
							int current_voice_index = -1;
							(void)SRAL_GetEngineParameter(
								SRAL_ENGINE_NONE, SRAL_PARAM_VOICE_INDEX, &current_voice_index);
							CHECK(current_voice_index == new_voice_index,
								"Voice index tracking matches",
								"Voice index tracking mismatch");

							if (original_voice_index != -1) {
								(void)SRAL_SetEngineParameter(
									SRAL_ENGINE_NONE, SRAL_PARAM_VOICE_INDEX, &original_voice_index);
								const char restore_voice_msg[] = "  Restored original voice index.\n";
								fwrite(restore_voice_msg, 1, sizeof(restore_voice_msg) - 1, stdout);
							}
						}
					}
				}
				if (SRAL_UNLIKELY(dynamic_alloc)) {
					SRAL_free(voice_infos);
				}
			}
		}
	}

	const char spelling_msg_header[] = "\nTesting ENABLE_SPELLING (Default Engine):\n";
	fwrite(spelling_msg_header, 1, sizeof(spelling_msg_header) - 1, stdout);
	int original_spelling_state = 0;
	if (SRAL_GetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_ENABLE_SPELLING, &original_spelling_state)) {
		int new_spelling_state = !original_spelling_state;
		if (SRAL_SetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_ENABLE_SPELLING, &new_spelling_state)) {
			int spelling_enabled = 0;
			(void)SRAL_GetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_ENABLE_SPELLING, &spelling_enabled);
			CHECK(spelling_enabled == new_spelling_state, "Spelling state matches", "Spelling state mismatch");
			(void)SRAL_SetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_ENABLE_SPELLING, &original_spelling_state);
		}
	}

	TEST_SECTION("SRAL_Delay");
	(void)SRAL_Speak("First message.", true);
	SRAL_Delay(3000);
	(void)SRAL_Speak("Second message after delay.", true);
	(void)SRAL_StopSpeech();

	TEST_SECTION("SRAL_Set/GetEnginesExclude");
	const int original_engines_to_exclude = engines_to_exclude;
	int test_exclude_mask = SRAL_ENGINE_NVDA | SRAL_ENGINE_SAPI;

	CHECK(SRAL_SetEnginesExclude(test_exclude_mask), "Excludes successfully configured.", "Failed configuration write");

	const int new_engines_to_exclude = SRAL_GetEnginesExclude();
	printf("  New excludes confirmed by get: 0x%X\n", (unsigned int)new_engines_to_exclude);
	CHECK(test_exclude_mask == new_engines_to_exclude,
		"Excludes configuration verification matches",
		"Excludes configuration tracking failure");

	CHECK_SRAL(SRAL_SetEnginesExclude(SRAL_GetTTSEngines()), "Excluded the TTS engine category.");

	const int current_with_tts_excluded = SRAL_GetCurrentEngine();
	CHECK((current_with_tts_excluded & SRAL_GetTTSEngines()) == 0,
		"No TTS engine is active while excluded.",
		"TTS leak encountered");

	(void)SRAL_SetEnginesExclude(original_engines_to_exclude);

	TEST_SECTION("Unregister Keyboard Hooks");
	SRAL_UnregisterKeyboardHooks();

	TEST_SECTION("SRAL_Uninitialize");
	SRAL_Uninitialize();
	CHECK(!SRAL_IsInitialized(),
		"SRAL_IsInitialized accurately returned false after uninitialization.",
		"Uninitialization barrier failure");

	prompt_user("All structural verification tests complete. Press Enter to exit.");
	fflush(stdout);
	return EXIT_SUCCESS;
}