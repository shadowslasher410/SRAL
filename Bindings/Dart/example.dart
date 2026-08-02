import 'dart:io';
import 'sral_types.dart';
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
  if (features == SralEngine.none) {
    print("  (None)");
    return;
  }
  if ((features & SralFeature.speech) != 0) print("  - SUPPORTS_SPEECH");
  if ((features & SralFeature.braille) != 0) print("  - SUPPORTS_BRAILLE");
  if ((features & SralFeature.speechRate) != 0) print("  - SUPPORTS_SPEECH_RATE");
  if ((features & SralFeature.speechVolume) != 0) print("  - SUPPORTS_SPEECH_VOLUME");
  if ((features & SralFeature.selectVoice) != 0) print("  - SUPPORTS_SELECT_VOICE");
  if ((features & SralFeature.pauseSpeech) != 0) print("  - SUPPORTS_PAUSE_SPEECH");
  if ((features & SralFeature.ssml) != 0) print("  - SUPPORTS_SSML");
  if ((features & SralFeature.speakToMemory) != 0) print("  - SUPPORTS_SPEAK_TO_MEMORY");
  if ((features & SralFeature.spelling) != 0) print("  - SUPPORTS_SPELLING");
  print("");
}

void printEngineNames(int engineBitmask, String title) {
  print("$title:");
  if (engineBitmask == SralEngine.none) {
    print("  (None)\n");
    return;
  }
  
  bool found = false;
  for (int i = 1; i <= 15; i++) {
    int flag = 1 << i;
    if ((engineBitmask & flag) != 0) {
      String name = Sral.getEngineName(flag);
      if (name.isEmpty) name = "Unnamed Core Driver Target";
      print("  - $name (0x${flag.toRadixString(16).toUpperCase()})");
      found = true;
    }
  }
  if (!found) print("  (Unknown bitmask: $engineBitmask)");
  print("");
}

