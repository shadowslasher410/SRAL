local sral = require("sral")

local function prompt_user(message)
    print(string.format("\n>>> %s (Press Enter to continue)...", message))
    io.read()
end

local function test_section(name)
    print("\n" .. string.rep("=", 40))
    print(string.format("  Testing: %s", name))
    print(string.rep("=", 40))
end

local function check(condition, success_msg, fail_msg)
    if condition then
        print(string.format("[SUCCESS] %s", success_msg))
    else
        print(string.format("[FAILURE] %s", fail_msg))
    end
end

local function check_sral(condition, action_desc)
    if condition then
        print(string.format("[SUCCESS] %s", action_desc))
    else
        print(string.format("[FAILURE] %s", action_desc))
    end
end

local function flag_status(fl)
    return fl and "Enabled" or "Disabled"
end

local function print_engine_names(engine_bitmask, title)
    print(title .. ":")
    if engine_bitmask == sral.Engines.NONE then
        print("  (None)\n")
        return
    end

    local found = false
    for name, value in pairs(sral.Engines) do
        if name ~= "NONE" and name ~= "CURRENT" and bit.band(engine_bitmask, value) ~= 0 then
            local engine_name = sral.get_engine_name(value)
            print(string.format("  - %s (%d)", engine_name, value))
            found = true
        end
    end

    if not found then
        print(string.format("  (Unknown bitmask: %d)", engine_bitmask))
    end
    print("")
end

local function print_supported_features(features)
    print(string.format("Supported Features (%d):", features))
    if features == sral.SupportedFeatures.NONE then
        print("  (None)\n")
        return
    end
    if bit.band(features, sral.SupportedFeatures.SPEECH) ~= 0 then print("  - SUPPORTS_SPEECH") end
    if bit.band(features, sral.SupportedFeatures.BRAILLE) ~= 0 then print("  - SUPPORTS_BRAILLE") end
    if bit.band(features, sral.SupportedFeatures.SPEECH_RATE) ~= 0 then print("  - SUPPORTS_SPEECH_RATE") end
    if bit.band(features, sral.SupportedFeatures.SPEECH_VOLUME) ~= 0 then print("  - SUPPORTS_SPEECH_VOLUME") end
    if bit.band(features, sral.SupportedFeatures.SELECT_VOICE) ~= 0 then print("  - SUPPORTS_SELECT_VOICE") end
    if bit.band(features, sral.SupportedFeatures.PAUSE_SPEECH) ~= 0 then print("  - SUPPORTS_PAUSE_SPEECH") end
    if bit.band(features, sral.SupportedFeatures.SSML) ~= 0 then print("  - SUPPORTS_SSML") end
    if bit.band(features, sral.SupportedFeatures.SPEAK_TO_MEMORY) ~= 0 then print("  - SUPPORTS_SPEAK_TO_MEMORY") end
    if bit.band(features, sral.SupportedFeatures.SPELLING) ~= 0 then print("  - SUPPORTS_SPELLING") end
    print("")
end

