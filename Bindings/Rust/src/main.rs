use sral::{
    PCMData, Sral, VoiceInfo, SRAL_ENGINE_NARRATOR, SRAL_ENGINE_NONE, SRAL_ENGINE_SAPI,
    SRAL_ENGINE_UIA, SRAL_FEATURE_BRAILLE, SRAL_FEATURE_PAUSE_SPEECH, SRAL_FEATURE_SELECT_VOICE,
    SRAL_FEATURE_SPEAK_TO_MEMORY, SRAL_FEATURE_SPEECH, SRAL_FEATURE_SSML, SRAL_PARAM_VOICE_INDEX,
};
use std::io::{self, BufRead};
use std::thread;
use std::time::Duration;

fn test_section(name: &str) {
    println!("\n\n========================================");
    println!("  Testing Rust Module: {}", name);
    println!("========================================");
}

fn check(condition: bool, success_msg: &str, fail_msg: &str) {
    if condition {
        println!("[SUCCESS] {}", success_msg);
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
    let stdin = io::stdin();
    let mut iterator = stdin.lock().lines();
    let _ = iterator.next();
}

fn print_supported_features(features: i32) {
    println!("Supported Features ({}):", features);
    if features == 0 {
        println!("  (None)");
        return;
    }
    if (features & SRAL_FEATURE_SPEECH) != 0 {
        println!("  - SUPPORTS_SPEECH");
    }
    if (features & SRAL_FEATURE_BRAILLE) != 0 {
        println!("  - SUPPORTS_BRAILLE");
    }
    if (features & SRAL_FEATURE_SPEECH_RATE) != 0 {
        println!("  - SUPPORTS_SPEECH_RATE");
    }
    if (features & SRAL_FEATURE_SPEECH_VOLUME) != 0 {
        println!("  - SUPPORTS_SPEECH_VOLUME");
    }
    if (features & SRAL_FEATURE_SELECT_VOICE) != 0 {
        println!("  - SUPPORTS_SELECT_VOICE");
    }
    if (features & SRAL_FEATURE_PAUSE_SPEECH) != 0 {
        println!("  - SUPPORTS_PAUSE_SPEECH");
    }
    if (features & SRAL_FEATURE_SSML) != 0 {
        println!("  - SUPPORTS_SSML");
    }
    if (features & SRAL_FEATURE_SPEAK_TO_MEMORY) != 0 {
        println!("  - SUPPORTS_SPEAK_TO_MEMORY");
    }
    if (features & SRAL_FEATURE_SPELLING) != 0 {
        println!("  - SUPPORTS_SPELLING");
    }
    println!();
}

fn print_engine_names(engine_bitmask: i32, title: &str) {
    println!("{}:", title);
    if engine_bitmask == SRAL_ENGINE_NONE {
        println!("  (None)\n");
        return;
    }
    let mut found = false;
    for i in 1..=15 {
        let flag = 1 << i;
        if (engine_bitmask & flag) != 0 {
            let name = Sral::get_engine_name(flag);
            println!("  - {} (0x{:X})", name, flag);
            found = true;
        }
    }
    if !found {
        println!("  (Unknown bitmask representation: {})", engine_bitmask);
    }
    println!();
}

fn main() {
    println!("SRAL Rust FFI Harness Tester\n-------------------------");

    test_section("Sral::is_initialized (Before Initialization)");
    check(
        !Sral::is_initialized(),
        "is_initialized correctly returns false before init.",
        "is_initialized returned true before init!",
    );

    test_section("Sral::initialize");
    let engines_to_exclude = SRAL_ENGINE_UIA;
    println!(
        "Attempting to initialize SRAL, excluding flag: {} ({})",
        engines_to_exclude,
        Sral::get_engine_name(engines_to_exclude)
    );

    if Sral::initialize(engines_to_exclude) {
        println!("[SUCCESS] SRAL Initialization complete.");
    } else {
        println!("[FAILURE] SRAL Initialization rejected by platform. Exiting.");
        return;
    }
    check(
        Sral.is_initialized(),
        "is_initialized correctly returns true after init.",
        "is_initialized returned false after init!",
    );

    test_section("Engine Telemetry Information");
    print_engine_names(
        Sral.get_available_engines(),
        "Available Engines on this Platform",
    );
    let active_engines = Sral.get_active_engines();
    print_engine_names(active_engines, "Currently Active/Usable Engines");

    let current_engine_id = Sral.get_current_engine();
    println!(
        "Current Default Engine: {} ({})",
        Sral.get_engine_name(current_engine_id),
        current_engine_id
    );

    let mut specific_engine_for_ex_tests = SRAL_ENGINE_NONE;
    for i in 1..=15 {
        let flag = 1 << i;
        if (active_engines & flag) != 0 && flag != current_engine_id {
            specific_engine_for_ex_tests = flag;
            break;
        }
    }
    if specific_engine_for_ex_tests == SRAL_ENGINE_NONE && active_engines != SRAL_ENGINE_NONE {
        for i in 1..=15 {
            let flag = 1 << i;
            if (active_engines & flag) != 0 {
                specific_engine_for_ex_tests = flag;
                break;
            }
        }
    }

    if specific_engine_for_ex_tests != SRAL_ENGINE_NONE {
        println!(
            "\nWill use engine '{}' ({}) for Ex tests.",
            Sral.get_engine_name(specific_engine_for_ex_tests),
            specific_engine_for_ex_tests
        );
    } else {
        println!("\nNo distinct specific engine active for explicit Ex tests.");
    }

    test_section("Keyboard Hooks Activation Layer");
    if Sral::register_keyboard_hooks() {
        println!("[SUCCESS] Global keyboard hooks registered.");
        prompt_user("Keyboard hooks active. Test them with upcoming speech checks.");
    } else {
        println!("[INFO] register_keyboard_hooks failed or unsupported on this platform driver.");
    }

    test_section("Sral::get_engine_features");
    println!(
        "Features for Default Engine ({}):",
        Sral.get_engine_name(current_engine_id)
    );
    let current_engine_features = Sral::get_engine_features(SRAL_ENGINE_NONE);
    print_supported_features(current_engine_features);

    if (current_engine_features & SRAL_FEATURE_SPEECH) != 0 {
        test_section("Sral::speak (Default Engine)");
        check_sral(
            Sral::speak("Testing SRAL Speak from Rust, not interrupting.", false),
            "Sral::speak (no interrupt)",
        );
        thread::sleep(Duration::from_secs(2));
        check_sral(
            Sral::speak("Testing SRAL Speak from Rust, interrupting now.", true),
            "Sral::speak (interrupt)",
        );
        thread::sleep(Duration::from_secs(2));

        if specific_engine_for_ex_tests != SRAL_ENGINE_NONE {
            test_section("Sral::speak_ex (Specific Engine)");
            let feat_ex = Sral::get_engine_features(specific_engine_for_ex_tests);
            if (feat_ex & SRAL_FEATURE_SPEECH) != 0 {
                check_sral(
                    Sral::speak_ex(
                        specific_engine_for_ex_tests,
                        "Testing explicit sub-driver routing configurations using speak_ex.",
                        false,
                    ),
                    "Sral::speak_ex (no interrupt)",
                );
                thread::sleep(Duration::from_secs(2));
            }
        }

        test_section("Speech Flow Control (Pause / Resume / Stop)");
        let long_speech = "This is a moderately long sentence configuration designed to validate pausing, resuming, and stopping workflows natively.";
        println!("Speaking sequence: \"{}\"", long_speech);
        Sral::speak(long_speech, true);
        thread::sleep(Duration::from_secs(1));
        println!(
            "IsSpeaking checking loop flag status: {}",
            Sral::is_speaking()
        );

        if (current_engine_features & SRAL_FEATURE_PAUSE_SPEECH) != 0 {
            check_sral(Sral::pause_speech(), "Sral::pause_speech");
            prompt_user("Speech engine paused. Verify silence.");
            check_sral(Sral::resume_speech(), "Sral::resume_speech");
            thread::sleep(Duration::from_millis(1500));
        }
        check_sral(Sral.stop_speech(), "Sral::stop_speech");
        println!(
            "IsSpeaking status post stop instruction: {}",
            Sral::is_speaking()
        );
    }

    test_section("Dynamic Voice Queries & Allocation");
    let voices = Sral::get_engine_voice_list(current_engine_id);
    if !voices.is_empty() {
        println!("Voices found count metric: {}", voices.len());
        for v in &voices {
            println!(
                "  - Voice [{}]: Name='{}' Lang='{}' Gender='{}' (Vendor: {})",
                v.index, v.name, v.language, v.gender, v.vendor
            );
        }

        if voices.len() > 1 && (current_engine_features & SRAL_FEATURE_SELECT_VOICE) != 0 {
            println!(
                "Switching channel environment context to alternative system voice index 1..."
            );
            if Sral::set_int_parameter(current_engine_id, SRAL_PARAM_VOICE_INDEX, 1) {
                Sral::speak("This text evaluates speech using an alternative systemic voice profile configuration.", true);
                thread::sleep(Duration::from_millis(2500));
                Sral::set_int_parameter(current_engine_id, SRAL_PARAM_VOICE_INDEX, 0);
            }
        }
    }

    test_section("SSML Parsing Verification");
    if (current_engine_features & SRAL_FEATURE_SSML) != 0 {
        let ssml_text =
            "<speak>Testing <break time=\"400ms\"/> structural SSML markup tags from Rust.</speak>";
        check_sral(Sral::speak_ssml(ssml_text, true), "Sral::speak_ssml");
        thread::sleep(Duration::from_secs(3));
    }

    test_section("Raw Audio Memory Generation Framework (speak_to_memory)");
    if (current_engine_features & SRAL_FEATURE_SPEAK_TO_MEMORY) != 0 {
        println!("Synthesizing raw string layouts into local application memory blocks...");
        if let Some(pcm) = Sral::speak_to_memory("Audio serialization buffer check.") {
            println!("[SUCCESS] Managed data block generated successfully.");
            println!(
                "  Buffer Array Dimensions: {} bytes extracted.",
                pcm.buffer.len()
            );
            println!(
                "  Format Specs: Channels={} | Rate={}Hz | Depth={}-bit.",
                pcm.channels, pcm.sample_rate, pcm.bits_per_sample
            );
        } else {
            println!("[FAILURE] speak_to_memory execution sequence faulted.");
        }
    }

    test_section("Tactile Braille Refresh & Combined Output Targets");
    if (current_engine_features & SRAL_FEATURE_BRAILLE) != 0 {
        check_sral(
            Sral::braille("RUST BINDINGS"),
            "Sral::braille pin layout updates",
        );
    }
    check_sral(
        Sral::output("Unified distribution test tracking endpoint paths.", true),
        "Sral::output combined pipeline paths",
    );
    thread::sleep(Duration::from_secs(2));

    test_section("Asynchronous Threaded Queue Loops (delay_output Methods)");
    if (current_engine_features & SRAL_FEATURE_SPEECH) != 0 {
        println!("Dispatching speech items onto asynchronous background delay processing thread pipelines...");

        check_sral(
            Sral::delay_output(
                "Staged delay message number one.",
                0,
                true,
                true,
                false,
                false,
            ),
            "delay_output 1 (Flushing Queue Context Instantly)",
        );
        check_sral(
            Sral::delay_output(
                "Staged delay message number two.",
                1500,
                false,
                true,
                false,
                false,
            ),
            "delay_output 2 (Staged Timing Enqueueing step)",
        );

        println!("Halting Rust application thread context to give background loop worker threads room to deplete...");
        thread::sleep(Duration::from_millis(3500));

        if specific_engine_for_ex_tests != SRAL_ENGINE_NONE {
            let feat_ex = Sral::get_engine_features(specific_engine_for_ex_tests);
            if (feat_ex & SRAL_FEATURE_SPEECH) != 0 {
                let name_ex = Sral::get_engine_name(specific_engine_for_ex_tests);
                println!("Staging text configurations onto async background queue workers targeting specific engine: {}...", name_ex);

                check_sral(
                    Sral::delay_output_ex(
                        specific_engine_for_ex_tests,
                        "Explicitly targeted background queue message step one.",
                        0,
                        true,
                        true,
                        false,
                        false,
                    ),
                    "delay_output_ex 1 (Flushing Explicit Target Instance)",
                );
                check_sral(
                    Sral::delay_output_ex(
                        specific_engine_for_ex_tests,
                        "Explicitly targeted background queue message step two.",
                        1500,
                        false,
                        true,
                        false,
                        false,
                    ),
                    "delay_output_ex 2 (Staged Explicit Target Instance Timing Enqueueing)",
                );

                println!("Halting Go thread frame to allow the explicit driver loop context ({}) to deplete thread stacks...", name_ex);
                thread::sleep(Duration::from_millis(3500));
            }
        }
    } else {
        println!("Auditory speech delivery pipelines are disabled. Skipping asynchronous queue thread validations.");
    }

    test_section("Dynamic Engine Exclusion List Adjustment Modifications");
    let original_engines_to_exclude = Sral::get_engines_exclude();
    println!(
        "Current global exclusion tracking filter profile bitmask: 0x{:X}",
        original_engines_to_exclude
    );

    let experimental_exclusion_mask = SRAL_ENGINE_SAPI | SRAL_ENGINE_NARRATOR;
    println!(
        "Updating system filter bitmask parameters to: 0x{:X}",
        experimental_exclusion_mask
    );

    if Sral::set_engines_exclude(experimental_exclusion_mask) {
        let freshly_fetched_exclusion_mask = Sral::get_engines_exclude();
        println!("New dynamic filter bitmask profile value confirmed by engine get channel feedback: 0x{:X}", freshly_fetched_exclusion_mask);
        check(
            freshly_fetched_exclusion_mask == experimental_exclusion_mask,
            "Dynamic profile exclusion changes verified successfully.",
            "Dynamic filter parameters failed value alignment validations!",
        );
        Sral::set_engines_exclude(original_engines_to_exclude);
    } else {
        println!("Native framework rejected dynamic exclusion tracking parameter adjustments.");
    }

    test_section("Global Access Keyboard Hook Cleanup Deconstruction");
    Sral::unregister_keyboard_hooks();
    println!("unregister_keyboard_hooks executed. Monitoring listener threads severed.");
    prompt_user("Keyboard hooks severed. Verify system transparency.");
    Sral::speak("Verifying systemic transparency after unregistering background keyboard listener thread contexts.", true);
    thread::sleep(Duration::from_secs(3));

    test_section("Core Library Uninitialization Framework Teardown");
    Sral::uninitialize();
    println!("uninitialize function handle called. Releasing references.");
    check(
        !Sral::is_initialized(),
        "is_initialized accurately returns false following uninitialization.",
        "Teardown error tracking validation boundary failure!",
    );

    println!("\nAttempting to call speech synthesis routines post-uninitialization framework context teardown (Should safely evaluate as no-op return false):");
    if Sral::speak(
        "This sentence should be caught by uninitialized guard blocks and drop silently.",
        false,
    ) {
        println!("[WARNING] speak wrapper returned true indicating potential framework state resource retention leaks!");
    } else {
        println!("[INFO] speak wrapper evaluated accurately and returned false inside uninitialized boundary bounds.");
    }

    prompt_user("All Rust integration verification suites executed completely. Press Enter to terminate process context.");
}