void main() {
  print("SRAL Dart/Flutter Tester\n-------------------------");

  testSection("sral.isInitialized (Before Initialization)");
  check(!Sral.isInitialized(), "isInitialized correctly returns false before init.", "isInitialized returned true before init!");

  testSection("sral.initialize");
  int enginesToExclude = SralEngine.uia; 
  print("Attempting to initialize SRAL, excluding engines flag: $enginesToExclude (${Sral.getEngineName(enginesToExclude)})");

  if (Sral.initialize(enginesExclude: enginesToExclude)) {
    print("[SUCCESS] SRAL_Initialize successful.");
  } else {
    print("[FAILURE] SRAL_Initialize failed. Exiting.");
    return;
  }
  check(Sral.isInitialized(), "isInitialized correctly returns true after init.", "isInitialized returned false after init!");

  testSection("Engine Telemetry Information");
  int availableEngines = Sral.getAvailableEngines();
  printEngineNames(availableEngines, "Available Engines on this Platform");

  int activeEngines = Sral.getActiveEngines();
  printEngineNames(activeEngines, "Currently Active/Usable Engines");

  int currentEngineId = Sral.getCurrentEngine();
  print("Current Default Engine: ${Sral.getEngineName(currentEngineId)} ($currentEngineId)");

  int specificEngineForExTests = SralEngine.none;
  for (int i = 1; i <= 15; i++) {
    int flag = 1 << i;
    if (((activeEngines & flag) != 0) && flag != currentEngineId) {
      specificEngineForExTests = flag;
      break;
    }
  }
  if (specificEngineForExTests == SralEngine.none && activeEngines != SralEngine.none) {
    for (int i = 1; i <= 15; i++) {
      int flag = 1 << i;
      if ((activeEngines & flag) != 0) {
        specificEngineForExTests = flag;
        break;
      }
    }
  }

  if (specificEngineForExTests != SralEngine.none) {
    print("\nWill use engine '${Sral.getEngineName(specificEngineForExTests)}' ($specificEngineForExTests) for specific engine (Ex) tests.");
  } else {
    print("\nNo distinct specific engine active for explicit Ex tests.");
  }

  testSection("Keyboard Hooks Activation Layer");
  if (Sral.registerKeyboardHooks()) {
    print("[SUCCESS] global keyboard hooks registered.");
    promptUser("Keyboard hooks (Ctrl=Interrupt, Shift=Pause) active. Test them during speech checks.");
  } else {
    print("[INFO] registerKeyboardHooks failed or unsupported on this specific target driver platform context.");
  }

  testSection("sral.getEngineFeatures");
  print("Features for Current Default Engine (${Sral.getEngineName(currentEngineId)}):");
  int currentEngineFeatures = Sral.getEngineFeatures(SralEngine.none);
  printSupportedFeatures(currentEngineFeatures);

  if (specificEngineForExTests != SralEngine.none) {
    print("Features for Specific Engine selected for Ex tests (${Sral.getEngineName(specificEngineForExTests)}):");
    int specificEngineFeatures = Sral.getEngineFeatures(specificEngineForExTests);
    printSupportedFeatures(specificEngineFeatures);
  }

  if ((currentEngineFeatures & SralFeature.speech) != 0) {
    testSection("sral.speak (Default Engine)");
    checkSRAL(Sral.speak("Testing SRAL Speak from Dart, not interrupting previous speech.", interrupt: false), "sral.speak (no interrupt)");
    Sral.delay(2000);
    checkSRAL(Sral.speak("Testing SRAL Speak from Dart, interrupting previous speech now.", interrupt: true), "sral.speak (interrupt)");
    Sral.delay(2000);

    if (specificEngineForExTests != SralEngine.none) {
      testSection("sral.speakEx (Specific Engine)");
      int featuresEx = Sral.getEngineFeatures(specificEngineForExTests);
      if ((featuresEx & SralFeature.speech) != 0) {
        checkSRAL(Sral.speakEx(specificEngineForExTests, "Testing explicit sub-driver routing configurations using SpeakEx.", interrupt: false), "sral.speakEx (no interrupt)");
        Sral.delay(2000);
        checkSRAL(Sral.speakEx(specificEngineForExTests, "Testing explicit sub-driver routing configurations using SpeakEx, interrupting.", interrupt: true), "sral.speakEx (interrupt)");
        Sral.delay(2000);
      }
    }

    testSection("Speech Flow Control (Pause / Resume / Stop)");
    String longSpeech = "This is a moderately long sentence configuration designed to validate the runtime pausing, resuming, and stopping workflows natively.";
    print("Speaking sequence: \"$longSpeech\"");
    Sral.speak(longSpeech, interrupt: true);
    Sral.delay(1000);
    print("IsSpeaking checking loop status flag: ${Sral.isSpeaking()}");

    if ((currentEngineFeatures & SralFeature.pauseSpeech) != 0) {
      checkSRAL(Sral.pauseSpeech(), "sral.pauseSpeech");
      promptUser("Speech engine paused. Verify silence.");
      checkSRAL(Sral.resumeSpeech(), "sral.resumeSpeech");
      Sral.delay(1500);
    } else {
      print("Pause/Resume not supported by current default engine. Will attempt stop directly.");
      promptUser("Speech should be ongoing. Press Enter to STOP.");
    }
    checkSRAL(Sral.stopSpeech(), "sral.stopSpeech");
    print("IsSpeaking status post stop instruction: ${Sral.isSpeaking()}");
    Sral.delay(500);

    if (specificEngineForExTests != SralEngine.none) {
      testSection("Speech Control Ex (Specific Engine)");
      int featuresEx = Sral.getEngineFeatures(specificEngineForExTests);
      if ((featuresEx & SralFeature.speech) != 0) {
        String engineName = Sral.getEngineName(specificEngineForExTests);
        print("Speaking long sentence with engine $engineName: \"$longSpeech\"");
        Sral.speakEx(specificEngineForExTests, longSpeech, interrupt: true);
        promptUser("Speech started (Ex). Press Enter to attempt PAUSE (Ex) (if supported).");
        print("IsSpeaking status: ${Sral.isSpeakingEx(specificEngineForExTests)}");

        if ((featuresEx & SralFeature.pauseSpeech) != 0) {
          checkSRAL(Sral.pauseSpeechEx(specificEngineForExTests), "sral.pauseSpeechEx");
          promptUser("Speech Paused (Ex). Press Enter to attempt RESUME (Ex).");
          checkSRAL(Sral.resumeSpeechEx(specificEngineForExTests), "sral.resumeSpeechEx");
          promptUser("Speech Resumed (Ex). Press Enter to STOP (Ex).");
        } else {
          print("Pause/Resume not supported by specific engine $engineName. Will attempt stop directly.");
          promptUser("Speech should be ongoing (Ex). Press Enter to STOP (Ex).");
        }

        checkSRAL(Sral.stopSpeechEx(specificEngineForExTests), "sral.stopSpeechEx");
        print("Speech should be stopped (Ex).\n");
        Sral.delay(500);
      }
    }
  }

  testSection("Dynamic Voice Queries & Allocation");
  List<SralVoiceInfo> voices = Sral.getEngineVoiceList(currentEngineId);
  int voiceCount = voices.length;
  
  if (voiceCount > 0) {
    print("Voices found count metric: $voiceCount");
    for (var v in voices) {
      print("  - Voice [${v.index}]: Name='${v.name}' Language='${v.language}' Gender='${v.gender}' (Vendor: ${v.vendor})");
    }
  }

  testSection("SSML Parsing Verification");
  if ((currentEngineFeatures & SralFeature.ssml) != 0) {
    String ssmlText = "<speak>Testing <break time=\"400ms\"/> structural SSML markup tags.</speak>";
    checkSRAL(Sral.speakSsml(ssmlText, interrupt: true), "sral.speakSsml");
    Sral.delay(3000);

    if (specificEngineForExTests != SralEngine.none) {
      testSection("SSML Parsing Verification Ex (Specific Engine)");
      int featuresEx = Sral.getEngineFeatures(specificEngineForExTests);
      if ((featuresEx & SralFeature.ssml) != 0) {
        checkSRAL(Sral.speakSsmlEx(specificEngineForExTests, ssmlText, interrupt: true), "sral.speakSsmlEx");
        Sral.delay(3000);
      } else {
        print("Specific engine ${Sral.getEngineName(specificEngineForExTests)} does not support SSML for SpeakSsmlEx.");
      }
    }
  } else {
    print("Current default engine does not support SSML. Skipping SSML tests.");
}
    testSection("Raw Audio Memory Generation Framework (speakToMemory)");
  if ((currentEngineFeatures & SralFeature.speakToMemory) != 0) {
    print("Synthesizing raw string layouts into local application typed array structures...");
    
    final SralPcmBuffer pcm = Sral.speakToMemory("Audio serialization buffer check.");
    try {
      if (!pcm.isEmpty) {
        print("[SUCCESS] Managed data block generated successfully.");
        print("  Buffer Array Dimensions: ${pcm.bytes.length} bytes extracted.");
        print("  Format Specs: Channels=${pcm.channels} | Rate=${pcm.sampleRate}Hz | Depth=${pcm.bitsPerSample}-bit depth layer.");
      } else {
        print("[FAILURE] speakToMemory execution sequence faulted.");
      }
    } finally {
      pcm.dispose();
    }
  }


  testSection("Tactile Braille Refresh & Combined Output Targets");
  if ((currentEngineFeatures & SralFeature.braille) != 0) {
    checkSRAL(Sral.braille("DART BINDINGS"), "sral.braille pin layout updates");
  }
  checkSRAL(Sral.output("Unified distribution test tracking endpoint paths.", interrupt: true), "sral.output combined pipeline paths");
  Sral.delay(2000);
  
  testSection("Asynchronous Threaded Queue Loops (delayOutput Methods)");
  if ((currentEngineFeatures & SralFeature.speech) != 0) {
    print("Dispatching speech items onto asynchronous background delay processing thread pipelines (Default Engine)...");
    
    checkSRAL(Sral.delayOutput(0, "Staged delay message number one.", interrupt: true), "delayOutput 1 (Flushing Queue Context Instantly)");
    checkSRAL(Sral.delayOutput(1500, "Staged delay message number two.", interrupt: false), "delayOutput 2 (Staged Timing Enqueueing step)");
    
    print("Halting Dart application thread loop execution context to give background processing loop worker threads room to deplete...");
    Sral.delay(3500);

    if (specificEngineForExTests != SralEngine.none) {
      int featEx = SralNative.sralGetEngineFeatures(specificEngineForExTests);
      if ((featEx & SralFeature.speech) != 0) {
        String nameEx = Sral.getEngineName(specificEngineForExTests);
        print("Staging text configurations onto async background queue workers targeting specific engine: $nameEx...");
        
        checkSRAL(Sral.delayOutputEx(specificEngineForExTests, 0, "Explicitly targeted background queue message step one.", interrupt: true), "delayOutputEx 1 (Flushing Explicit Target Instance)");
        checkSRAL(Sral.delayOutputEx(specificEngineForExTests, 1500, "Explicitly targeted background queue message step two.", interrupt: false), "delayOutputEx 2 (Staged Explicit Target Instance Timing Enqueueing)");
        
        print("Halting Dart thread frame to allow the explicit driver loop context ($nameEx) to deplete thread stacks...");
        Sral.delay(3500);
      }
    }
  } else {
    print("Auditory speech delivery pipelines are disabled. Skipping asynchronous queue thread validations.");
  }

  testSection("Dynamic Engine Exclusion List Adjustment Modifications");
  final int? originalEnginesToExclude = Sral.getEnginesExclude();
  final actualExcludeMask = originalEnginesToExclude ?? SralEngine.none;
  print("Current global exclusion tracking filter profile bitmask: 0x${actualExcludeMask.toRadixString(16).toUpperCase()}");
  
  int experimentalExclusionMask = SralEngine.sapi | SralEngine.narrator;
  print("Updating system filter bitmask parameters to: 0x${experimentalExclusionMask.toRadixString(16).toUpperCase()}");
  
  if (Sral.setEnginesExclude(experimentalExclusionMask)) {
    final freshlyFetchedExclusionMask = Sral.getEnginesExclude() ?? SralEngine.none;
    print("New dynamic filter bitmask profile value confirmed by engine get channel feedback: 0x${freshlyFetchedExclusionMask.toRadixString(16).toUpperCase()}");
    
    check(freshlyFetchedExclusionMask == experimentalExclusionMask, "Dynamic profile exclusion changes verified successfully.", "Dynamic filter parameters failed value alignment validations!");
    
    Sral.setEnginesExclude(actualExcludeMask);
  } else {
    print("Native framework rejected dynamic exclusion tracking parameter adjustments.");
  }

  testSection("Global Access Keyboard Hook Cleanup Deconstruction");
  Sral.unregisterKeyboardHooks();
  print("unregisterKeyboardHooks executed. Monitoring listener threads severed.");
  promptUser("Keyboard hooks severed. Verify system transparency by typing Ctrl/Shift inputs with upcoming speech outputs.");
  Sral.speak("Verifying systemic transparency after unregistering background keyboard listener thread contexts.", interrupt: true);
  Sral.delay(3000);

  testSection("Core Library Uninitialization Framework Teardown (SRAL_Uninitialize)");
  Sral.uninitialize();
  print("uninitialize function handle called. Releasing references.");
  check(!Sral.isInitialized(), "isInitialized accurately returns false following uninitialization.", "Teardown error tracking validation boundary failure!");

  print("\nAttempting to call speech synthesis routines post-uninitialization framework context teardown (Should safely evaluate as no-op return false):");
  if (Sral.speak("This sentence should be caught by uninitialized guard blocks and drop silently.", interrupt: false)) {
    print("[WARNING] speak wrapper returned true indicating potential framework state resource retention leaks!");
  } else {
    print("[INFO] speak wrapper evaluated accurately and returned false inside uninitialized boundary bounds.");
  }

  promptUser("All Dart/Flutter integration verification suites executed completely. Press Enter to terminate process context.");
}

void errorHandlingDemo() {
  print("\n=== Error Handling Demo ===");
  try {
    print("Attempting call invocation without running initialization pipelines...");
    bool fallbackCallResult = Sral.speak("This action block is bound to fail cleanly.", interrupt: true);
    print("Result feedback: $fallbackCallResult (should evaluate to false)");
  } catch (err) {
    print("Exception caught inside boundary handler structures: $err");
  }
}