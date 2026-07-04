import 'dart:io';
import 'sral.dart';

void testSection(String name) {
  print("\n\n========================================");
  print("  Testing: $name");
  print("========================================");
}

void check(bool condition, String successMsg, String failMsg) {
  if (condition) {
    print("[SUCCESS] $successMsg");
  } else {
    print("[FAILURE] $failMsg");
  }
}

void checkSRAL(bool condition, String actionDesc) {
  if (condition) {
    print("[SUCCESS] $actionDesc");
  } else {
    print("[FAILURE] $actionDesc");
  }
}

void promptUser(String message) {
  print("\n>>> $message (Press Enter to continue)...");
  stdin.readLineSync();
}

void printSupportedFeatures(int features) {
  print("Supported Features ($features):");
  if (features == SralFeatureFlags.none) {
    print("  (None)");
    return;
  }
  if ((features & SralFeatureFlags.supportsSpeech) != 0) print("  - SUPPORTS_SPEECH");
  if ((features & SralFeatureFlags.supportsBraille) != 0) print("  - SUPPORTS_BRAILLE");
  if ((features & SralFeatureFlags.supportsSpeechRate) != 0) print("  - SUPPORTS_SPEECH_RATE");
  if ((features & SralFeatureFlags.supportsSpeechVolume) != 0) print("  - SUPPORTS_SPEECH_VOLUME");
  if ((features & SralFeatureFlags.supportsSelectVoice) != 0) print("  - SUPPORTS_SELECT_VOICE");
  if ((features & SralFeatureFlags.supportsPauseSpeech) != 0) print("  - SUPPORTS_PAUSE_SPEECH");
  if ((features & SralFeatureFlags.supportsSSML) != 0) print("  - SUPPORTS_SSML");
  if ((features & SralFeatureFlags.supportsSpeakToMemory) != 0) print("  - SUPPORTS_SPEAK_TO_MEMORY");
  if ((features & SralFeatureFlags.supportsSpelling) != 0) print("  - SUPPORTS_SPELLING");
  print("");
}

void printEngineNames(int engineBitmask, String title) {
  print("$title:");
  if (engineBitmask == SralEngineFlags.none) {
    print("  (None)\n");
    return;
  }
  
  final sral = SRAL();
  bool found = false;
  
  for (int i = 1; i <= 14; i++) {
    int flag = 1 << i;
    if ((engineBitmask & flag) != 0) {
      String name = sral.getEngineName(flag);
      print("  - $name (0x${flag.toRadixString(16).toUpperCase()})");
      found = true;
    }
  }
  if (!found) print("  (Unknown bitmask: $engineBitmask)");
  print("");
}