local function main()
    print("SRAL Tester")
    print("-------------------------")

    test_section("SRAL_IsInitialized (Before Initialization)")
    check(not sral.is_initialized(), "SRAL_IsInitialized correctly returns false before init.", "SRAL_IsInitialized returned true before init!")

    test_section("SRAL_Initialize")
    local engines_to_exclude = sral.Engines.NONE
    engines_to_exclude = sral.Engines.UIA
    print(string.format("Attempting to initialize SRAL, excluding engines: %d (%s)", engines_to_exclude, sral.get_engine_name(engines_to_exclude)))

    if sral.initialize(engines_to_exclude) then
        print("[SUCCESS] SRAL_Initialize successful.")
    else
        print("[FAILURE] SRAL_Initialize failed. Some tests may not run or behave as expected. Exiting.")
        return
    end
    check(sral.is_initialized(), "SRAL_IsInitialized correctly returns true after init.", "SRAL_IsInitialized returned false after init!")

    test_section("Engine Information")
    local available_engines = sral.get_available_engines()
    print_engine_names(available_engines, "Available Engines on this Platform")

    local active_engines = sral.get_active_engines()
    print_engine_names(active_engines, "Currently Active/Usable Engines")

    local current_engine_id = sral.get_current_engine()
    print(string.format("Current Default Engine: %s (%d)", sral.get_engine_name(current_engine_id), current_engine_id))

    print("\nNames of all SRAL_Engines enum members:")
    for name, value in pairs(sral.Engines) do
        if name ~= "NONE" and name ~= "CURRENT" then
            print(string.format("  Engine ID %d: %s", value, sral.get_engine_name(value)))
        end
    end

    local specific_engine_for_ex_tests = sral.Engines.NONE
    if active_engines ~= sral.Engines.NONE then
        for name, value in pairs(sral.Engines) do
            if name ~= "NONE" and name ~= "CURRENT" and bit.band(active_engines, value) ~= 0 and value ~= current_engine_id then
                specific_engine_for_ex_tests = value
                break
            end
        end
        if specific_engine_for_ex_tests == sral.Engines.NONE then
            for name, value in pairs(sral.Engines) do
                if name ~= "NONE" and name ~= "CURRENT" and bit.band(active_engines, value) ~= 0 then
                    specific_engine_for_ex_tests = value
                    break
                end
            end
        end
    end

    if specific_engine_for_ex_tests ~= sral.Engines.NONE then
        print(string.format("\nWill use engine '%s' (%d) for specific engine (Ex) tests.", sral.get_engine_name(specific_engine_for_ex_tests), specific_engine_for_ex_tests))
    else
        print("\nNo specific engine distinct from default for Ex tests.")
    end

    test_section("Keyboard Hooks")
    if sral.register_keyboard_hooks() then
        print("[SUCCESS] SRAL_RegisterKeyboardHooks registered.")
        prompt_user("Keyboard hooks (Ctrl=Interrupt, Shift=Pause/Resume) are active. Test them with upcoming speech.")
    else
        print("[INFO] SRAL_RegisterKeyboardHooks failed or did not register (Expected for mobile and screen readers).")
    end

    test_section("SRAL_GetEngineFeatures")
    print(string.format("Features for Current Default Engine (%s):", sral.get_engine_name(current_engine_id)))
    local current_engine_features = sral.get_engine_features(sral.Engines.NONE)
    print_supported_features(current_engine_features)

    if specific_engine_for_ex_tests ~= sral.Engines.NONE then
        print(string.format("Features for Specific Engine selected for Ex tests (%s):", sral.get_engine_name(specific_engine_for_ex_tests)))
        local specific_engine_features = sral.get_engine_features(specific_engine_for_ex_tests)
        print_supported_features(specific_engine_features)
    end

    if bit.band(current_engine_features, sral.SupportedFeatures.SPEECH) ~= 0 then
        test_section("SRAL_Speak (Default Engine)")
        check_sral(sral.speak("Testing SRAL Speak, not interrupting previous speech.", false), "SRAL_Speak (no interrupt)")
        sral.delay(2000)
        check_sral(sral.speak("Testing SRAL Speak, interrupting previous speech.", true), "SRAL_Speak (interrupt)")
        sral.delay(2000)

        if specific_engine_for_ex_tests ~= sral.Engines.NONE then
            test_section("SRAL_SpeakEx (Specific Engine)")
            local features_ex = sral.get_engine_features(specific_engine_for_ex_tests)
            if bit.band(features_ex, sral.SupportedFeatures.SPEECH) ~= 0 then
                check_sral(sral.speak_ex(specific_engine_for_ex_tests, "Testing SRAL SpeakEx, not interrupting.", false), "SRAL_SpeakEx (no interrupt)")
                sral.delay(2000)
                check_sral(sral.speak_ex(specific_engine_for_ex_tests, "Testing SRAL SpeakEx, interrupting.", true), "SRAL_SpeakEx (interrupt)")
                sral.delay(2000)
            else
                print(string.format("Specific engine %s does not support speech.", sral.get_engine_name(specific_engine_for_ex_tests)))
            end
        end
    else
        print("Current default engine does not support speech. Skipping speech tests.")
    end

    if bit.band(current_engine_features, sral.SupportedFeatures.SSML) ~= 0 then
        test_section("SRAL_SpeakSsml (Default Engine)")
        local ssml_test = "<speak>This is <prosody pitch='150%'>SSML</prosody> text.</speak>"
        check_sral(sral.speak_ssml(ssml_test, true), "SRAL_SpeakSsml")
        sral.delay(3000)

        if specific_engine_for_ex_tests ~= sral.Engines.NONE then
            test_section("SRAL_SpeakSsmlEx (Specific Engine)")
            local features_ex = sral.get_engine_features(specific_engine_for_ex_tests)
            if bit.band(features_ex, sral.SupportedFeatures.SSML) ~= 0 then
                check_sral(sral.speak_ssml_ex(specific_engine_for_ex_tests, ssml_test, true), "SRAL_SpeakSsmlEx")
                sral.delay(3000)
            else
                print(string.format("Specific engine %s does not support SSML.", sral.get_engine_name(specific_engine_for_ex_tests)))
            end
        end
    else
        print("Current default engine does not support SSML. Skipping SSML tests.")
    end

    if bit.band(current_engine_features, sral.SupportedFeatures.SPEAK_TO_MEMORY) ~= 0 then
        test_section("SRAL_SpeakToMemory (Default Engine)")
        local pcm_data = sral.speak_to_memory("Testing speak to memory audio synthesis.")
        if pcm_data then
            print("[SUCCESS] SRAL_SpeakToMemory successful.")
            print(string.format("  Buffer Size: %d bytes", pcm_data.size))
			print(string.format("  Channels: %d", pcm_data.channels))
			print(string.format("  Sample Rate: %d Hz", pcm_data.sample_rate))
			print(string.format("  Bits Per Sample: %d", pcm_data.bits_per_sample))
		else
			print("[FAILURE] SRAL_SpeakToMemory failed.")
			}
		if specific_engine_for_ex_tests ~= sral.Engines.NONE then
			test_section("SRAL_SpeakToMemoryEx (Specific Engine)")
			local features_ex = sral.get_engine_features(specific_engine_for_ex_tests)
		if bit.band(features_ex, sral.SupportedFeatures.SPEAK_TO_MEMORY) ~= 0 then
			local pcm_data_ex = sral.speak_to_memory_ex(specific_engine_for_ex_tests, "Testing speak to memory ex.")
		if pcm_data_ex then
			print(string.format("[SUCCESS] SRAL_SpeakToMemoryEx successful for engine %s.", sral.get_engine_name(specific_engine_for_ex_tests)))
			print(string.format("  Buffer Size: %d bytes", pcm_data_ex.size))
		else
			print(string.format("[FAILURE] SRAL_SpeakToMemoryEx failed for engine %s.", sral.get_engine_name(specific_engine_for_ex_tests)))
			end
		else
			print(string.format("Specific engine %s does not support Speak To Memory.", sral.get_engine_name(specific_engine_for_ex_tests)))
			end
			end
		else
			print("Current default engine does not support Speak To Memory. Skipping memory buffer tests.")end
		if bit.band(current_engine_features, sral.SupportedFeatures.BRAILLE) ~= 0 then
		test_section("SRAL_Braille (Default Engine)")
		prompt_user("Prepare to check Braille display for 'Testing SRAL Braille output.'")
		check_sral(sral.braille("Testing SRAL Braille output."), "SRAL_Braille")
		if specific_engine_for_ex_tests ~= sral.Engines.NONE then
		test_section("SRAL_BrailleEx (Specific Engine)")
		local features_ex = sral.get_engine_features(specific_engine_for_ex_tests)
		if bit.band(features_ex, sral.SupportedFeatures.BRAILLE) ~= 0 then
		prompt_user("Prepare to check Braille display for 'Testing SRAL Braille Ex output.'")
		check_sral(sral.braille_ex(specific_engine_for_ex_tests, "Testing SRAL Braille Ex output."), "SRAL_BrailleEx")
		else
		print(string.format("Specific engine %s does not support Braille.", sral.get_engine_name(specific_engine_for_ex_tests)))
		end
		end
		else
		print("Current default engine does not support Braille. Skipping Braille tests.")
		end
		test_section("SRAL_Output (Default Engine)")
		prompt_user("Prepare for SRAL_Output (Speech and/or Braille) for 'Testing SRAL Output, not interrupting.'")
		check_sral(sral.output("Testing SRAL Output, not interrupting.", false), "SRAL_Output (no interrupt)")
		sral.delay(2000)
		prompt_user("Prepare for SRAL_Output (Speech and/or Braille) for 'Testing SRAL Output, interrupting.'")
		check_sral(sral.output("Testing SRAL Output, interrupting now.", true), "SRAL_Output (interrupt)")
		sral.delay(2000)
		if specific_engine_for_ex_tests ~= sral.Engines.NONE then
		test_section("SRAL_OutputEx (Specific Engine)")
		prompt_user("Prepare for SRAL_OutputEx with specific engine for 'Testing SRAL OutputEx, not interrupting.'")
		check_sral(sral.output_ex(specific_engine_for_ex_tests, "Testing SRAL OutputEx, not interrupting.", false), "SRAL_OutputEx (no interrupt)")
		sral.delay(2000)prompt_user("Prepare for SRAL_OutputEx with specific engine for 'Testing SRAL OutputEx, interrupting.'")
		check_sral(sral.output_ex(specific_engine_for_ex_tests, "Testing SRAL OutputEx, interrupting now.", true), "SRAL_OutputEx (interrupt)")
		sral.delay(2000)
		end
		if bit.band(current_engine_features, sral.SupportedFeatures.SPEECH) ~= 0 then
		test_section("Speech Control (Default Engine)")
		local long_speech = "This is a moderately long sentence designed to test the pause, resume, and stop functionality of the SRAL library effectively."
		print(string.format("Speaking long sentence with default engine: "%s"", long_speech))
		sral.speak(long_speech, true)
		prompt_user("Speech started. Press Enter to attempt PAUSE.")
		print("IsSpeaking status: " .. tostring(sral.is_speaking()))
		if bit.band(current_engine_features, sral.SupportedFeatures.PAUSE_SPEECH) ~= 0 then
		check_sral(sral.pause_speech(), "SRAL_PauseSpeech")
		prompt_user("Speech Paused. Press Enter to attempt RESUME.")
		check_sral(sral.resume_speech(), "SRAL_ResumeSpeech")
		prompt_user("Speech Resumed. Press Enter to STOP.")
		else
		print("Pause/Resume not supported by default engine. Will attempt stop directly.")
		prompt_user("Speech should be ongoing. Press Enter to STOP.")
		end
		check_sral(sral.stop_speech(), "SRAL_StopSpeech")
		print("Speech should be stopped now.\n")
		sral.delay(500)
		if specific_engine_for_ex_tests ~= sral.Engines.NONE then
		test_section("Speech Control Ex (Specific Engine)")
		local features_ex = sral.get_engine_features(specific_engine_for_ex_tests)
		if bit.band(features_ex, sral.SupportedFeatures.SPEECH) ~= 0 then
		print(string.format("Speaking long sentence with engine %s: "%s"", sral.get_engine_name(specific_engine_for_ex_tests), long_speech))
		sral.speak_ex(specific_engine_for_ex_tests, long_speech, true)
		prompt_user("Speech started (Ex). Press Enter to PAUSE (Ex).")
		print("IsSpeaking status: " .. tostring(sral.is_speaking_ex(specific_engine_for_ex_tests)))
		if bit.band(features_ex, sral.SupportedFeatures.PAUSE_SPEECH) ~= 0
		then
		check_sral(sral.pause_speech_ex(specific_engine_for_ex_tests), "SRAL_PauseSpeechEx")
		prompt_user("Speech Paused (Ex). Press Enter to RESUME (Ex).")
		check_sral(sral.resume_speech_ex(specific_engine_for_ex_tests), "SRAL_ResumeSpeechEx")
		prompt_user("Speech Resumed (Ex). Press Enter to STOP (Ex).")
		else
		print(string.format("Pause/Resume not supported by specific engine %s. Attempting direct stop.", sral.get_engine_name(specific_engine_for_ex_tests)))
		prompt_user("Speech should be ongoing (Ex). Press Enter to STOP (Ex).")
		end
		check_sral(sral.stop_speech_ex(specific_engine_for_ex_tests), "SRAL_StopSpeechEx")
		print("Speech should be stopped (Ex).\n")
		sral.delay(500)
		end
		end
		end
		test_section("SRAL Engine Parameters (Default Engine)")
		if bit.band(current_engine_features, sral.SupportedFeatures.SPEECH_RATE) ~= 0 then
		print("\nTesting SPEECH_RATE (Default Engine):")
		local original_rate = sral.get_rate()if original_rate ~= -1 then
		print("  Original rate: " .. original_rate)
		local new_rate = (original_rate > 90) and (original_rate - 10) or (original_rate + 10)
		print("  Attempting to set rate to: " .. new_rate)
		if sral.set_rate(new_rate) then
		local fetched_rate = sral.get_rate()
		print("  New rate confirmed by get: " .. fetched_rate)
		check(math.abs(fetched_rate - new_rate) <= 5, "Rate set and get matches", "Rate set/get variance critical failure")
		sral.speak("Testing new speech rate setting.", true)
		sral.delay(2500)
		sral.set_rate(original_rate)
		end
		end
		end
		if bit.band(current_engine_features, sral.SupportedFeatures.SPEECH_VOLUME) ~= 0 then
		print("\nTesting SPEECH_VOLUME (Default Engine):")
		local original_volume = sral.get_volume()
		if original_volume ~= -1 then
		print("  Original volume: " .. original_volume)
		local new_volume = (original_volume > 90) and (original_volume - 10) or (original_volume + 10)
		print("  Attempting to set volume to: " .. new_volume)
		if sral.set_volume(new_volume) then
		local fetched_volume = sral.get_volume()
		print("  New volume confirmed by get: " .. fetched_volume)
		check(math.abs(fetched_volume - new_volume) <= 5, "Volume set and get matches", "Volume set/get variance critical failure")
		sral.speak("Testing new speech volume setting.", true)
		sral.delay(2500)
		sral.set_volume(original_volume)
		end
		end
		end
		if bit.band(current_engine_features, sral.SupportedFeatures.SELECT_VOICE) ~= 0 then
		print("\nTesting VOICE parameters (Default Engine):")
		local voice_count = sral.get_int_parameter(current_engine_id, sral.EngineParams.VOICE_COUNT)
		if voice_count > 0 then
		print("  Voice count: " .. voice_count)
		local voices = sral.get_voices(current_engine_id)
		for i, voice in ipairs(voices) do
		print(string.format("    %d: %s | Lang: %s | Gen: %s | Ven: %s", voice.index, voice.name, voice.language, voice.gender, voice.vendor))
		end
		local original_voice_index = sral.get_int_parameter(current_engine_id, sral.EngineParams.VOICE_INDEX)
		if voice_count > 1 then
		local new_voice_index = (original_voice_index + 1) % voice_count
		print(string.format("  Attempting to set voice to index: %d", new_voice_index))
		if sral.set_int_parameter(current_engine_id, sral.EngineParams.VOICE_INDEX, new_voice_index) then
		local current_voice = sral.get_int_parameter(current_engine_id, sral.EngineParams.VOICE_INDEX)
		check(current_voice == new_voice_index, "Voice index set/get matches", "Voice index mismatch")
		sral.speak("Testing newly selected voice.", true)
		sral.delay(3000)sral.set_int_parameter(current_engine_id, sral.EngineParams.VOICE_INDEX, original_voice_index)
		end
		end
		end
		end
		test_section("SRAL Platform Engine Telemetry & Exclusions")
		print("\nQuerying Categories and Active States of known engines:")
		for name, value in pairs(sral.Engines) do
		if name ~= "NONE" and name ~= "CURRENT" then
		local eng_name = sral.get_engine_name(value)
		if eng_name ~= "Unknown Engine" then
		local category = sral.get_engine_category(value)
		local active = sral.is_engine_active(value)
		local cat_str = "Unknown"
		if category == sral.EngineCategory.SCREEN_READER
		then cat_str = "Screen Reader"
		else
		if category == sral.EngineCategory.TEXT_TO_SPEECH_ENGINE
		then cat_str = "Text to Speech"
		else
		if category == sral.EngineCategory.ACCESSIBILITY_PROVIDER
		then cat_str = "Accessibility Provider"
		end
		print(string.format("  - Engine: %-25s | Category: %-22s | Active: %t", eng_name, cat_str, active))
		end
		end
		end
		test_section("SRAL Asynchronous Threaded Delay Queue Output")
		if bit.band(current_engine_features, sral.SupportedFeatures.SPEECH) ~= 0 then
		print("Dispatching speech items onto asynchronous background delay processing thread pipelines (Default Engine)...")
		check_sral(sral.delay_output("Staged delay message number one.", 0, true, true, false, false), "DelayOutput 1")
		check_sral(sral.delay_output("Staged delay message number two.", 1500, false, true, false, false), "DelayOutput 2")
		print("Waiting for default engine async thread loop processing context to exhaust...")
		sral.delay(3500)
		end
		test_section("Unified Multi-Platform Categories Validation")
		local tts_mask = sral.get_tts_engines()
		local at_mask = sral.get_assistive_tech_engines()
		print(string.format("Platform derived pure Text-to-Speech engines bitmask: 0x%X", tts_mask))
		print(string.format("Platform derived active Assistive Tech engines bitmask: 0x%X", at_mask)
		sral.set_engines_exclude(engines_to_exclude)test_section("Unregister Keyboard Hooks")
		sral.unregister_keyboard_hooks()
		print("SRAL_UnregisterKeyboardHooks called. Hooks should now be inactive.")
		prompt_user("Try Ctrl/Shift with next speech to confirm hooks are off.")
		sral.speak("Testing speech output after attempting to unregister keyboard hooks.", true)
		sral.delay(3000)
		test_section("SRAL_Uninitialize")
		sral.uninitialize()
		print("SRAL_Uninitialize called.")
		check(not sral.is_initialized(), "SRAL_IsInitialized correctly returns false after uninit.", "SRAL_IsInitialized returned true after uninit!")
		print("\nAttempting to speak after uninitialization (should fail or do nothing):")
		if sral.speak("This speech should not happen.", false)
		then
		print("[WARNING] SRAL_Speak appeared to succeed after uninitialization!")
		else
		print("[INFO] SRAL_Speak correctly failed or did nothing after uninitialization.")
		end
		prompt_user("All tests complete. Press Enter to exit.")
		end
		local function error_handling_demo()
		print("\n=== Error Handling Demo ===")
		local status, result = pcall(function()return sral.speak("This should fail", true)end)
		if status then
		print("Result: " .. tostring(result) .. " (should be false)")
		else
		print("Error caught: " .. tostring(result))
		end
		end
		local main_status, main_err = pcall(main)
		if not main_status then
		print("Demo failed with error: " .. tostring(main_err))
		end
		error_handling_demo()
