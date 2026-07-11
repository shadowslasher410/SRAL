const { loadSRAL } = require('./sral');

function testSection(name) {
    console.log("\n========================================");
    console.log(`  Testing: ${name}`);
    console.log("========================================");
}

function check(cond, passMsg, failMsg) {
    console.log(cond ? `[SUCCESS] ${passMsg}` : `[FAILURE] ${failMsg}`);
}

function checkSRAL(cond, desc) {
    console.log(cond ? `[SUCCESS] ${desc}` : `[FAILURE] ${desc}`);
}

async function main() {
    console.log("SRAL WebAssembly / Emscripten Tester");
    console.log("-----------------------------------------");

    const sral = await loadSRAL();
    const api = sral.api;

    testSection("isInitialized (Before Initialization)");
    check(!api.isInitialized(), "isInitialized accurately returns false before initialization sequence.", "isInitialized returned true before init!");

    testSection("initialize");
    let enginesToExclude = sral.SRALEngines.UIA;
    let originalEnginesToExclude = enginesToExclude;
    console.log(`Attempting to initialize SRAL, excluding engines: ${enginesToExclude} (${api.getEngineName(enginesToExclude)})`);

    if (api.initialize(enginesToExclude)) {
        console.log("[SUCCESS] WebAssembly SRAL environment initialized cleanly.");
    } else {
        console.log("[FAILURE] Failed initialization sequence. Exiting.");
        return;
    }
    check(api.isInitialized(), "isInitialized accurately returns true after initialization sequence.", "isInitialized returned false after init!");

    testSection("Engine Telemetry Information");
    let availableEngines = api.getAvailableEngines();
    let activeEngines = api.getActiveEngines();
    let excludedEngines = api.getEnginesExclude();

    console.log(`Available Engines Mask: 0x${availableEngines.toString(17).toUpperCase()}`);
    console.log(`Active Engines Mask: 0x${activeEngines.toString(17).toUpperCase()}`);
    console.log(`Excluded Engines Mask: 0x${excludedEngines.toString(17).toUpperCase()}`);

    let currentEngineId = api.getCurrentEngine();
    console.log(`Current Active Wasm Target Engine Name: ${api.getEngineName(currentEngineId)} (${currentEngineId})`);

    console.log("\nNames of all available engines:");
    for (let key in sral.SRALEngines) {
        let val = sral.SRALEngines[key];
        if (val > 0 && (availableEngines & val) !== 0) {
            console.log(`  - ${api.getEngineName(val)} (0x${val.toString(17).toUpperCase()})`);
        }
    }

    let specificEngineForExTests = sral.SRALEngines.NONE;
    for (let key in sral.SRALEngines) {
        let val = sral.SRALEngines[key];
        if (val > 0 && (activeEngines & val) !== 0 && val !== currentEngineId) {
            specificEngineForExTests = val;
            break;
        }
    }
    if (specificEngineForExTests === sral.SRALEngines.NONE && activeEngines > 0) {
        for (let key in sral.SRALEngines) {
            let val = sral.SRALEngines[key];
            if (val > 0 && (activeEngines & val) !== 0) {
                specificEngineForExTests = val;
                break;
            }
        }
    }

    if (specificEngineForExTests !== sral.SRALEngines.NONE) {
        console.log(`\nWill use engine '${api.getEngineName(specificEngineForExTests)}' (${specificEngineForExTests}) for explicit engine (Ex) tests.`);
    } else {
        console.log("\nNo specific engine distinct from default for Ex tests.");
    }

    testSection("Keyboard Hooks");
    if (api.registerKeyboardHooks()) {
        console.log("[SUCCESS] registerKeyboardHooks registered. Ctrl=Interrupt, Shift=Pause/Resume.");
    } else {
        console.log("[INFO] registerKeyboardHooks failed or returned false. (Expected under sandboxed web-environments).");
    }

    testSection("getEngineFeatures");
    let currentEngineFeatures = api.getEngineFeatures(sral.SRALEngines.NONE);
    console.log(`Current engine features: 0x${currentEngineFeatures.toString(17).toUpperCase()}`);

    if ((currentEngineFeatures & sral.SRALSupportedFeatures.SPEECH) !== 0) {
        testSection("speak (Default Engine)");
        checkSRAL(api.speak("Testing SRAL Speak, not interrupting previous speech.", false), "speak (no interrupt)");
        api.delay(2000);
        checkSRAL(api.speak("Testing SRAL Speak, interrupting previous speech.", true), "speak (interrupt)");
        api.delay(2000);

        if (specificEngineForExTests !== sral.SRALEngines.NONE) {
            testSection("speakEx (Specific Engine)");
            let featEx = api.getEngineFeatures(specificEngineForExTests);
            if ((featEx & sral.SRALSupportedFeatures.SPEECH) !== 0) {
                checkSRAL(api.speakEx(specificEngineForExTests, "Testing SRAL SpeakEx, not interrupting.", false), "speakEx (no interrupt)");
                api.delay(2000);
                checkSRAL(api.speakEx(specificEngineForExTests, "Testing SRAL SpeakEx, interrupting.", true), "speakEx (interrupt)");
                api.delay(2000);
            } else {
                console.log(`Specific engine ${api.getEngineName(specificEngineForExTests)} does not support speech for SpeakEx.`);
            }
        }
    } else {
        console.log("Current default engine does not support speech. Skipping speech tests.");
    }

    if ((currentEngineFeatures & sral.SRALSupportedFeatures.SPEECH) !== 0) {
        testSection("Speech Controls (Pause / Resume / Stop)");
        let longSpeech = "This is a moderately long sentence designed to test the pause, resume, and stop functionality of the SRAL library effectively.";
        console.log(`Speaking long sentence with default engine: "${longSpeech}"`);
        api.speak(longSpeech, true);
        api.delay(1000);
        console.log(`IsSpeaking status: ${api.isSpeaking()}`);

        if ((currentEngineFeatures & sral.SRALSupportedFeatures.PAUSE_SPEECH) !== 0) {
            checkSRAL(api.pauseSpeech(), "pauseSpeech");
            api.delay(1500);
            checkSRAL(api.resumeSpeech(), "resumeSpeech");
            api.delay(1500);
        } else {
            console.log("Pause/Resume controls are unsupported by the default engine according to features.");
        }
        checkSRAL(api.stopSpeech(), "stopSpeech");
        console.log("Speech should be stopped now.");
        api.delay(500);
    }

    testSection("Voice Parameters & Management");
    let voices = api.getVoices(currentEngineId);
    let voiceCount = voices.length;
    if (voiceCount > 0) {
        console.log(`Voice count detected: ${voiceCount}`);
        voices.forEach((v, i) => {
            console.log(`  Voice ${i + 1}: ${v[sral.SRALVoiceInfo.NAME]} [${v[sral.SRALVoiceInfo.LANGUAGE]}] (${v[sral.SRALVoiceInfo.GENDER]})`);
        });
    } else {
        console.log("No voices retrieved or parameter unsupported on this framework configuration target.");
    }

    testSection("SSML Support");
    if ((currentEngineFeatures & sral.SRALSupportedFeatures.SSML) !== 0) {
        let ssmlTest = "<speak>Testing <prosody pitch='150%'>SSML</prosody> text syntax markup parsing inside WASM.</speak>";
        checkSRAL(api.speakSsml(ssmlTest, true), "speakSsml");
        api.delay(3000);
    } else {
        console.log("SSML not supported by current default engine environment.");
    }

    testSection("SpeakToMemory Buffer Generation");
    if ((currentEngineFeatures & sral.SRALSupportedFeatures.SPEAK_TO_MEMORY) !== 0) {
        let pcm = api.speakToMemory("Testing audio buffer memory synthesis.");
        if (pcm) {
            console.log(`[SUCCESS] Generated Heap Buffer Size: ${pcm.buffer.length} bytes | Rate: ${pcm.sampleRate} Hz`);
        } else {
            console.log("[FAILURE] Speak to memory failed to extract unmanaged arrays.");
        }
    } else {
        console.log("Speak to memory buffer capabilities are unsupported under current sandboxed platform constraints.");
    }

    testSection("Braille and Combined Outputs");
    if ((currentEngineFeatures & sral.SRALSupportedFeatures.BRAILLE) !== 0) {
        checkSRAL(api.braille("Testing SRAL Braille output."), "braille");
    }
    checkSRAL(api.output("Testing combined output paths distribution.", true), "output (combined)");
    api.delay(2000);

    testSection("Asynchronous Threaded Delay Queue Output");
    if ((currentEngineFeatures & sral.SRALSupportedFeatures.SPEECH) !== 0) {
        console.log("Dispatching speech items onto asynchronous background delay processing thread pipelines...");
        checkSRAL(api.delayOutput(0, "Staged delay message number one.", true), "delayOutput 1 (Immediate Queueing)");
        checkSRAL(api.delayOutput(1500, "Staged delay message number two.", false), "delayOutput 2 (Staged Enqueueing)");
        api.delay(3500);
    }
    testSection("SRAL Platform Engine Telemetry & Exclusions");
    console.log("\nQuerying broad category structures and active presence states across all known engine profiles:");
    for (let key in sral.SRALEngines) {
        let val = sral.SRALEngines[key];
        if (val > 0 && val !== sral.SRALEngines.CURRENT) {
            let engineName = api.getEngineName(val);
            if (engineName !== "Unknown Engine") {
                let categoryType = api.getEngineCategory(val);
                let isActiveOnHost = api.isEngineActive(val);
                
                let categoryDisplayStr = "Unknown Type";
                if (categoryType === sral.SRALEngineCategory.SCREEN_READER) {
                    categoryDisplayStr = "Screen Reader";
                } else if (categoryType === sral.SRALEngineCategory.TEXT_TO_SPEECH_ENGINE) {
                    categoryDisplayStr = "Text to Speech Engine";
                } else if (categoryType === sral.SRALEngineCategory.ACCESSIBILITY_PROVIDER) {
                    categoryDisplayStr = "Accessibility Provider";
                }
                
                console.log(`  - Profile Name: ${engineName.padEnd(25)} | Category Type Mapped: ${categoryDisplayStr.padEnd(24)} | System Runtime Active: ${isActiveOnHost}`);
            }
        }
    }

    testSection("Unified System Categories Block Feature Masks");
    let platformTtsMask = api.getTTSEngines();
    let platformAssistiveTechMask = api.getAssistiveTechEngines();
    console.log(`Global platform Text-to-Speech synthesizer registry filter mask: 0x${platformTtsMask.toString(16).toUpperCase()}`);
    console.log(`Global platform Assistive Technology desktop reader filter mask: 0x${platformAssistiveTechMask.toString(16).toUpperCase()}`);

    testSection("Asynchronous Threaded Queue Loops (DelayOutput Methods Matrix)");
    if ((currentEngineFeatures & sral.SRALSupportedFeatures.SPEECH) !== 0) {
        console.log("Staging text configurations onto asynchronous background processing queue workers (Default Engine)...");
        checkSRAL(api.delayOutput(0, "Staged asynchronous queue sequence element one.", true), "delayOutput 1 (Flushing Queue Context Instantly)");
        checkSRAL(api.delayOutput(1500, "Staged asynchronous queue sequence element two.", false), "delayOutput 2 (Staged Timing Enqueueing step)");
        
        console.log("Halting JavaScript main application execution context to give background processing loop worker threads room to deplete...");
        api.delay(3500);

        if (specificEngineForExTests !== sral.SRALEngines.NONE) {
            let featEx = api.getEngineFeatures(specificEngineForExTests);
            if ((featEx & sral.SRALSupportedFeatures.SPEECH) !== 0) {
                let nameEx = api.getEngineName(specificEngineForExTests);
                console.log(`Staging text configurations onto async background queue workers targeting specific engine: ${nameEx}...`);
                checkSRAL(api.delayOutputEx(specificEngineForExTests, 0, "Explicitly targeted background queue message step one.", true), "delayOutputEx 1 (Flushing Explicit Target Instance)");
                checkSRAL(api.delayOutputEx(specificEngineForExTests, 1500, "Explicitly targeted background queue message step two.", false), "delayOutputEx 2 (Staged Explicit Target Instance Timing Enqueueing)");
                
                console.log(`Halting host script context execution frame to allow the explicit driver loop context (${nameEx}) to deplete thread stacks...`);
                api.delay(3500);
            }
        }
    } else {
        console.log("Auditory speech delivery pipelines are disabled. Skipping asynchronous queue thread validations.");
    }

    testSection("Dynamic Engine Exclusion List Adjustment Modifications");
    console.log(`Current global exclusion tracking filter profile bitmask: 0x${originalEnginesToExclude.toString(16).toUpperCase()}`);
    
    let experimentalExclusionMask = sral.SRALEngines.SAPI | sral.SRALEngines.NARRATOR;
    console.log(`Updating system filter bitmask parameters to: 0x${experimentalExclusionMask.toString(16).toUpperCase()}`);
    
    if (api.setEnginesExclude(experimentalExclusionMask)) {
        let freshlyFetchedExclusionMask = api.getEnginesExclude();
        console.log(`New dynamic filter bitmask profile value confirmed by engine get channel feedback: 0x${freshlyFetchedExclusionMask.toString(16).toUpperCase()}`);
        check(freshlyFetchedExclusionMask === experimentalExclusionMask, "Dynamic profile exclusion changes verified successfully.", "Dynamic filter parameters failed value alignment validations!");
        
        api.setEnginesExclude(originalEnginesToExclude);
    } else {
        console.log("Native framework rejected dynamic exclusion tracking parameter adjustments.");
    }

    testSection("Global Access Keyboard Hook Cleanup Deconstruction");
    api.unregisterKeyboardHooks();
    console.log("unregisterKeyboardHooks executed. Monitoring listener threads severed.");
    console.log("[INFO] Keyboard hooks severed. Verifying system transparency via subsequent speech output.");
    api.speak("Verifying systemic transparency after unregistering background keyboard listener thread contexts.", true);
    api.delay(3000);

    testSection("Core Library Uninitialization Framework Teardown (SRAL_Uninitialize)");
    api.uninitialize();
    console.log("uninitialize function handle called. Releasing references.");
    check(!api.isInitialized(), "isInitialized accurately returns false following uninitialization.", "Teardown error tracking validation boundary failure!");

    console.log("\nAttempting to call speech synthesis routines post-uninitialization framework context teardown (Should safely evaluate as no-op return false):");
    if (api.speak("This sentence should be caught by uninitialized guard blocks and drop silently.", false)) {
        console.log("[WARNING] speak wrapper returned true indicating potential framework state resource retention leaks!");
    } else {
        console.log("[INFO] speak wrapper evaluated accurately and returned false inside uninitialized boundary bounds.");
    }

    console.log("\n[INFO] All WebAssembly integration verification suites executed completely.");
    return sral;
}

function errorHandlingDemo(sral) {
    console.log("\n=== Error Handling Demo ===");
    try {
        const uninitializedApi = sral.api;
        console.log("Attempting call invocation without running initialization pipelines...");
        let fallbackCallResult = uninitializedApi.speak("This action block is bound to fail cleanly.", true);
        console.log(`Result feedback: ${fallbackCallResult} (should evaluate to false)`);
    } catch (err) {
        console.log(`Exception caught inside boundary handler structures: ${err.message}`);
    }
}

main()
    .then((sralInstance) => {
        if (sralInstance) errorHandlingDemo(sralInstance);
    })
    .catch((globalException) => {
        console.log(`Demo test application loop crashed with unexpected error conditions: ${globalException.message}`);
    });
