package.path = package.path .. ";./?.lua"

local bit = require("bit")
local sral = require("sral")

local function test_section(name)
    print("\n\n========================================")
    print("  Testing Lua Module: " .. name)
    print("========================================")
end

local function check(condition, success_msg, fail_msg)
    if condition then
        print("[SUCCESS] " .. success_msg)
    else
        print("[FAILURE] " .. fail_msg)
    end
end

local function check_sral(condition, action_desc)
    if condition then
        print("[SUCCESS] " .. action_desc)
    else
        print("[FAILURE] " .. action_desc)
    end
end

local function prompt_user(message)
    io.write("\n>>> " .. message .. " (Press Enter to continue)... ")
    io.flush()
    local _ = io.read()
end

local function print_supported_features(features)
    print("Supported Features (" .. tostring(features) .. "):")
    if features == 0 then
        print("  (None)")
        return
    end
    if bit.band(features, sral.Feature.SPEECH) ~= 0 then print("  - SUPPORTS_SPEECH") end
    if bit.band(features, sral.Feature.BRAILLE) ~= 0 then print("  - SUPPORTS_BRAILLE") end
    if bit.band(features, sral.Feature.SPEECH_RATE) ~= 0 then print("  - SUPPORTS_SPEECH_RATE") end
    if bit.band(features, sral.Feature.SPEECH_VOLUME) ~= 0 then print("  - SUPPORTS_SPEECH_VOLUME") end
    if bit.band(features, sral.Feature.SELECT_VOICE) ~= 0 then print("  - SUPPORTS_SELECT_VOICE") end
    if bit.band(features, sral.Feature.PAUSE_SPEECH) ~= 0 then print("  - SUPPORTS_PAUSE_SPEECH") end
    if bit.band(features, sral.Feature.SSML) ~= 0 then print("  - SUPPORTS_SSML") end
    if bit.band(features, sral.Feature.SPEAK_TO_MEMORY) ~= 0 then print("  - SUPPORTS_SPEAK_TO_MEMORY") end
    if bit.band(features, sral.Feature.SPELLING) ~= 0 then print("  - SUPPORTS_SPELLING") end
    print("")
end

local function print_engine_names(engine_bitmask, title)
    print(title .. ":")
    if engine_bitmask == sral.Engine.NONE then
        print("  (None)\n")
        return
    end
    local found = false
    for i = 1, 15 do
        local flag = bit.lshift(1, i)
        if bit.band(engine_bitmask, flag) ~= 0 then
            local name = sral.get_engine_name(flag)
            if name == "" then name = "Unnamed Core Engine Target" end
            print(string.format("  - %s (0x%X)", name, flag))
            found = true
        end
    end
    if not found then
        print("  (Unknown bitmask representation: " .. tostring(engine_bitmask) .. ")")
    end
    print("")
end

