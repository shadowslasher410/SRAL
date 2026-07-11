use sral::*;
use std::io::{self, BufRead};
use std::thread;
use std::time::Duration;

fn test_section(name: &str) {
    println!("\n========================================");
    println!("  Testing: {}", name);
    println!("========================================");
}

fn check(condition: bool, pass_msg: &str, fail_msg: &str) {
    if condition {
        println!("[SUCCESS] {}", pass_msg);
    } else {
        println!("[FAILURE] {}", fail_msg);
    }
}

fn check_sral(condition: bool, action_desc: &str) {
    if condition {
        println!("[SUCCESS] {}", action_desc);
    } else {
        println!("[FAILURE] {}", action_desc);
    }
}

fn prompt_user(message: &str) {
    println!("\n>>> {} (Press Enter to continue)...", message);
    let mut iterator = io::stdin().lock().lines();
    let _ = iterator.next();
}

fn flag_status(fl: bool) -> &'static str {
    if fl { "Enabled" } else { "Disabled" }
}

fn main() {
    println!("SRAL Rust Tester\n-------------------------");
    
    test_section("is_initialized (Before Initialization)");
    check(!Sral::is_initialized(), "is_initialized correctly returns false before init.", "is_initialized returned true before init!");

    test_section("initialize");
    let engines_to_exclude = SRAL_ENGINE_UIA;
    println!("Attempting to initialize SRAL, excluding UIA Engine (Flag: 0x{:X})", engines_to_exclude);

    if Sral::initialize(engines_to_exclude) {
        println!("[SUCCESS] SRAL Initialization successful.");
    } else {
        println!("[FAILURE] SRAL Initialization failed. Exiting.");
        return;
    }
    check(Sral::is_initialized(), "is_initialized correctly returns true after init.", "is_initialized returned false after init!");

    test_section("Engine Telemetry Information");
    let available_engines = Sral::get_available_engines();
    let active_engines = Sral::get_active_engines();
    println!("Available Engines Profile Mask: 0x{:X}", available_engines);
    println!("Active Engines Profile Mask: 0x{:X}", active_engines);

    let current_engine_id = Sral::get_current_engine();
    println!("Current Default Engine: {} ({})", Sral::get_engine_name(current_engine_id), current_engine_id);

    println!("\nNames of all active system engine flags detected:");
    let engine_check_list = [
        SRAL_ENGINE_NVDA, SRAL_ENGINE_JAWS, SRAL_ENGINE_ZDSR, SRAL_ENGINE_NARRATOR,
        SRAL_ENGINE_UIA, SRAL_ENGINE_SAPI, SRAL_ENGINE_SPEECH_DISPATCHER,
        SRAL_ENGINE_VOICE_OVER, SRAL_ENGINE_NS_SPEECH, SRAL_ENGINE_AV_SPEECH,
        SRAL_ENGINE_ANDROID_ACCESSIBILITY_MANAGER, SRAL_ENGINE_ANDROID_TEXT_TO_SPEECH,
        SRAL_ENGINE_CHROMEVOX, SRAL_ENGINE_ORCA, SRAL_ENGINE_ACCESSKIT
    ];

    for &engine in &engine_check_list {
        if (available_engines & engine) != 0 {
            println!("  - {} (Flag: 0x{:X})", Sral::get_engine_name(engine), engine);
        }
    }

    let mut specific_engine_for_ex_tests = SRAL_ENGINE_NONE;
    for &engine in &engine_check_list {
        if (active_engines & engine) != 0 && engine != current_engine_id {
            specific_engine_for_ex_tests = engine;
            break;
        }
    }
    if specific_engine_for_ex_tests == SRAL_ENGINE_NONE && active_engines > 0 {
        for &engine in &engine_check_list {
            if (active_engines & engine) != 0 {
                specific_engine_for_ex_tests = engine;
                break;
            }
        }
    }

    test_section("Global Accessibility Keyboard Hooks");
    if Sral::register_keyboard_hooks() {
        println!("[SUCCESS] register_keyboard_hooks active. (Ctrl=Interrupt, Shift=Pause).");
    } else {
        println!("[INFO] register_keyboard_hooks skipped or failed.");
    }

    test_section("get_engine_features");
    let current_features = Sral::get_engine_features(SRAL_ENGINE_NONE);
    println!("Default Engine Features Mask: 0x{:X}", current_features);

    if (current_features & SRAL_FEATURE_SPEECH) != 0 {
        test_section("speak (Default Engine)");
        check_sral(Sral::speak("Testing SRAL Speak via Rust, not interrupting.", false), "speak (no interrupt)");
        Sral::delay(2000);
        check_sral(Sral::speak("Testing SRAL Speak via Rust, interrupting previous speech.", true), "speak (interrupt)");
        Sral::delay(2000);

        if specific_engine_for_ex_tests != SRAL_ENGINE_NONE {
            let feat_ex = Sral::get_engine_features(specific_engine_for_ex_tests);
            if (feat_ex & SRAL_FEATURE_SPEECH) != 0 {
                test_section("speak_ex (Specific Subsystem Engine)");
                check_sral(Sral::speak_ex(specific_engine_for_ex_tests, "Testing specific engine routing speak_ex.", true), "speak_ex routing validation");
                Sral::delay(2000);
            }
        }
    }

    test_section("Speech Controls (Pause / Resume / Stop)");
    if (current_features & SRAL_FEATURE_SPEECH) != 0 {
        let long_speech = "This is a moderately long sentence designed to test the pause, resume, and stop functionality.";
        Sral::speak(long_speech, true);
        Sral::delay(1000);
        println!("IsSpeaking baseline verification flag: {}", Sral::is_speaking());

        if (current_features & SRAL_FEATURE_PAUSE_SPEECH) != 0 {
            check_sral(Sral::pause_speech(), "pause_speech operation");
            Sral::delay(1500);
            check_sral(Sral::resume_speech(), "resume_speech operation");
            Sral::delay(1500);
        }
        check_sral(Sral::stop_speech(), "stop_speech operation");
    }

    test_section("Voice Configuration Matrix Properties Extraction");
    let voice_count = Sral::get_int_parameter(current_engine_id, SRAL_PARAM_VOICE_COUNT);
    if voice_count > 0 {
        println!("Total available native system voice records found: {}", voice_count);
        let voices = Sral::get_voices(current_engine_id);
        for (i, v) in voices.iter().enumerate() {
            println!("  Voice [{}]: {} [{}] (Gender: {}, Vendor: {})", i + 1, v.name, v.language, v.gender, v.vendor);
        }
    }

    test_section("SSML Generation Verification");
    if (current_features & SRAL_FEATURE_SSML) != 0 {
        let ssml_payload = "<speak>Testing <break time=\"450ms\"/> SSML parsing capabilities.</speak>";
        check_sral(Sral::speak_ssml(ssml_payload, true), "speak_ssml parsing validation");
        Sral::delay(3000);
    }

    test_section("Direct Linear Buffer Generation (speak_to_memory)");
    if (current_features & SRAL_FEATURE_SPEAK_TO_MEMORY) != 0 {
        if let Some(pcm) = Sral::speak_to_memory("Audio rendering buffer synthesis.") {
            println!("[SUCCESS] Captured {} bytes of PCM audio inside owned vector.", pcm.buffer.len());
            println!("  Format Properties: Channels={}, Rate={}Hz, Depth={}-bit depth", pcm.channels, pcm.sample_rate, pcm.bits_per_sample);
        }
    }

    test_section("Tactile Braille Refresh Output Channels");
    if (current_features & SRAL_FEATURE_BRAILLE) != 0 {
        check_sral(Sral::braille("RUST ENGINE ACTIVE"), "braille cell pinning");
    }
    check_sral(Sral::output("Final composite execution sequence test.", true), "output combined channel distribution");
    Sral::delay(2000);

        test_section("Asynchronous Threaded Queue Loops (delay_output Matrix)");
    if (current_features & SRAL_FEATURE_SPEECH) != 0 {
        println!("Dispatching speech items onto asynchronous background delay processing thread pipelines (Default Engine)...");
        check_sral(Sral::delay_output("Staged delay message number one.", 0, true, true, false, false), "delay_output element 1");
        check_sral(Sral::delay_output("Staged delay message number two.", 1500, false, true, false, false), "delay_output element 2");
        
        println!("Waiting for default engine async thread loop processing context to exhaust...");
        Sral::delay(3500);

        if specific_engine_for_ex_tests != SRAL_ENGINE_NONE {
            let feat_ex = Sral::get_engine_features(specific_engine_for_ex_tests);
            if (feat_ex & SRAL_FEATURE_SPEECH) != 0 {
                let name_ex = Sral::get_engine_name(specific_engine_for_ex_tests);
                println!("Dispatching speech items onto async delay queues for specific engine: {}...", name_ex);
                check_sral(Sral::delay_output_ex(specific_engine_for_ex_tests, "Staged delay message ex number one.", 0, true, true, false, false), "delay_output_ex element 1");
                check_sral(Sral::delay_output_ex(specific_engine_for_ex_tests, "Staged delay message ex number two.", 1500, false, true, false, false), "delay_output_ex element 2");
                
                println!("Waiting for specific engine {} async thread loop to exhaust...", name_ex);
                Sral::delay(3500);
            }
        }
    } else {
        println!("Speech feature not supported. Skipping asynchronous delay queue tests.");
    }

    test_section("System Platform Engine Categories Matrix Diagnostics");

    for &engine in &engine_check_list {
        let name = Sral::get_engine_name(engine);
        if name != "Unknown Engine" {
            println!("  - Engine: {:<25} | Category ID: {:<2} | Active Presence: {}", name, Sral::get_engine_category(engine), Sral::get_active_engines(engine));
        }
    }

    test_section("Dynamic Profile Exclusion Configurations Filter Updates");
    let original_exclude = Sral::get_engines_exclude();
    let test_exclude_mask = SRAL_ENGINE_SAPI | SRAL_ENGINE_NARRATOR;
    if Sral::set_engines_exclude(test_exclude_mask) {
        println!("Confirmed active filter exclusion bitmask: 0x{:X}", Sral::get_engines_exclude());
        Sral::set_engines_exclude(original_exclude);
    }

    test_section("Core Module Uninitialization Framework Teardown");
    Sral::unregister_keyboard_hooks();
    Sral::uninitialize();
    check(!Sral::is_initialized(), "uninitialize executed, references released cleanly.", "Teardown context validation failure!");

    println!("\nAttempting speech execution post uninitialization sequence:");
    if Sral::speak("This text should block cleanly.", false) {
        println!("[WARNING] speak returned true inside an uninitialized environment scope!");
    } else {
        println!("[INFO] speak wrapper evaluated accurately and returned false.");
    }

    error_handling_demo();
}

fn error_handling_demo() {
    test_section("Error Handling Scenario");
    println!("Attempting call invocation without running initialization pipelines...");
    let result = Sral::speak("This action block is bound to fail cleanly.", true);
	println!("Result feedback: {} (should evaluate to false)", result);
}
