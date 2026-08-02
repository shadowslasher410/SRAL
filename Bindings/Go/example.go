package main

import (
	"bufio"
	"fmt"
	"os"
	"runtime"
	"strings"
	"time"
	"unsafe"
)

func testSection(name string) {
	fmt.Printf("\n\n========================================\n")
	fmt.Printf("  Testing: %s\n", name)
	fmt.Printf("========================================\n")
}

func check(condition bool, successMsg, failMsg string) {
	if condition {
		fmt.Printf("[SUCCESS] %s\n", successMsg)
	} else {
		fmt.Printf("[FAILURE] %s\n", failMsg)
	}
}

func checkSRAL(condition bool, actionDesc string) {
	if condition {
		fmt.Printf("[SUCCESS] %s\n", actionDesc)
	} else {
		fmt.Printf("[FAILURE] %s\n", actionDesc)
	}
}

func promptUser(message string) {
	fmt.Printf("\n>>> %s (Press Enter to continue)...", message)
	reader := bufio.NewReader(os.Stdin)
	_, _ = reader.ReadString('\n')
}
func printSupportedFeatures(features uint32) {
	fmt.Printf("Supported Features (%d):\n", features)
	if features == 0 {
		fmt.Printf("  (None)\n")
		return
	}
	if (features & uint32(FeatureSpeech)) != 0 {
		fmt.Printf("  - SUPPORTS_SPEECH\n")
	}
	if (features & uint32(FeatureBraille)) != 0 {
		fmt.Printf("  - SUPPORTS_BRAILLE\n")
	}
	if (features & uint32(FeatureSpeechRate)) != 0 {
		fmt.Printf("  - SUPPORTS_SPEECH_RATE\n")
	}
	if (features & uint32(FeatureSpeechVolume)) != 0 {
		fmt.Printf("  - SUPPORTS_SPEECH_VOLUME\n")
	}
	if (features & uint32(FeatureSelectVoice)) != 0 {
		fmt.Printf("  - SUPPORTS_SELECT_VOICE\n")
	}
	if (features & uint32(FeaturePauseSpeech)) != 0 {
		fmt.Printf("  - SUPPORTS_PAUSE_SPEECH\n")
	}
	if (features & uint32(FeatureSsml)) != 0 {
		fmt.Printf("  - SUPPORTS_SSML\n")
	}
	if (features & uint32(FeatureSpeakToMemory)) != 0 {
		fmt.Printf("  - SUPPORTS_SPEAK_TO_MEMORY\n")
	}
	if (features & uint32(FeatureSpelling)) != 0 {
		fmt.Printf("  - SUPPORTS_SPELLING\n")
	}
}

