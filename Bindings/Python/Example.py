import sys
import time
from ctypes import c_int, byref
from sral import Sral, SralEngine, SralFeature, SralParam, SralEngineCategory

def test_section(name: str):
    """Prints a highlighted structural separator for individual test phases."""
    print("\n\n========================================")
    print(f"  Testing Python Module: {name}")
    print("========================================")

def check(condition: bool, success_msg: str, fail_msg: str):
    """Logs simple true/false diagnostic validations across test boundaries."""
    print(f"[SUCCESS] {success_msg}" if condition else f"[FAILURE] {fail_msg}")

def check_sral(condition: bool, action_desc: str):
    """Validates unmanaged abstraction library execution feedback states."""
    print(f"[SUCCESS] {action_desc}" if condition else f"[FAILURE] {action_desc}")

def prompt_user(message: str):
    """Halts the primary thread loop and waits for developer confirmation."""
    input(f"\n>>> {message} (Press Enter to continue)...")

def print_supported_features(features: int):
    """Decodes and logs active unmanaged engine capability feature bitmasks."""
    print(f"Supported Features ({features}):")
    if features == 0:
        print("  (None)")
        return
    if features & SralFeature.speech:         print("  - SUPPORTS_SPEECH")
    if features & SralFeature.braille:        print("  - SUPPORTS_BRAILLE")
    if features & SralFeature.speechRate:     print("  - SUPPORTS_SPEECH_RATE")
    if features & SralFeature.speechVolume:   print("  - SUPPORTS_SPEECH_VOLUME")
    if features & SralFeature.selectVoice:    print("  - SUPPORTS_SELECT_VOICE")
    if features & SralFeature.pauseSpeech:     print("  - SUPPORTS_PAUSE_SPEECH")
    if features & SralFeature.ssml:           print("  - SUPPORTS_SSML")
    if features & SralFeature.speakToMemory:  print("  - SUPPORTS_SPEAK_TO_MEMORY")
    if features & SralFeature.spelling:       print("  - SUPPORTS_SPELLING")
    print("")

def print_engine_names(engine_bitmask: int, title: str):
    """Iterates over the runtime bitmask registry and prints discovered engine tokens."""
    print(f"{title}:")
    if engine_bitmask == SralEngine.none:
        print("  (None)\n")
        return
    found = False
    for i in range(1, 16):
        flag = 1 << i
        if engine_bitmask & flag:
            name = Sral.get_engine_name(flag)
            if not name: 
                name = "Unnamed Core Engine Target"
            print(f"  - {name} (0x{flag:X})")
            found = True
    if not found:
        print(f"  (Unknown bitmask representation: {engine_bitmask})")
    print("")

