#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SRAL_STATIC
#define SRAL_STATIC
#endif
#include <SRAL.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <stddef.h>

#if defined(_WIN32) || defined(_WIN64)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#include <threads.h>
#include <time.h>
#else
#include <time.h>
#endif

void sleep_ms(unsigned int milliseconds) {
#if defined(_WIN32) || defined(_WIN64)
	extern void __stdcall Sleep(unsigned long dwMilliseconds);
	Sleep((unsigned long)milliseconds);
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
	struct timespec ts;
	ts.tv_sec = (time_t)(milliseconds / 1000U);
	ts.tv_nsec = (long)((milliseconds % 1000U) * 1000000UL);
	(void)thrd_sleep(&ts, NULL);
#else
	struct timespec ts;
	ts.tv_sec = (time_t)(milliseconds / 1000U);
	ts.tv_nsec = (long)((milliseconds % 1000U) * 1000000UL);
	(void)nanosleep(&ts, NULL);
#endif
}

static void prompt_user(const char* const message) {
	printf("\n>>> %s (Press Enter to continue)...", message ? message : "Prompt");
	int c;
	while ((c = getchar()) != '\n' && c != EOF) {
	}

	if (c == EOF && feof(stdin)) {
		printf("EOF detected on stdin, continuing without prompt.\n");
	}
}
#define TEST_SECTION(name)                                                                                             \
	do {                                                                                                               \
		printf("\n\n========================================\n");                                                      \
		printf("  Testing: %s\n", (name));                                                                             \
		printf("========================================\n");                                                          \
	} while (0)

#define CHECK(condition, success_msg, fail_msg)                                                                        \
	do {                                                                                                               \
		if (condition) {                                                                                               \
			printf("[SUCCESS] %s\n", (success_msg));                                                                   \
		}                                                                                                              \
		else {                                                                                                         \
			printf("[FAILURE] %s\n", (fail_msg));                                                                      \
		}                                                                                                              \
	} while (0)

#define CHECK_SRAL(func_call, action_desc)                                                                             \
	do {                                                                                                               \
		const bool success_val = (func_call);                                                                          \
		if (success_val) {                                                                                             \
			printf("[SUCCESS] %s\n", (action_desc));                                                                   \
		}                                                                                                              \
		else {                                                                                                         \
			printf("[FAILURE] %s\n", (action_desc));                                                                   \
		}                                                                                                              \
	} while (0)

static void PrintEngineNames(const int engineBitmask, const char* const title) {
	printf("%s:\n", title ? title : "Engines");
	if (engineBitmask == SRAL_ENGINE_NONE) {
		printf("  (None)\n");
		return;
	}
	bool found = false;
	for (int engine_val = SRAL_ENGINE_NVDA; engine_val <= SRAL_ENGINE_CHROMEVOX; engine_val <<= 1) {
		if (engineBitmask & engine_val) {
			const char* const name = SRAL_GetEngineName(engine_val);
			printf("  - %s (0x%X)\n", name ? name : "Unknown Engine", engine_val);
			found = true;
		}
	}
	if (!found && engineBitmask != 0) {
		printf("  (Unknown bitmask: 0x%X)\n", engineBitmask);
	}
	printf("\n");
}

static const char* CategoryName(const int category) {
	switch (category) {
	case SRAL_ENGINE_CATEGORY_SCREEN_READER:
		return "Screen Reader";
	case SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE:
		return "Text-To-Speech Engine";
	case SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER:
		return "Accessibility Provider";
	default:
		return "Unknown";
	}
}
static void print_supported_features(const int features) {
	printf("Supported Features (0x%X):\n", features);
	if (features == 0) {
		printf("  (None)\n");
		return;
	}
	if (features & SRAL_SUPPORTS_SPEECH)
		printf("  - SUPPORTS_SPEECH\n");
	if (features & SRAL_SUPPORTS_BRAILLE)
		printf("  - SUPPORTS_BRAILLE\n");
	if (features & SRAL_SUPPORTS_SPEECH_RATE)
		printf("  - SUPPORTS_SPEECH_RATE\n");
	if (features & SRAL_SUPPORTS_SPEECH_VOLUME)
		printf("  - SUPPORTS_SPEECH_VOLUME\n");
	if (features & SRAL_SUPPORTS_SELECT_VOICE)
		printf("  - SUPPORTS_SELECT_VOICE\n");
	if (features & SRAL_SUPPORTS_PAUSE_SPEECH)
		printf("  - SUPPORTS_PAUSE_SPEECH\n");
	if (features & SRAL_SUPPORTS_SSML)
		printf("  - SUPPORTS_SSML\n");
	if (features & SRAL_SUPPORTS_SPEAK_TO_MEMORY)
		printf("  - SUPPORTS_SPEAK_TO_MEMORY\n");
	if (features & SRAL_SUPPORTS_SPELLING)
		printf("  - SUPPORTS_SPELLING\n");
	printf("\n");
}

