const {
	SRAL,
	SRALEngine,
	SRALEngineCategory,
	SRALFeature,
	SRALParam,
	SRALVoiceInfo
} = require('./lib/sral');

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

function flagStatus(fl) { return fl ? "Enabled" : "Disabled"; }

function main() {
	const sral = new SRAL();
	console.log("SRAL Node.js Comprehensive Tester\n-------------------------");

	testSection("SRAL_IsInitialized (Before Initialization)");
	check(!sral.isInitialized(), "isInitialized accurately returns false before init.", "isInitialized returned true before init!");

	testSection("SRAL_Initialize");
	let originalEnginesToExclude = SRALEngine.UIA;
	console.log(`Attempting to initialize SRAL, excluding engines mask: 0x${originalEnginesToExclude.toString(16).toUpperCase()}`);

	if (sral.initialize(originalEnginesToExclude)) {
		console.log("[SUCCESS] initialize successful.");
	} else {
		console.log("[FAILURE] initialize failed. Exiting framework testing process.");
		return;
	}
	check(sral.isInitialized(), "isInitialized cleanly returns true after init.", "isInitialized returned false after init!");

	testSection("Engine Information & Mask Telemetry");
	let availableEngines = sral.getAvailableEngines();
	console.log(`Available Engines Mask: 0x${availableEngines.toString(16).toUpperCase()}`);
	let activeEngines = sral.getActiveEngines();
	console.log(`Active Engines Mask: 0x${activeEngines.toString(16).toUpperCase()}`);
	let originalExcludeMask = sral.getEnginesExclude();
	console.log(`Current Mask Exclusions: 0x${originalExcludeMask.toString(16).toUpperCase()}`);

	let currentEngineId = sral.getCurrentEngine();
	console.log(`Current Active Default Engine ID: ${currentEngineId} (${sral.getEngineName(currentEngineId)})`);

	console.log("\nNames of all available SRALEngine enum members:");
	for (let key in SRALEngine) {
		let val = SRALEngine[key];
		if (val > 0 && val !== SRALEngine.CURRENT && (availableEngines & val) !== 0) {
			console.log(`  - Engine ID 0x${val.toString(16).toUpperCase()}: ${sral.getEngineName(val)}`);
		}
	}

	let specificEngineForExTests = SRALEngine.NONE;
	for (let key in SRALEngine) {
		let val = SRALEngine[key];
		if (val > 0 && val !== SRALEngine.CURRENT && (activeEngines & val) !== 0 && val !== currentEngineId) {
			specificEngineForExTests = val;
			break;
		}
	}
	if (specificEngineForExTests === SRALEngine.NONE && activeEngines > 0) {
		for (let key in SRALEngine) {
			let val = SRALEngine[key];
			if (val > 0 && val !== SRALEngine.CURRENT && (activeEngines & val) !== 0) {
				specificEngineForExTests = val;
				break;
			}
		}
	}

	if (specificEngineForExTests !== SRALEngine.NONE) {
		console.log(`\nSelected Engine for Explicit (Ex) Subsystem routing: ${sral.getEngineName(specificEngineForExTests)} (${specificEngineForExTests})`);
	} else {
		console.log("\nNo alternative explicit engines found active for distinct Ex verification execution paths.");
	}

	testSection("Global Intercept Keyboard Hooks");
	if (sral.registerKeyboardHooks()) {
		console.log("[SUCCESS] registerKeyboardHooks successfully engaged. Ctrl=Interrupt, Shift=Pause/Resume.");
	} else {
		console.log("[INFO] registerKeyboardHooks did not bind. (Expected behavior across exclusive screen readers or mobile layers).");
	}

	testSection("SRAL_GetEngineFeatures Topology Checks");
	let currentEngineFeatures = sral.getEngineFeatures(SRALEngine.NONE);
	console.log(`Features for Current Default Engine (${sral.getEngineName(currentEngineId)}): 0x${currentEngineFeatures.toString(16).toUpperCase()}`);

	if ((currentEngineFeatures & SRALFeature.SPEECH) !== 0) console.log("  - Auditory Speech Support: YES");
	if ((currentEngineFeatures & SRALFeature.BRAILLE) !== 0) console.log("  - Tactile Braille Refresh: YES");
	if ((currentEngineFeatures & SRALFeature.SSML) !== 0) console.log("  - Contextual SSML Tokenizing: YES");
	if ((currentEngineFeatures & SRALFeature.SPEAK_TO_MEMORY) !== 0) console.log("  - PCM Stream Speech Synthesis to Memory: YES");

	if ((currentEngineFeatures & SRALFeature.SPEECH) !== 0) {
		testSection("SRAL_Speak & SRAL_SpeakEx Output Verification");
		checkSRAL(sral.speak("Testing SRAL Speak, not interrupting previous operations.", false), "speak (no interrupt)");
		sral.delay(2000);
		checkSRAL(sral.speak("Testing SRAL Speak, executing forced interrupt sequence.", true), "speak (interrupt)");
		sral.delay(2000);

		if (specificEngineForExTests !== SRALEngine.NONE) {
			let featEx = sral.getEngineFeatures(specificEngineForExTests);
			if ((featEx & SRALFeature.SPEECH) !== 0) {
				checkSRAL(sral.speakEx(specificEngineForExTests, "Testing direct specific engine speak ex output, non-interrupting.", false), "speakEx (no interrupt)");
				sral.delay(2000);
			}
		}
	}

	testSection("Speech Flow Controls Lifecycle (Default vs Extended)");
	if ((currentEngineFeatures & SRALFeature.SPEECH) !== 0) {
		let longSpeech = "This is a moderately long sentence configuration designed to validate pausing, resuming, and termination routines within the abstract interface layer.";
		console.log(`Spawning speech sequence stream: "${longSpeech}"`);
		sral.speak(longSpeech, true);
		sral.delay(1000);
		console.log(`Current real-time speaking condition: ${sral.isSpeaking()}`);

		if ((currentEngineFeatures & SRALFeature.PAUSE_SPEECH) !== 0) {
			checkSRAL(sral.pauseSpeech(), "pauseSpeech execution");
			console.log(`IsSpeaking execution condition inside pause boundary: ${sral.isSpeaking()}`);
			sral.delay(1500);
			checkSRAL(sral.resumeSpeech(), "resumeSpeech execution");
			sral.delay(1500);
		}

		checkSRAL(sral.stopSpeech(), "stopSpeech flush");
		console.log(`Final speech condition processed post stop command: ${sral.isSpeaking()}`);
		sral.delay(500);

		if (specificEngineForExTests !== SRALEngine.NONE) {
			let featEx = sral.getEngineFeatures(specificEngineForExTests);
			if ((featEx & SRALFeature.SPEECH) !== 0) {
				sral.speakEx(specificEngineForExTests, longSpeech, true);
				sral.delay(1000);
				if ((featEx & SRALFeature.PAUSE_SPEECH) !== 0) {
					checkSRAL(sral.pauseSpeechEx(specificEngineForExTests), "pauseSpeechEx execution");
					sral.delay(1000);
					checkSRAL(sral.resumeSpeechEx(specificEngineForExTests), "resumeSpeechEx execution");
					sral.delay(1000);
				}
				checkSRAL(sral.stopSpeechEx(specificEngineForExTests), "stopSpeechEx flush");
				console.log(`IsSpeakingEx boundary post direct termination: ${sral.isSpeakingEx(specificEngineForExTests)}`);
				sral.delay(500);
			}
		}
	}

	testSection("Generic SetIntParameter / GetIntParameter Execution Channels");
	let originalSymbolLevel = sral.getIntParameter(currentEngineId, SRALParam.SYMBOL_LEVEL);
	if (originalSymbolLevel !== -1) {
		console.log(`Current engine baseline punctuation symbol level setting: ${originalSymbolLevel}`);
		let targetSymbolLevel = SRALSymbolLevel.ALL;

		console.log(`Attempting to adjust symbol level channel value to: ${targetSymbolLevel} (ALL)`);
		if (sral.setIntParameter(currentEngineId, SRALParam.SYMBOL_LEVEL, targetSymbolLevel)) {
			let fetchedSymbolLevel = sral.getIntParameter(currentEngineId, SRALParam.SYMBOL_LEVEL);
			console.log(`New symbol level confirmed by get channel wrapper: ${fetchedSymbolLevel}`);
			check(fetchedSymbolLevel === targetSymbolLevel, "Symbol level set and get channels match perfectly.", "Symbol level channel validation mismatch!");

			sral.speak("Testing symbol level parameter modification using special characters: at symbol @ hash tag # punctuation check.", true);
			sral.delay(4000);
			sral.setIntParameter(currentEngineId, SRALParam.SYMBOL_LEVEL, originalSymbolLevel);
		} else {
			console.log("Failed to override default symbol level configuration on this specific driver.");
		}
	} else {
		console.log("SYMBOL_LEVEL parameter tuning channel is unsupported by the current default engine framework.");
	}

	testSection("Advanced Character Spelling Modes Tuning");
	let originalSpellingState = sral.getIntParameter(currentEngineId, SRALParam.ENABLE_SPELLING);
	if (originalSpellingState !== -1) {
		console.log(`Current engine spelling configuration baseline flag: ${originalSpellingState}`);
		let targetSpellingState = 1;

		console.log("Enabling character spelling mode parameter layer...");
		if (sral.setIntParameter(currentEngineId, SRALParam.ENABLE_SPELLING, targetSpellingState)) {
			let confirmedSpellingState = sral.getIntParameter(currentEngineId, SRALParam.ENABLE_SPELLING);
			console.log(`New character spelling flag status confirmed: ${confirmedSpellingState}`);

			sral.speak("SRAL", true);
			sral.delay(3000);

			sral.setIntParameter(currentEngineId, SRALParam.ENABLE_SPELLING, originalSpellingState);
			console.log("Character spelling configuration flag restored.");
		}
	} else {
		console.log("ENABLE_SPELLING configuration channel is unsupported by the current active accessibility profile.");
	}

	testSection("Explicit Subsystem Engine Invocations (Ex Methods Matrix)");
	let engineTargets = [SRALEngine.NVDA, SRALEngine.JAWS, SRALEngine.SAPI];
	engineTargets.forEach((engineId) => {
		if ((availableEngines & engineId) !== 0) {
			let name = sral.getEngineName(engineId);
			console.log(`Running targeted performance validations against engine instance: ${name}...`);

			let featEx = sral.getEngineFeatures(engineId);
			if ((featEx & SRALFeature.SPEECH) !== 0) {
				checkSRAL(sral.speakEx(engineId, `Hello from the explicitly targeted ${name} accessibility layer profile interface.`, true), `speakEx routing target for ${name}`);
				sral.delay(2500);

				console.log(`  - ${name} is actively speaking verification query status: ${sral.isSpeakingEx(engineId)}`);

				checkSRAL(sral.stopSpeechEx(engineId), `stopSpeechEx instance execution command for ${name}`);
				sral.delay(500);
			}

			if ((featEx & SRALFeature.SSML) !== 0) {
				let explicitSsmlText = "<speak>Testing explicit subsystem engine routing <break time=\"400ms\"/> text.</speak>";
				checkSRAL(sral.speakSsmlEx(engineId, explicitSsmlText, true), `speakSsmlEx instance routing target for ${name}`);
				sral.delay(3000);
			}

			if ((featEx & SRALFeature.BRAILLE) !== 0) {
				checkSRAL(sral.brailleEx(engineId, "EX MODE ACTIVE"), `brailleEx instance layout pinning for ${name}`);
			}

			checkSRAL(sral.outputEx(engineId, `Combined distribution routing target output payload for explicit instance context: ${name}`, true), `outputEx routine handle for ${name}`);
			sral.delay(2000);
		}
	});

	testSection("Raw PCM Audio Processing Pipelines (SpeakToMemory / Ex)");
	if ((currentEngineFeatures & SRALFeature.SPEAK_TO_MEMORY) !== 0) {
		console.log("Synthesizing raw string values into unmanaged memory blocks (Default Engine)...");
		let pcmData = sral.speakToMemory("This is a comprehensive serialization check of raw speech conversion buffers.");

		if (pcmData && pcmData.buffer) {
			console.log("[SUCCESS] speakToMemory successfully returned unmanaged audio array parameters.");
			console.log(`  - Buffer Frame Volume: ${pcmData.buffer.length} bytes serialized.`);
			console.log(`  - Format Layout Met: Channels=${pcmData.channels} | Rate=${pcmPdata.sampleRate} Hz | Depth=${pcmData.bitsPerSample}-bit depth layout.`);
		} else {
			console.log("[FAILURE] speakToMemory execution returned empty boundary handles.");
		}

		if (specificEngineForExTests !== SRALEngine.NONE) {
			let featEx = sral.getEngineFeatures(specificEngineForExTests);
			if ((featEx & SRALFeature.SPEAK_TO_MEMORY) !== 0) {
				let nameEx = sral.getEngineName(specificEngineForExTests);
				console.log(`Synthesizing raw string values into unmanaged memory blocks explicitly targeting: ${nameEx}...`);

				let pcmDataEx = sral.speakToMemoryEx(specificEngineForExTests, "Targeted instance memory synthesis.");
				if (pcmDataEx && pcmDataEx.buffer) {
					console.log(`[SUCCESS] speakToMemoryEx executed successfully for targeted driver node profile: ${nameEx}.`);
					console.log(`  - Buffer Frame Volume: ${pcmDataEx.buffer.length} bytes processed into JavaScript environment buffer containers.`);
				} else {
					console.log(`[FAILURE] speakToMemoryEx execution routine faulted on targeted engine: ${nameEx}`);
				}
			}
		}
	} else {
		console.log("SPEAK_TO_MEMORY buffer synthesis capabilities are unsupported by the current active driver layer.");
	}

	testSection("Unified System Categories and Active Diagnostics Loops");
	let failedEnginesBitmask = sral.getFailedEngines();
	if (failedEnginesBitmask !== SRALEngine.NONE) {
		console.log(`Bitmask warning array of platform modules failing constructor setup pipelines: 0x${failedEnginesBitmask.toString(16).toUpperCase()}`);
	} else {
		console.log("All target platform subsystem layout architectures instantiated successfully.");
	}

	console.log("\nQuerying broad category structures and active presence states across all known engine profiles:");
	for (let key in SRALEngine) {
		let val = SRALEngine[key];
		if (val > 0 && val !== SRALEngine.CURRENT) {
			let engineName = sral.getEngineName(val);
			if (engineName !== "Unknown Engine") {
				let categoryType = sral.getEngineCategory(val);
				let isActiveOnHost = sral.isEngineActive(val);

				let categoryDisplayStr = "Unknown Type";
				if (categoryType === SRALEngineCategory.SCREEN_READER) categoryDisplayStr = "Screen Reader";
				else if (categoryType === SRALEngineCategory.TEXT_TO_SPEECH_ENGINE) categoryDisplayStr = "Text to Speech Engine";
				else if (categoryType === SRALEngineCategory.ACCESSIBILITY_PROVIDER) categoryDisplayStr = "Accessibility Provider";

				console.log(`  - Profile Name: ${engineName, -26} | Category Type Mapped: ${categoryDisplayStr, -24} | System Runtime Active: ${isActiveOnHost}`);
			}
		}
	}

	testSection("Unified System Categories Block Feature Masks");
	let platformTtsMask = sral.getTTSEngines();
	let platformAssistiveTechMask = sral.getAssistiveTechEngines();
	console.log(`Global platform Text-to-Speech synthesizer registry filter mask: 0x${platformTtsMask.toString(16).toUpperCase()}`);
	console.log(`Global platform Assistive Technology desktop reader filter mask: 0x${platformAssistiveTechMask.toString(16).toUpperCase()}`);

	testSection("Asynchronous Threaded Queue Loops (DelayOutput Methods Matrix)");
	if ((currentEngineFeatures & SRALFeature.SPEECH) !== 0) {
		console.log("Staging text configurations onto asynchronous background processing queue workers (Default Engine)...");
		checkSRAL(sral.delayOutput("Staged asynchronous queue sequence element one.", 0, true, true, false, false), "delayOutput 1 (Flushing Queue Context Instantly)");
		checkSRAL(sral.delayOutput("Staged asynchronous queue sequence element two.", 1500, false, true, false, false), "delayOutput 2 (Staged Timing Enqueueing step)");

		console.log("Halting JavaScript main application execution context to give background processing loop worker threads room to deplete...");
		sral.delay(3500);

		if (specificEngineForExTests !== SRALEngine.NONE) {
			let featEx = sral.getEngineFeatures(specificEngineForExTests);
			if ((featEx & SRALFeature.SPEECH) !== 0) {
				let nameEx = sral.getEngineName(specificEngineForExTests);
				console.log(`Staging text configurations onto async background queue workers targeting specific engine: ${nameEx}...`);

				checkSRAL(sral.delayOutputEx(specificEngineForExTests, "Explicitly targeted background queue message step one.", 0, true, true, false, false), "delayOutputEx 1 (Flushing Explicit Target Instance)");
				checkSRAL(sral.delayOutputEx(specificEngineForExTests, "Explicitly targeted background queue message step two.", 1500, false, true, false, false), "delayOutputEx 2 (Staged Explicit Target Instance Timing Enqueueing)");

				console.log(`Halting host script context execution frame to allow the explicit driver loop context (${nameEx}) to deplete thread stacks...`);
				sral.delay(3500);
			}
		}
	} else {
		console.log("Auditory speech delivery pipelines are disabled. Skipping asynchronous queue thread validations.");
	}

	testSection("Dynamic Engine Exclusion List Adjustment Modifications");
	console.log(`Current global exclusion tracking filter profile bitmask: 0x${originalEnginesToExclude.toString(16).toUpperCase()}`);

	let experimentalExclusionMask = SRALEngine.SAPI | SRALEngine.NARRATOR;
	console.log(`Updating system filter bitmask parameters to: 0x${experimentalExclusionMask.toString(16).toUpperCase()}`);

	if (sral.setEnginesExclude(experimentalExclusionMask)) {
		let freshlyFetchedExclusionMask = sral.getEnginesExclude();
		console.log(`New dynamic filter bitmask profile value confirmed by engine get channel feedback: 0x${freshlyFetchedExclusionMask.toString(16).toUpperCase()}`);
		check(freshlyFetchedExclusionMask === experimentalExclusionMask, "Dynamic profile exclusion changes verified successfully.", "Dynamic filter parameters failed value alignment validations!");

		sral.setEnginesExclude(originalEnginesToExclude);
	} else {
		console.log("Native framework rejected dynamic exclusion tracking parameter adjustments.");
	}

	testSection("Global Access Keyboard Hook Cleanup Deconstruction");
	sral.unregisterKeyboardHooks();
	console.log("unregisterKeyboardHooks executed. Monitoring listener threads severed.");
	promptUser("Keyboard hooks severed. Verify system transparency by typing Ctrl/Shift inputs with upcoming speech outputs.");
	sral.speak("Verifying systemic transparency after unregistering background keyboard listener thread contexts.", true);
	sral.delay(3000);

	testSection("Core Library Uninitialization Framework Teardown (SRAL_Uninitialize)");
	sral.uninitialize();
	console.log("uninitialize function handle called. Releasing references.");
	check(!sral.isInitialized(), "isInitialized accurately returns false following uninitialization.", "Teardown error tracking validation boundary failure!");

	console.log("\nAttempting to call speech synthesis routines post-uninitialization framework context teardown (Should safely evaluate as no-op return false):");
	if (sral.speak("This sentence should be caught by uninitialized guard blocks and drop silently.", false)) {
		console.log("[WARNING] speak wrapper returned true indicating potential framework state resource retention leaks!");
	} else {
		console.log("[INFO] speak wrapper evaluated accurately and returned false inside uninitialized boundary bounds.");
	}

	promptUser("All Node.js integration verification suites executed completely. Press Enter to terminate process context.");
}

function errorHandlingDemo() {
	console.log("\n=== Error Handling Demo ===");
	try {
		const structuralErrorCollectorInstance = new SRAL();
		console.log("Attempting call invocation without running initialization pipelines...");
		let fallbackCallResult = structuralErrorCollectorInstance.speak("This action block is bound to fail cleanly.", true);
		console.log(`Result feedback: ${fallbackCallResult} (should evaluate to false)`);
	} catch (err) {
		console.log(`Exception caught inside boundary handler structures: ${err.message}`);
	}
}

try {
	main();
} catch (globalException) {
	console.log(`Demo test application loop crashed with unexpected error conditions: ${globalException.message}`);
}

errorHandlingDemo();