def main():
    print("SRAL Python ctypes Tester\n-------------------------")

    test_section("Sral.is_initialized (Before Initialization)")
    check(not Sral.is_initialized(), "is_initialized correctly returns False before init.", "is_initialized returned True before init!")

    test_section("Sral.initialize")
    engines_to_exclude = SralEngine.uia
    print(f"Attempting to initialize SRAL, excluding flag: {engines_to_exclude} ({Sral.get_engine_name(engines_to_exclude)})")

    if Sral.initialize(engines_to_exclude):
        print("[SUCCESS] SRAL Initialization complete.")
    else:
        print("[FAILURE] SRAL Initialization rejected by platform. Exiting.")
        return
    check(Sral.is_initialized(), "is_initialized correctly returns True after init.", "is_initialized returned False after init!")

    test_section("Engine Telemetry Information")
    print_engine_names(Sral.get_available_engines(), "Available Engines on this Platform")
    active_engines = Sral.get_active_engines()
    print_engine_names(active_engines, "Currently Active/Usable Engines")

    current_engine_id = Sral.get_current_engine()
    print(f"Current Default Engine: {Sral.get_engine_name(current_engine_id)} ({current_engine_id})")

    specific_engine_for_ex_tests = SralEngine.none
    for i in range(1, 16):
        flag = 1 << i
        if (active_engines & flag) and flag != current_engine_id:
            specific_engine_for_ex_tests = flag
            break
    if specific_engine_for_ex_tests == SralEngine.none and active_engines != SralEngine.none:
        for i in range(1, 16):
            flag = 1 << i
            if active_engines & flag:
                specific_engine_for_ex_tests = flag
                break

    if specific_engine_for_ex_tests != SralEngine.none:
        print(f"\nWill use engine '{Sral.get_engine_name(specific_engine_for_ex_tests)}' ({specific_engine_for_ex_tests}) for Ex tests.")
    else:
        print("\nNo distinct specific engine active for explicit Ex tests.")

    test_section("Keyboard Hooks Activation Layer")
    if Sral.register_keyboard_hooks():
        print("[SUCCESS] Global keyboard hooks registered.")
        prompt_user("Keyboard hooks active. Test them with upcoming speech checks.")
    else:
        print("[INFO] register_keyboard_hooks failed or unsupported on this platform driver.")

    test_section("Sral.get_engine_features")
    print(f"Features for Default Engine ({Sral.get_engine_name(current_engine_id)}):")
    current_engine_features = Sral.get_engine_features(SralEngine.none)
    print_supported_features(current_engine_features)

    if current_engine_features & SralFeature.speech:
        test_section("Sral.speak (Default Engine)")
        check_sral(Sral.speak("Testing SRAL Speak from Python, not interrupting.", False), "Sral.speak (no interrupt)")
        time.sleep(2)
        check_sral(Sral.speak("Testing SRAL Speak from Python, interrupting now.", True), "Sral.speak (interrupt)")
        time.sleep(2)

        if specific_engine_for_ex_tests != SralEngine.none:
            test_section("Sral.speak_ex (Specific Engine)")
            feat_ex = Sral.get_engine_features(specific_engine_for_ex_tests)
            if feat_ex & SralFeature.speech:
                check_sral(Sral.speak_ex(specific_engine_for_ex_tests, "Testing explicit sub-driver routing configurations using speak_ex.", False), "Sral.speak_ex (no interrupt)")
                time.sleep(2)

        test_section("Speech Flow Control (Pause / Resume / Stop)")
        long_speech = "This is a moderately long sentence configuration designed to validate pausing, resuming, and stopping workflows natively."
        print(f"Speaking sequence: \"{long_speech}\"")
        Sral.speak(long_speech, True)
        time.sleep(1)
        print(f"IsSpeaking checking loop flag status: {Sral.is_speaking()}")

        if current_engine_features & SralFeature.pauseSpeech:
            check_sral(Sral.pause_speech(), "Sral.pause_speech")
            prompt_user("Speech engine paused. Verify silence.")
            check_sral(Sral.resume_speech(), "Sral.resume_speech")
            time.sleep(1.5)
        check_sral(Sral.stop_speech(), "Sral.stop_speech")
        print(f"IsSpeaking status post stop instruction: {Sral.is_speaking()}")

    test_section("Dynamic Voice Queries & Allocation")
    voices = Sral.get_engine_voice_list(current_engine_id)
    if voices:
        print(f"Voices found count metric: {len(voices)}")
        for v in voices:
            print(f"  - Voice [{v.index}]: Name='{v.name}' Lang='{v.language}' Gender='{v.gender}' (Vendor: {v.vendor})")

        if len(voices) > 1 and (current_engine_features & SralFeature.selectVoice):
            print("Switching channel environment context to alternative system voice index 1...")
            target_idx = c_int(1)
            if Sral.set_engine_parameter(current_engine_id, SralParam.voiceIndex, byref(target_idx)):
                Sral.speak("This text evaluates speech using an alternative systemic voice profile configuration.", True)
                time.sleep(2.5)
                reset_idx = c_int(0)
                Sral.set_engine_parameter(current_engine_id, SralParam.voiceIndex, byref(reset_idx))

    test_section("SSML Parsing Verification")
    if current_engine_features & SralFeature.ssml:
        ssml_text = "<speak>Testing <break time=\"400ms\"/> structural SSML markup tags from Python.</speak>"
        check_sral(Sral.speak_ssml(ssml_text, True), "Sral.speak_ssml")
        time.sleep(3)

    test_section("Raw Audio Memory Generation Framework (speak_to_memory)")
    if current_engine_features & SralFeature.speakToMemory:
        print("Synthesizing raw string layouts into local application memory blocks...")
        with Sral.speak_to_memory("Audio serialization buffer check.") as pcm:
            if not pcm.is_empty:
                print("[SUCCESS] Managed data block generated successfully.")
                print(f"  Buffer Array Dimensions: {len(pcm.data)} bytes extracted via zero-copy view.")
                print(f"  Format Specs: Channels={pcm.channels} | Rate={pcm.sample_rate}Hz | Depth={pcm.bits_per_sample}-bit depth layer.")
            else:
                print("[FAILURE] speak_to_memory execution sequence faulted.")

    test_section("Tactile Braille Refresh & Combined Output Targets")
    if current_engine_features & SralFeature.braille:
        check_sral(Sral.braille("PYTHON BINDINGS"), "Sral.braille pin layout updates")
    check_sral(Sral.output("Unified distribution test tracking endpoint paths.", True), "Sral.output combined pipeline paths")
    time.sleep(2)

    test_section("Asynchronous Threaded Queue Loops (delay_output Methods)")
    if current_engine_features & SralFeature.speech:
        print("Dispatching speech items onto asynchronous background delay processing thread pipelines...")
        check_sral(Sral.delay_output(0, "Staged delay message number one.", True), "delay_output 1 (Flushing Queue Context Instantly)")
        check_sral(Sral.delay_output(1500, "Staged delay message number two.", False), "delay_output 2 (Staged Timing Enqueueing step)")
        print("Halting Python application thread loop execution context to give background loop worker threads room to deplete...")
    time.sleep(3.5)

    test_section("Dynamic Engine Exclusion List Adjustment Modifications")
    original_engines_to_exclude = Sral.get_engines_exclude()
    actual_exclude_mask = original_engines_to_exclude if original_engines_to_exclude is not None else SralEngine.none
    print(f"Current global exclusion tracking filter profile bitmask: 0x{actual_exclude_mask:X}")

    experimental_exclusion_mask = SralEngine.sapi | SralEngine.narrator
    print(f"Updating system filter bitmask parameters to: 0x{experimental_exclusion_mask:X}")

    if Sral.set_engines_exclude(experimental_exclusion_mask):
        freshly_fetched_exclusion_mask = Sral.get_engines_exclude() or SralEngine.none
        print(f"New dynamic filter bitmask profile value confirmed by engine get channel feedback: 0x{freshly_fetched_exclusion_mask:X}")
        check(freshly_fetched_exclusion_mask == experimental_exclusion_mask, "Dynamic profile exclusion changes verified successfully.", "Dynamic filter parameters failed value alignment validations!")
        Sral.set_engines_exclude(actual_exclude_mask)
    else:
        print("Native framework rejected dynamic exclusion tracking parameter adjustments.")

    test_section("Global Access Keyboard Hook Cleanup Deconstruction")
    Sral.unregister_keyboard_hooks()
    print("unregister_keyboard_hooks executed. Monitoring listener threads severed.")
    prompt_user("Keyboard hooks severed. Verify system transparency.")
    Sral.speak("Verifying systemic transparency after unregistering background keyboard listener thread contexts.", True)
    time.sleep(3)

    test_section("Core Library Uninitialization Framework Teardown")
    Sral.uninitialize()
    print("uninitialize function handle called. Releasing references.")
    check(not Sral.is_initialized(), "is_initialized accurately returns False following uninitialization.", "Teardown error tracking validation boundary failure!")

    print("\nAttempting to call speech synthesis routines post-uninitialization framework context teardown (Should safely evaluate as no-op return False):")
    if Sral.speak("This sentence should be caught by uninitialized guard blocks and drop silently.", False):
        print("[WARNING] speak wrapper returned True indicating potential framework state resource retention leaks!")
    else:
        print("[INFO] speak wrapper evaluated accurately and returned False inside uninitialized boundary bounds.")

    prompt_user("All Python integration verification suites executed completely. Press Enter to terminate process context.")

if __name__ == "__main__":
    main()