// printEngineNames iterates over the engine bitmask and resolves string descriptions.
func printEngineNames(engineBitmask SralEngines, title string) {
	fmt.Printf("%s:\n", title)
	if engineBitmask == EngineNone {
		fmt.Printf("  (None)\n\n")
		return
	}
	found := false
	for i := 1; i <= 15; i++ {
		flag := SralEngines(1 << i)
		if (engineBitmask & flag) != 0 {
			name := GetEngineName(flag)
			if name == "" {
				name = "Unnamed Core Engine Instance"
			}
			fmt.Printf("  - %s (0x%X)\n", name, uint32(flag))
			found = true
		}
	}
	if !found {
		fmt.Printf("  (Unknown bitmask representation: %d)\n", uint32(engineBitmask))
	}
	fmt.Printf("\n")
}
func main() {
	// Lock the OS thread context explicitly for reliable UI and hook tracking layers
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()

	fmt.Printf("SRAL Go Cross-Platform Tester\n-------------------------\n")

	testSection("SRAL.IsInitialized (Before Initialization)")
	check(!IsInitialized(), "IsInitialized correctly returns false before init.", "IsInitialized returned true before init!")

	testSection("SRAL.Initialize")
	enginesToExclude := EngineUia
	fmt.Printf("Attempting to initialize SRAL, excluding engines flag: %d (%s)\n", uint32(enginesToExclude), GetEngineName(enginesToExclude))

	if Initialize(enginesToExclude) {
		fmt.Printf("[SUCCESS] SRAL_Initialize successful.\n")
	} else {
		fmt.Printf("[FAILURE] SRAL_Initialize failed. Exiting.\n")
		return
	}
	check(IsInitialized(), "IsInitialized correctly returns true after init.", "IsInitialized returned false after init!")
}
func runEngineAndSpeechTests(activeEngines SralEngines, currentEngineID SralEngines, currentEngineFeatures uint32) {
	testSection("Engine Telemetry Information")
	printEngineNames(GetAvailableEngines(), "Available Engines on this Platform")
	printEngineNames(activeEngines, "Currently Active/Usable Engines")
	fmt.Printf("Current Default Engine: %s (%d)\n", GetEngineName(currentEngineID), currentEngineID)

	specificEngineForExTests := EngineNone
	for i := 1; i <= 15; i++ {
		flag := SralEngines(1 << i)
		if (activeEngines&flag) != 0 && flag != currentEngineID {
			specificEngineForExTests = flag
			break
		}
	}
	if specificEngineForExTests == EngineNone && activeEngines != EngineNone {
		for i := 1; i <= 15; i++ {
			flag := SralEngines(1 << i)
			if (activeEngines & flag) != 0 {
				specificEngineForExTests = flag
				break
			}
		}
	}

	if specificEngineForExTests != EngineNone {
		fmt.Printf("\nWill use engine '%s' (%d) for specific engine (Ex) tests.\n", GetEngineName(specificEngineForExTests), specificEngineForExTests)
	} else {
		fmt.Printf("\nNo distinct specific engine active for explicit Ex tests.\n")
	}

	testSection("Keyboard Hooks Activation Layer")
	if RegisterKeyboardHooks() {
		fmt.Printf("[SUCCESS] global keyboard hooks registered.\n")
		promptUser("Keyboard hooks (Ctrl=Interrupt, Shift=Pause) active. Test them during speech checks.")
	} else {
		fmt.Printf("[INFO] registerKeyboardHooks failed or unsupported on this target platform environment.\n")
	}

	testSection("SRAL.GetEngineFeatures")
	fmt.Printf("Features for Current Default Engine (%s):\n", GetEngineName(currentEngineID))
	printSupportedFeatures(currentEngineFeatures)

	if (currentEngineFeatures & uint32(FeatureSpeech)) != 0 {
		testSection("SRAL.Speak (Default Engine)")
		checkSRAL(Speak("Testing SRAL Speak from Go, not interrupting previous speech.", false), "SRAL.Speak (no interrupt)")
		time.Sleep(2 * time.Second)
		checkSRAL(Speak("Testing SRAL Speak from Go, interrupting previous speech now.", true), "SRAL.Speak (interrupt)")
		time.Sleep(2 * time.Second)

		if specificEngineForExTests != EngineNone {
			testSection("SRAL.SpeakEx (Specific Engine)")
			featEx := GetEngineFeatures(specificEngineForExTests)
			if (featEx & uint32(FeatureSpeech)) != 0 {
				checkSRAL(SpeakEx(specificEngineForExTests, "Testing explicit sub-driver routing configurations using SpeakEx.", false), "SRAL.SpeakEx (no interrupt)")
				time.Sleep(2 * time.Second)
			}
		}
	}
}
func runFlowAndVoiceTests(currentEngineID SralEngines, currentEngineFeatures uint32, _ SralEngines) {
	if (currentEngineFeatures & uint32(FeatureSpeech)) != 0 {
		testSection("Speech Flow Control (Pause / Resume / Stop)")
		longSpeech := "This is a moderately long sentence configuration designed to validate the runtime pausing, resuming, and stopping workflows natively."
		fmt.Printf("Speaking sequence: \"%s\"\n", longSpeech)
		Speak(longSpeech, true)
		time.Sleep(1 * time.Second)
		fmt.Printf("IsSpeaking checking loop status flag: %t\n", IsSpeaking())

		if (currentEngineFeatures & uint32(FeaturePauseSpeech)) != 0 {
			PauseSpeech()
			promptUser("Speech engine paused. Verify silence.")
			ResumeSpeech()
			time.Sleep(1500 * time.Millisecond)
		}
		StopSpeech()
		fmt.Printf("IsSpeaking status post stop instruction: %t\n", IsSpeaking())
	}

	testSection("Dynamic Voice Queries & Allocation")
	voices := GetEngineVoiceList(currentEngineID)
	voiceCount := len(voices)
	if voiceCount > 0 {
		fmt.Printf("Voices found count metric: %d\n", voiceCount)
		for _, v := range voices {
			fmt.Printf("  - Voice [%d]: Name='%s' Language='%s' Gender='%s' (Vendor: %s)\n", v.Index, v.Name, v.Language, v.Gender, v.Vendor)
		}

		if voiceCount > 1 && (currentEngineFeatures&uint32(FeatureSelectVoice)) != 0 {
			fmt.Printf("Switching channel environment context to alternative system voice index 1...\n")
			var targetIdx int32 = 1
			if SetEngineParameter(currentEngineID, ParamVoiceIndex, unsafe.Pointer(&targetIdx)) {
				Speak("This text evaluates speech using an alternative systemic voice profile configuration.", true)
				time.Sleep(2500 * time.Millisecond)
				var resetIdx int32 = 0
				SetEngineParameter(currentEngineID, ParamVoiceIndex, unsafe.Pointer(&resetIdx))
			}
		}
	}

	testSection("SSML Parsing Verification")
	if (currentEngineFeatures & uint32(FeatureSsml)) != 0 {
		ssmlText := "<speak>Testing <break time=\"400ms\"/> structural SSML markup tags.</speak>"
		checkSRAL(SpeakSsml(ssmlText, true), "SRAL.SpeakSsml")
		time.Sleep(3 * time.Second)
	}
}
func runMemoryAndTeardownTests(currentEngineFeatures uint32, specificEngineForExTests SralEngines) {
	testSection("Raw Audio Memory Generation Framework (SpeakToMemory)")
	if (currentEngineFeatures & uint32(FeatureSpeakToMemory)) != 0 {
		fmt.Printf("Synthesizing raw string layouts into local application typed array structures...\n")
		pcm := SpeakToMemory("Audio serialization buffer check.")
		if !pcm.IsEmpty() {
			fmt.Printf("[SUCCESS] Managed data block generated successfully.\n")
			fmt.Printf("  Buffer Array Dimensions: %d bytes extracted.\n", len(pcm.Data))
			fmt.Printf("  Format Specs: Channels=%d | Rate=%dHz | Depth=%d-bit depth layer.\n", pcm.Channels, pcm.SampleRate, pcm.BitsPerSample)
			pcm.Free()
		} else {
			fmt.Printf("[FAILURE] SpeakToMemory execution sequence faulted.\n")
		}
	}

	testSection("Tactile Braille Refresh & Combined Output Targets")
	if (currentEngineFeatures & uint32(FeatureBraille)) != 0 {
		checkSRAL(Braille("GO BINDINGS"), "SRAL.Braille pin layout updates")
	}
	checkSRAL(Output("Unified distribution test tracking endpoint paths.", true), "SRAL.Output combined pipeline paths")
	time.Sleep(2 * time.Second)

	testSection("Asynchronous Threaded Queue Loops (DelayOutput Methods)")
	if (currentEngineFeatures & uint32(FeatureSpeech)) != 0 {
		fmt.Printf("Dispatching speech items onto asynchronous background delay processing thread pipelines (Default Engine)...\n")
		checkSRAL(DelayOutput(0, "Staged delay message number one.", true), "DelayOutput 1 (Flushing Queue Context Instantly)")
		checkSRAL(DelayOutput(1500, "Staged delay message number two.", false), "DelayOutput 2 (Staged Timing Enqueueing step)")
		fmt.Printf("Halting Go application thread loop execution context to give background processing loop worker threads room to deplete...\n")
		time.Sleep(3500 * time.Millisecond)

		if specificEngineForExTests != EngineNone {
			featEx := GetEngineFeatures(specificEngineForExTests)
			if (featEx & uint32(FeatureSpeech)) != 0 {
				nameEx := GetEngineName(specificEngineForExTests)
				fmt.Printf("Staging text configurations onto async background queue workers targeting specific engine: %s...\n", nameEx)
				checkSRAL(DelayOutputEx(specificEngineForExTests, 0, "Explicitly targeted background queue message step one.", true), "DelayOutputEx 1 (Flushing Explicit Target Instance)")
				checkSRAL(DelayOutputEx(specificEngineForExTests, 1500, "Explicitly targeted background queue message step two.", false), "DelayOutputEx 2 (Staged Explicit Target Instance Timing Enqueueing)")
				fmt.Printf("Halting Go thread frame to allow the explicit driver loop context (%s) to deplete thread stacks...\n", nameEx)
				time.Sleep(3500 * time.Millisecond)
			}
		}
	}

	testSection("Dynamic Engine Exclusion List Adjustment Modifications")
	originalEnginesToExclude, hasExclusions := GetEnginesExclude()
	actualExcludeMask := EngineNone
	if hasExclusions {
		actualExcludeMask = SralEngines(originalEnginesToExclude)
	}
	fmt.Printf("Current global exclusion tracking filter profile bitmask: 0x%s\n", strings.ToUpper(fmt.Sprintf("%x", uint32(actualExcludeMask))))

	experimentalExclusionMask := EngineSapi | EngineNarrator
	fmt.Printf("Updating system filter bitmask parameters to: 0x%s\n", strings.ToUpper(fmt.Sprintf("%x", uint32(experimentalExclusionMask))))

	if SetEnginesExclude(experimentalExclusionMask) {
		freshlyFetchedExclusionMask, _ := GetEnginesExclude()
		fmt.Printf("New dynamic filter bitmask profile value confirmed by engine get channel feedback: 0x%s\n", strings.ToUpper(fmt.Sprintf("%x", freshlyFetchedExclusionMask)))
		check(freshlyFetchedExclusionMask == uint32(experimentalExclusionMask), "Dynamic profile exclusion changes verified successfully.", "Dynamic filter parameters failed value alignment validations!")
		SetEnginesExclude(actualExcludeMask)
	}

	testSection("Global Access Keyboard Hook Cleanup Deconstruction")
	UnregisterKeyboardHooks()
	fmt.Printf("UnregisterKeyboardHooks executed. Monitoring listener threads severed.\n")
	promptUser("Keyboard hooks severed. Verify system transparency by typing Ctrl/Shift inputs with upcoming speech outputs.")
	Speak("Verifying systemic transparency after unregistering background keyboard listener thread contexts.", true)
	time.Sleep(3 * time.Second)

	testSection("Core Library Uninitialization Framework Teardown")
	Uninitialize()
	check(!IsInitialized(), "IsInitialized accurately returns false following uninitialization.", "Teardown error tracking validation boundary failure!")

	fmt.Printf("\nAttempting to call speech synthesis routines post-uninitialization framework context teardown (Should safely evaluate as no-op return false):\n")
	if Speak("This sentence should be caught by uninitialized guard blocks and drop silently.", false) {
		fmt.Printf("[WARNING] speak wrapper returned true indicating potential framework state resource retention leaks!\n")
	} else {
		fmt.Printf("[INFO] speak wrapper evaluated accurately and returned false inside uninitialized boundary bounds.\n")
	}
	promptUser("All Go integration verification suites executed completely. Press Enter to terminate process context.")
}