int main(void) {
	printf("SRAL Cross-Platform Verification Tester\n");
	printf("----------------------------------------\n");

	TEST_SECTION("SRAL_IsInitialized (Before Initialization)");
	CHECK(!SRAL_IsInitialized(),
		"SRAL_IsInitialized correctly returns false before init.",
		"SRAL_IsInitialized returned true before init!");

	TEST_SECTION("SRAL_Initialize");
	int engines_to_exclude = SRAL_ENGINE_NONE;

#if defined(_WIN32)
	engines_to_exclude = SRAL_ENGINE_UIA;
#endif

	printf("Attempting to initialize SRAL, excluding engines: 0x%X (%s)\n",
		engines_to_exclude,
		SRAL_GetEngineName(engines_to_exclude) ? SRAL_GetEngineName(engines_to_exclude) : "None");

	if (SRAL_Initialize(engines_to_exclude)) {
		printf("[SUCCESS] SRAL_Initialize successful.\n");
	}
	else {
		printf("[FAILURE] SRAL_Initialize failed. Exiting.\n");
		return 1;
	}
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

	const bool at_active = (active_engines & at_engines) != 0;
	printf("Assistive tech currently active: %s\n\n", at_active ? "yes" : "no");

	CHECK((tts_engines & at_engines) == 0,
		"TTS and assistive-tech masks are disjoint.",
		"TTS and assistive-tech masks overlap!");

	printf("\nCategory of each available engine (SRAL_GetEngineCategory):\n");
	for (int e_val = SRAL_ENGINE_NVDA; e_val <= SRAL_ENGINE_CHROMEVOX; e_val <<= 1) {
		if (available_engines & e_val) {
			printf("  - %s: %s\n", SRAL_GetEngineName(e_val), CategoryName((int)SRAL_GetEngineCategory(e_val)));
		}
	}
	printf("\n");

	const int current_engine_id = SRAL_GetCurrentEngine();
	printf("Current Default Engine: %s (0x%X)\n",
		SRAL_GetEngineName(current_engine_id) ? SRAL_GetEngineName(current_engine_id) : "None/Unknown",
		current_engine_id);

	printf("\nNames of all SRAL_Engines enum members (as per SRAL_GetEngineName):\n");
	for (int e_val = SRAL_ENGINE_NVDA; e_val <= SRAL_ENGINE_CHROMEVOX; e_val <<= 1) {
		const char* const name = SRAL_GetEngineName(e_val);
		printf("  Engine ID 0x%X: %s\n", e_val, name ? name : "(Name not defined or not a single engine ID)");
	}

	int specific_engine_for_ex_tests = SRAL_ENGINE_NONE;
	if (active_engines != SRAL_ENGINE_NONE) {
		for (int e_val = SRAL_ENGINE_NVDA; e_val <= SRAL_ENGINE_CHROMEVOX; e_val <<= 1) {
			if ((active_engines & e_val) && e_val != current_engine_id) {
				specific_engine_for_ex_tests = e_val;
				break;
			}
		}
		if (specific_engine_for_ex_tests == SRAL_ENGINE_NONE) {
			for (int e_val = SRAL_ENGINE_NVDA; e_val <= SRAL_ENGINE_CHROMEVOX; e_val <<= 1) {
				if (active_engines & e_val) {
					specific_engine_for_ex_tests = e_val;
					break;
				}
			}
		}
	}

	if (specific_engine_for_ex_tests != SRAL_ENGINE_NONE) {
		printf("\nWill use engine '%s' (0x%X) for specific engine (Ex) tests.\n",
			SRAL_GetEngineName(specific_engine_for_ex_tests),
			specific_engine_for_ex_tests);
	}
	else {
		printf("\nNo specific engine distinct from default (or no active engines) for Ex tests.\n");
	}

	TEST_SECTION("Keyboard Hooks");
	if (SRAL_RegisterKeyboardHooks()) {
		printf("[SUCCESS] SRAL_RegisterKeyboardHooks registered.\n");
		prompt_user("Keyboard hooks active. Test Ctrl=Interrupt, Shift=Pause/Resume now.");
	}
	else {
		printf("[INFO] SRAL_RegisterKeyboardHooks failed or unsupported on this platform sandbox configuration.\n");
	}

	TEST_SECTION("SRAL_GetEngineFeatures");
	printf("Features for Current Default Engine (%s):\n",
		SRAL_GetEngineName(current_engine_id) ? SRAL_GetEngineName(current_engine_id) : "None");
	const int current_engine_features = SRAL_GetEngineFeatures(SRAL_ENGINE_NONE);
	print_supported_features(current_engine_features);

	if (specific_engine_for_ex_tests != SRAL_ENGINE_NONE) {
		printf("Features for Specific Engine selected for Ex tests (%s):\n",
			SRAL_GetEngineName(specific_engine_for_ex_tests));
		const int specific_engine_features = SRAL_GetEngineFeatures(specific_engine_for_ex_tests);
		print_supported_features(specific_engine_features);
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
			const int features_ex = SRAL_GetEngineFeatures(specific_engine_for_ex_tests);
			if (features_ex & SRAL_SUPPORTS_SPEECH) {
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
		const char* const ssml_test = "<speak>This is <prosody pitch='150%'>SSML</prosody> text.</speak>";
		CHECK_SRAL(SRAL_SpeakSsml(ssml_test, true), "SRAL_SpeakSsml");
		sleep_ms(3000);

		if (specific_engine_for_ex_tests != SRAL_ENGINE_NONE) {
			TEST_SECTION("SRAL_SpeakSsmlEx (Specific Engine)");
			const int features_ex = SRAL_GetEngineFeatures(specific_engine_for_ex_tests);
			if (features_ex & SRAL_SUPPORTS_SSML) {
				CHECK_SRAL(SRAL_SpeakSsmlEx(specific_engine_for_ex_tests, ssml_test, true), "SRAL_SpeakSsmlEx");
				sleep_ms(3000);
			}
		}
	}

	if (current_engine_features & SRAL_SUPPORTS_SPEAK_TO_MEMORY) {
		TEST_SECTION("SRAL_SpeakToMemory (Default Engine)");
		uint64_t buffer_size = 0;
		int channels = 0, sample_rate = 0, bits_per_sample = 0;
		void* pcm_buffer = SRAL_SpeakToMemory(
			"Testing speak to memory audio synthesis.", &buffer_size, &channels, &sample_rate, &bits_per_sample);
		if (pcm_buffer) {
			printf("[SUCCESS] SRAL_SpeakToMemory successful.\n");
			printf("  Buffer Size: %llu bytes\n", (unsigned long long)buffer_size);
			printf("  Channels: %d, Sample Rate: %d Hz, Bits: %d\n", channels, sample_rate, bits_per_sample);
			SRAL_free(pcm_buffer);
		}

		if (specific_engine_for_ex_tests != SRAL_ENGINE_NONE) {
			TEST_SECTION("SRAL_SpeakToMemoryEx (Specific Engine)");
			const int features_ex = SRAL_GetEngineFeatures(specific_engine_for_ex_tests);
			if (features_ex & SRAL_SUPPORTS_SPEAK_TO_MEMORY) {
				pcm_buffer = SRAL_SpeakToMemoryEx(specific_engine_for_ex_tests,
					"Testing speak to memory ex.",
					&buffer_size,
					&channels,
					&sample_rate,
					&bits_per_sample);
				if (pcm_buffer) {
					printf("[SUCCESS] SRAL_SpeakToMemoryEx successful.\n");
					SRAL_free(pcm_buffer);
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
			const int features_ex = SRAL_GetEngineFeatures(specific_engine_for_ex_tests);
			if (features_ex & SRAL_SUPPORTS_BRAILLE) {
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
		const char* const long_speech =
			"This is a moderately long sentence designed to test structural pausing functionality.";
		(void)SRAL_Speak(long_speech, true);
		prompt_user("Speech started. Press Enter to attempt PAUSE (if supported).");

		printf("IsSpeaking status: %s\n", SRAL_IsSpeaking() ? "true" : "false");

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
		printf("\nTesting SPEECH_RATE (Default Engine):\n");
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
				int diff = fetched_rate - new_rate;
				if (diff < 0)
					diff = -diff;
				CHECK(diff <= 5, "Rate set/get matches close enough", "Significant rate mismatch");
				(void)SRAL_SetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_SPEECH_RATE, &original_rate);
			}
		}
	}

	if (current_engine_features & SRAL_SUPPORTS_SPEECH_VOLUME) {
		printf("\nTesting SPEECH_VOLUME (Default Engine):\n");
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
				int diff = fetched_volume - new_volume;
				if (diff < 0)
					diff = -diff;
				CHECK(diff <= 5, "Volume set/get matches close enough", "Significant volume mismatch");
				(void)SRAL_SetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_SPEECH_VOLUME, &original_volume);
			}
		}
	}
	if (current_engine_features & SRAL_SUPPORTS_SELECT_VOICE) {
		printf("\nTesting VOICE parameters (Default Engine):\n");
		int voice_count = 0;
		if (SRAL_GetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_VOICE_COUNT, &voice_count) && voice_count > 0) {
			SRAL_VoiceInfo* voice_infos = (SRAL_VoiceInfo*)SRAL_malloc((size_t)voice_count * sizeof(SRAL_VoiceInfo));
			if (voice_infos && SRAL_GetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_VOICE_PROPERTIES, voice_infos)) {
				int original_voice_index = -1;
				(void)SRAL_GetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_VOICE_INDEX, &original_voice_index);

				if (voice_count > 1) {
					int new_voice_index = (original_voice_index + 1) % voice_count;
					if (SRAL_SetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_VOICE_INDEX, &new_voice_index)) {
						int current_voice_index = -1;
						(void)SRAL_GetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_VOICE_INDEX, &current_voice_index);
						CHECK(current_voice_index == new_voice_index,
							"Voice index tracking matches",
							"Voice index tracking mismatch");
						if (original_voice_index != -1) {
							(void)SRAL_SetEngineParameter(
								SRAL_ENGINE_NONE, SRAL_PARAM_VOICE_INDEX, &original_voice_index);
							printf("  Restored original voice index to: %d\n", original_voice_index);
						}
					}
				}
			}
			if (voice_infos) {
				SRAL_free(voice_infos);
			}
		}
	}

	printf("\nTesting ENABLE_SPELLING (Default Engine):\n");
	bool original_spelling_state = false;
	if (SRAL_GetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_ENABLE_SPELLING, &original_spelling_state)) {
		bool new_spelling_state = !original_spelling_state;
		if (SRAL_SetEngineParameter(SRAL_ENGINE_NONE, SRAL_PARAM_ENABLE_SPELLING, &new_spelling_state)) {
			bool spelling_enabled = false;
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
	engines_to_exclude = SRAL_ENGINE_NVDA | SRAL_ENGINE_SAPI;

	CHECK(SRAL_SetEnginesExclude(SRAL_ENGINE_NVDA | SRAL_ENGINE_SAPI),
		"Excludes successfully configured.",
		"Failed configuration write");
	const int new_engines_to_exclude = SRAL_GetEnginesExclude();
	printf("  New excludes confirmed by get: 0x%X\n", new_engines_to_exclude);
	CHECK(engines_to_exclude == new_engines_to_exclude,
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
	return 0;
}