void main() {
  final sral = SRAL();
  print("SRAL Dart/Flutter Tester\n-------------------------");

  testSection("sral.isInitialized (Before Initialization)");
  check(!sral.isInitialized(), "isInitialized correctly returns false before init.", "isInitialized returned true before init!");

  testSection("sral.initialize");
  int enginesToExclude = SralEngineFlags.uia;
  print("Attempting to initialize SRAL, excluding engines flag: $enginesToExclude (${sral.getEngineName(enginesToExclude)})");

  if (sral.initialize(enginesToExclude)) {
    print("[SUCCESS] SRAL_Initialize successful.");
  } else {
    print("[FAILURE] SRAL_Initialize failed. Exiting.");
    return;
  }
  check(sral.isInitialized(), "isInitialized correctly returns true after init.", "isInitialized returned false after init!");

  testSection("Engine Telemetry Information");
  int availableEngines = sral.getAvailableEngines();
  printEngineNames(availableEngines, "Available Engines on this Platform");

  int activeEngines = sral.getActiveEngines();
  printEngineNames(activeEngines, "Currently Active/Usable Engines");

  int currentEngineId = sral.getCurrentEngine();
  print("Current Default Engine: ${sral.getEngineName(currentEngineId)} ($currentEngineId)");

  int specificEngineForExTests = SralEngineFlags.none;
  for (int i = 1; i <= 14; i++) {
    int flag = 1 << i;
    if (((activeEngines & flag) != 0) && flag != currentEngineId) {
      specificEngineForExTests = flag;
      break;
    }
  }
  if (specificEngineForExTests == SralEngineFlags.none && activeEngines != SralEngineFlags.none) {
    for (int i = 1; i <= 14; i++) {
      int flag = 1 << i;
      if ((activeEngines & flag) != 0) {
        specificEngineForExTests = flag;
        break;
      }
    }
  }

  if (specificEngineForExTests != SralEngineFlags.none) {
    print("\nWill use engine '${sral.getEngineName(specificEngineForExTests)}' ($specificEngineForExTests) for specific engine (Ex) tests.");
  } else {
    print("\nNo distinct specific engine active for explicit Ex tests.");
  }

  testSection("Keyboard Hooks Activation Layer");
  if (sral.registerKeyboardHooks()) {
    print("[SUCCESS] global keyboard hooks registered.");
    promptUser("Keyboard hooks (Ctrl=Interrupt, Shift=Pause) active. Test them during speech checks.");
  } else {
    print("[INFO] registerKeyboardHooks failed or unsupported on this specific target driver platform context.");
  }

  testSection("sral.getEngineFeatures");
  print("Features for Current Default Engine (${sral.getEngineName(currentEngineId)}):");
  int currentEngineFeatures = sral.getEngineFeatures(SralEngineFlags.none);
  printSupportedFeatures(currentEngineFeatures);

  if ((currentEngineFeatures & SralFeatureFlags.supportsSpeech) != 0) {
    testSection("sral.speak (Default Engine)");
    checkSRAL(sral.speak("Testing SRAL Speak from Dart, not interrupting previous speech.", false), "sral.speak (no interrupt)");
    sral.delay(2000);
    checkSRAL(sral.speak("Testing SRAL Speak from Dart, interrupting previous speech now.", true), "sral.speak (interrupt)");
    sral.delay(2000);

    if (specificEngineForExTests != SralEngineFlags.none) {
      testSection("sral.speakEx (Specific Engine)");
      int featuresEx = sral.getEngineFeatures(specificEngineForExTests);
      if ((featuresEx & SralFeatureFlags.supportsSpeech) != 0) {
        checkSRAL(sral.speakEx(specificEngineForExTests, "Testing explicit sub-driver routing configurations using SpeakEx.", false), "sral.speakEx (no interrupt)");
        sral.delay(2000);
      }
    }

    testSection("Speech Flow Control (Pause / Resume / Stop)");
    String longSpeech = "This is a moderately long sentence configuration designed to validate the runtime pausing, resuming, and stopping workflows natively.";
    print("Speaking sequence: \"$longSpeech\"");
    sral.speak(longSpeech, true);
    sral.delay(1000);
    print("IsSpeaking checking loop status flag: ${sral.isSpeaking()}");

    if ((currentEngineFeatures & SralFeatureFlags.supportsPauseSpeech) != 0) {
      checkSRAL(sral.pauseSpeech(), "sral.pauseSpeech");
      promptUser("Speech engine paused. Verify silence.");
      checkSRAL(sral.resumeSpeech(), "sral.resumeSpeech");
      sral.delay(1500);
    }
    checkSRAL(sral.stopSpeech(), "sral.stopSpeech");
    print("IsSpeaking status post stop instruction: ${sral.isSpeaking()}");
  }

  testSection("Dynamic Voice Queries & Allocation");
  int voiceCount = sral.getIntParameter(currentEngineId, SralEngineParams.voiceCount);
  if (voiceCount > 0) {
    print("Voices found count metric: $voiceCount");
    List<SralVoiceInfo> voices = sral.getVoices(currentEngineId);
    for (var v in voices) {
      print("  - Voice [${v.index}]: Name='${v.name}' Language='${v.language}' Gender='${v.gender}' (Vendor: ${v.vendor})");
    }

    if (voiceCount > 1 && (currentEngineFeatures & SralFeatureFlags.supportsSelectVoice) != 0) {
      print("Switching channel environment context to alternative system voice index 1...");
      if (sral.setIntParameter(currentEngineId, SralEngineParams.voiceIndex, 1)) {
        sral.speak("This text evaluates speech using an alternative systemic voice profile configuration.", true);
        sral.delay(2500);
        sral.setIntParameter(currentEngineId, SralEngineParams.voiceIndex, 0);
      }
    }
  }

  testSection("SSML Parsing Verification");
  if ((currentEngineFeatures & SralFeatureFlags.supportsSSML) != 0) {
    String ssmlText = "<speak>Testing <break time=\"400ms\"/> structural SSML markup tags.</speak>";
    checkSRAL(sral.speakSsml(ssmlText, true), "sral.speakSsml");
    sral.delay(3000);
  }

  testSection("Raw Audio Memory Generation Framework (speakToMemory)");
  if ((currentEngineFeatures & SralFeatureFlags.supportsSpeakToMemory) != 0) {
    print("Synthesizing raw string layouts into local application typed array structures...");
    final pcm = sral.speakToMemory("Audio serialization buffer check.");
    if (pcm != null) {
      print("[SUCCESS] Managed data block generated successfully.");
      print("  Buffer Array Dimensions: ${pcm.buffer.length} bytes extracted.");
      print("  Format Specs: Channels=${pcm.channels} | Rate=${pcm.sampleRate}Hz | Depth=${pcm.bitsPerSample}-bit depth layer.");
    } else {
      print("[FAILURE] speakToMemory execution sequence faulted.");
    }
  }

  testSection("Tactile Braille Refresh & Combined Output Targets");
  if ((currentEngineFeatures & SralFeatureFlags.supportsBraille) != 0) {
    checkSRAL(sral.braille("DART BINDINGS"), "sral.braille pin layout updates");
  }
  checkSRAL(sral.output("Unified distribution test tracking endpoint paths.", true), "sral.output combined pipeline paths");
  sral.delay(2000);
    testSection("Asynchronous Threaded Queue Loops (delayOutput Methods)");
  if ((currentFeatures & SralFeatureFlags.speech) != 0) {
    print("Dispatching speech items onto asynchronous background delay processing thread pipelines (Default Engine)...");
    checkSRAL(sral.delayOutput("Staged delay message number one.", 0, true, true, false, false), "delayOutput 1 (Flushing Queue Context Instantly)");
    checkSRAL(sral.delayOutput("Staged delay message number two.", 1500, false, true, false, false), "delayOutput 2 (Staged Timing Enqueueing step)");
    
    print("Halting Dart application thread loop execution context to give background processing loop worker threads room to deplete...");
    sral.delay(3500);

    if (specificEngineForExTests != SralEngineFlags.none) {
      int featEx = sral.getEngineFeatures(specificEngineForExTests);
      if ((featEx & SralFeatureFlags.speech) != 0) {
        String nameEx = sral.getEngineName(specificEngineForExTests);
        print("Staging text configurations onto async background queue workers targeting specific engine: $nameEx...");
        
        checkSRAL(sral.delayOutputEx(specificEngineForExTests, "Explicitly targeted background queue message step one.", 0, true, true, false, false), "delayOutputEx 1 (Flushing Explicit Target Instance)");
        checkSRAL(sral.delayOutputEx(specificEngineForExTests, "Explicitly targeted background queue message step two.", 1500, false, true, false, false), "delayOutputEx 2 (Staged Explicit Target Instance Timing Enqueueing)");
        
        print("Halting Dart thread frame to allow the explicit driver loop context ($nameEx) to deplete thread stacks...");
        sral.delay(3500);
      }
    }
  } else {
    print("Auditory speech delivery pipelines are disabled. Skipping asynchronous queue thread validations.");
  }

  testSection("Dynamic Engine Exclusion List Adjustment Modifications");
  print("Current global exclusion tracking filter profile bitmask: 0x${originalEnginesToExclude.toRadixString(16).toUpperCase()}");
  
  int experimentalExclusionMask = SralEngineFlags.sapi | SralEngineFlags.narrator;
  print("Updating system filter bitmask parameters to: 0x${experimentalExclusionMask.toRadixString(16).toUpperCase()}");
  
  if (sral.setEnginesExclude(experimentalExclusionMask)) {
    int freshlyFetchedExclusionMask = sral.getEnginesExclude();
    print("New dynamic filter bitmask profile value confirmed by engine get channel feedback: 0x${freshlyFetchedExclusionMask.toRadixString(16).toUpperCase()}");
    check(freshlyFetchedExclusionMask == experimentalExclusionMask, "Dynamic profile exclusion changes verified successfully.", "Dynamic filter parameters failed value alignment validations!");
    
    sral.setEnginesExclude(originalEnginesToExclude);
  } else {
    print("Native framework rejected dynamic exclusion tracking parameter adjustments.");
  }

  testSection("Global Access Keyboard Hook Cleanup Deconstruction");
  sral.unregisterKeyboardHooks();
  print("unregisterKeyboardHooks executed. Monitoring listener threads severed.");
  promptUser("Keyboard hooks severed. Verify system transparency by typing Ctrl/Shift inputs with upcoming speech outputs.");
  sral.speak("Verifying systemic transparency after unregistering background keyboard listener thread contexts.", true);
  sral.delay(3000);

  testSection("Core Library Uninitialization Framework Teardown (SRAL_Uninitialize)");
  sral.uninitialize();
  print("uninitialize function handle called. Releasing references.");
  check(!sral.isInitialized(), "isInitialized accurately returns false following uninitialization.", "Teardown error tracking validation boundary failure!");

  print("\nAttempting to call speech synthesis routines post-uninitialization framework context teardown (Should safely evaluate as no-op return false):");
  if (sral.speak("This sentence should be caught by uninitialized guard blocks and drop silently.", false)) {
    print("[WARNING] speak wrapper returned true indicating potential framework state resource retention leaks!");
  } else {
    print("[INFO] speak wrapper evaluated accurately and returned false inside uninitialized boundary bounds.");
  }

  promptUser("All Dart/Flutter integration verification suites executed completely. Press Enter to terminate process context.");
}

void errorHandlingDemo() {
  print("\n=== Error Handling Demo ===");
  try {
    final sralErr = SRAL();
    print("Attempting call invocation without running initialization pipelines...");
    bool fallbackCallResult = sralErr.speak("This action block is bound to fail cleanly.", true);
    print("Result feedback: $fallbackCallResult (should evaluate to false)");
  } catch (err) {
    print("Exception caught inside boundary handler structures: $err");
  }
}