local function main()
    print("SRAL LuaJIT FFI Harness Tester\n-------------------------")

    test_section("sral.is_initialized (Before Initialization)")
    check(not sral.is_initialized(), "is_initialized correctly returns false before init.", "is_initialized returned true before init!")

    test_section("sral.initialize")
    local engines_to_exclude = sral.Engine.UIA
    print(string.format("Attempting to initialize SRAL, excluding flag: %d (%s)", engines_to_exclude, sral.get_engine_name(engines_to_exclude)))

    if sral.initialize(engines_to_exclude) then
        print("[SUCCESS] SRAL Initialization complete.")
    else
        print("[FAILURE] SRAL Initialization rejected by platform. Exiting.")
        return
    end
    check(sral.is_initialized(), "is_initialized correctly returns true after init.", "is_initialized returned false after init!")

    test_section("Engine Telemetry Information")
    print_engine_names(sral.get_available_engines(), "Available Engines on this Platform")
    local active_engines = sral.get_active_engines()
    print_engine_names(active_engines, "Currently Active/Usable Engines")

    local current_engine_id = sral.get_current_engine()
    print(string.format("Current Default Engine: %s (%d)", sral.get_engine_name(current_engine_id), current_engine_id))

    local specific_engine_for_ex_tests = sral.Engine.NONE
    for i = 1, 15 do
        local flag = bit.lshift(1, i)
        if bit.band(active_engines, flag) ~= 0 and flag ~= current_engine_id then
            specific_engine_for_ex_tests = flag
            break
        end
    end
    if specific_engine_for_ex_tests == sral.Engine.NONE and active_engines ~= sral.Engine.NONE then
        for i = 1, 15 do
            local flag = bit.lshift(1, i)
            if bit.band(active_engines, flag) ~= 0 then
                specific_engine_for_ex_tests = flag
                break
            end
        end
    end

    if specific_engine_for_ex_tests ~= sral.Engine.NONE then
        print(string.format("\nWill use engine '%s' (%d) for Ex tests.", sral.get_engine_name(specific_engine_for_ex_tests), specific_engine_for_ex_tests))
    else
        print("\nNo distinct specific engine active for explicit Ex tests.")
    end

    test_section("Keyboard Hooks Activation Layer")
    if sral.register_keyboard_hooks() then
        print("[SUCCESS] Global keyboard hooks registered.")
        prompt_user("Keyboard hooks active. Test them with upcoming speech checks.")
    else
        print("[INFO] register_keyboard_hooks failed or unsupported on this platform driver.")
    end

    test_section("sral.get_engine_features")
    print(string.format("Features for Default Engine (%s):", sral.get_engine_name(current_engine_id)))
    local current_engine_features = sral.get_engine_features(sral.Engine.NONE)
    print_supported_features(current_engine_features)

    if bit.band(current_engine_features, sral.Feature.SPEECH) ~= 0 then
        test_section("sral.speak (Default Engine)")
        check_sral(sral.speak("Testing SRAL Speak from Lua, not interrupting.", false), "sral.speak (no interrupt)")
        sral.delay(2000)
        check_sral(sral.speak("Testing SRAL Speak from Lua, interrupting now.", true), "sral.speak (interrupt)")
        sral.delay(2000)

        if specific_engine_for_ex_tests ~= sral.Engine.NONE then
            test_section("sral.speak_ex (Specific Engine)")
            local feat_ex = sral.get_engine_features(specific_engine_for_ex_tests)
            if bit.band(feat_ex, sral.Feature.SPEECH) ~= 0 then
                check_sral(sral.speak_ex(specific_engine_for_ex_tests, "Testing explicit sub-driver routing configurations using speak_ex.", false), "sral.speak_ex (no interrupt)")
                sral.delay(2000)
            end
        end

        test_section("Speech Flow Control (Pause / Resume / Stop)")
        local long_speech = "This is a moderately long sentence configuration designed to validate pausing, resuming, and stopping workflows natively."
        print(string.format("Speaking sequence: \"%s\"", long_speech))
        sral.speak(long_speech, true)
        sral.delay(1000)
        print(string.format("IsSpeaking checking loop flag status: %s", tostring(sral.is_speaking())))

        if bit.band(current_engine_features, sral.Feature.PAUSE_SPEECH) ~= 0 then
            check_sral(sral.pause_speech(), "sral.pause_speech")
            prompt_user("Speech engine paused. Verify silence.")
            check_sral(sral.resume_speech(), "sral.resume_speech")
            sral.delay(1500)
        end
        check_sral(sral.stop_speech(), "sral.stop_speech")
        print(string.format("IsSpeaking status post stop instruction: %s", tostring(sral.is_speaking())))
    end

    test_section("Dynamic Voice Queries & Allocation")
    local voices = sral.get_engine_voice_list(current_engine_id)
    if #voices > 0 then
        print(string.format("Voices found count metric: %d", #voices))
        for _, v in ipairs(voices) do
            print(string.format("  - Voice [%d]: Name='%s' Lang='%s' Gender='%s' (Vendor: %s)", v.index, v.name, v.language, v.gender, v.vendor))
        end

        if #voices > 1 and bit.band(current_engine_features, sral.Feature.SELECT_VOICE) ~= 0 then
            print("Switching channel environment context to alternative system voice index 1...")
            if sral.set_int_parameter(current_engine_id, sral.Param.VOICE_INDEX, 1) then
                sral.speak("This text evaluates speech using an alternative systemic voice profile configuration.", true)
                sral.delay(2500)
                sral.set_int_parameter(current_engine_id, sral.Param.VOICE_INDEX, 0)
            end
        end
    end

    test_section("SSML Parsing Verification")
    if bit.band(current_engine_features, sral.Feature.SSML) ~= 0 then
        local ssml_text = "<speak>Testing <break time=\"400ms\"/> structural SSML markup tags from Lua.</speak>"
        check_sral(sral.speak_ssml(ssml_text, true), "sral.speak_ssml")
        sral.delay(3000)
    end

    test_section("Raw Audio Memory Generation Framework (speak_to_memory)")
    if bit.band(current_engine_features, sral.Feature.SPEAK_TO_MEMORY) ~= 0 then
        print("Synthesizing raw string layouts into local application memory blocks...")
        local pcm = sral.speak_to_memory("Audio serialization buffer check.")
        if pcm then
            print("[SUCCESS] Managed data block generated successfully.")
            print(string.format("  Buffer Array Dimensions: %d bytes extracted via raw stream slice.", pcm.length))
            print(string.format("  Format Specs: Channels=%d | Rate=%dHz | Depth=%d-bit depth layer.", pcm.channels, pcm.sample_rate, pcm.bits_per_sample))
            pcm:free()
        else
            print("[FAILURE] speak_to_memory execution sequence faulted.")
        end
    end

    test_section("Tactile Braille Refresh & Combined Output Targets")
    if bit.band(current_engine_features, sral.Feature.BRAILLE) ~= 0 then
        check_sral(sral.braille("LUA BINDINGS"), "sral.braille pin layout updates")
    end
    check_sral(sral.output("Unified distribution test tracking endpoint paths.", true), "sral.output combined pipeline paths")
    sral.delay(2000)
    test_section("Asynchronous Threaded Queue Loops (delay_output Methods)")
    if bit.band(current_engine_features, sral.Feature.SPEECH) ~= 0 then
        print("Dispatching speech items onto asynchronous background delay processing thread pipelines...")
        check_sral(sral.delay_output(0, "Staged delay message number one.", true), "delay_output 1 (Flushing Queue Context Instantly)")
        check_sral(sral.delay_output(1500, "Staged delay message number two.", false), "delay_output 2 (Staged Timing Enqueueing step)")
        print("Halting Lua application execution context to give background processing loop worker threads room to deplete...")
        sral.delay(3500)

        if specific_engine_for_ex_tests ~= sral.Engine.NONE then
            local feat_ex = sral.get_engine_features(specific_engine_for_ex_tests)
            if bit.band(feat_ex, sral.Feature.SPEECH) ~= 0 then
                local name_ex = sral.get_engine_name(specific_engine_for_ex_tests)
                print(string.format("Staging text configurations onto async background queue workers targeting specific engine: %s...", name_ex))
                
                check_sral(sral.delay_output_ex(specific_engine_for_ex_tests, 0, "Explicitly targeted background queue message step one.", true), "delay_output_ex 1 (Flushing Explicit Target Instance)")
                check_sral(sral.delay_output_ex(specific_engine_for_ex_tests, 1500, "Explicitly targeted background queue message step two.", false), "delay_output_ex 2 (Staged Explicit Target Instance Timing Enqueueing)")
                
                print(string.format("Halting host script context execution frame to allow the explicit driver loop context (%s) to deplete thread stacks...", name_ex))
                sral.delay(3500)
            end
        end
    else
        print("Auditory speech delivery pipelines are disabled. Skipping asynchronous queue thread validations.")
    end

    test_section("Dynamic Engine Exclusion List Adjustment Modifications")
    local original_engines_to_exclude = sral.get_engines_exclude() or sral.Engine.NONE
    print(string.format("Current global exclusion tracking filter profile bitmask: 0x%X", original_engines_to_exclude))

    local experimental_exclusion_mask = bit.bor(sral.Engine.SAPI, sral.Engine.NARRATOR)
    print(string.format("Updating system filter bitmask parameters to: 0x%X", experimental_exclusion_mask))

    if sral.set_engines_exclude(experimental_exclusion_mask) then
        local freshly_fetched_exclusion_mask = sral.get_engines_exclude() or sral.Engine.NONE
        print(string.format("New dynamic filter bitmask profile value confirmed by engine get channel feedback: 0x%X", freshly_fetched_exclusion_mask))
        check(freshly_fetched_exclusion_mask == experimental_exclusion_mask, "Dynamic profile exclusion changes verified successfully.", "Dynamic filter parameters failed value alignment validations!")
        sral.set_engines_exclude(original_engines_to_exclude)
    else
        print("Native framework rejected dynamic exclusion tracking parameter adjustments.")
    end

    test_section("Global Access Keyboard Hook Cleanup Deconstruction")
    sral.unregister_keyboard_hooks()
    print("unregister_keyboard_hooks executed. Monitoring listener threads severed.")
    prompt_user("Keyboard hooks severed. Verify system transparency.")
    sral.speak("Verifying systemic transparency after unregistering background keyboard listener thread contexts.", true)
    sral.delay(3000)

    test_section("Core Library Uninitialization Framework Teardown")
    sral.uninitialize()
    print("uninitialize function handle called. Releasing references.")
    check(not sral.is_initialized(), "is_initialized accurately returns false following uninitialization.", "Teardown error tracking validation boundary failure!")

    print("\nAttempting to call speech synthesis routines post-uninitialization framework context teardown (Should safely evaluate as no-op return false):")
    if sral.speak("This sentence should be caught by uninitialized guard blocks and drop silently.", false) then
         print("[WARNING] speak wrapper returned true indicating potential framework state resource retention leaks!")
    else
         print("[INFO] speak wrapper evaluated accurately and returned false inside uninitialized boundary bounds.")
    end

    prompt_user("All Lua integration verification suites executed completely. Press Enter to terminate process context.")
end

main()

