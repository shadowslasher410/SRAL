using System;
using System.Runtime.InteropServices;
using System.Threading;
using SralCSharp;

class Program
{
	static void PromptUser(string message)
	{
		Console.Write($"\n>>> {message} (Press Enter to continue)...");
		Console.ReadLine();
	}

	static void PrintEngineNames(SralEngineFlags engineBitmask, string title)
	{
		Console.WriteLine($"{title}:");
		if (engineBitmask == SralEngineFlags.None)
		{
			Console.WriteLine("  (None)\n");
			return;
		}

		bool found = false;
		for (int i = 1; i <= (1 << 14); i <<= 1)
		{
			SralEngineFlags flag = (SralEngineFlags)i;
			if ((engineBitmask & flag) != 0)
			{
				string name = Sral.GetEngineName((int)flag);
				Console.WriteLine($"  - {name} ({i})");
				found = true;
			}
		}

		if (!found && engineBitmask != SralEngineFlags.None)
		{
			Console.WriteLine($"  (Unknown bitmask: {(int)engineBitmask})");
		}
		Console.WriteLine();
	}

	static void PrintSupportedFeatures(int features)
	{
		Console.WriteLine($"Supported Features ({features}):");
		if (features == (int)SralFeatureFlags.None)
		{
			Console.WriteLine("  (None)\n");
			return;
		}

		if ((features & (int)SralFeatureFlags.SRAL_SUPPORTS_SPEECH) != 0) Console.WriteLine("  - SUPPORTS_SPEECH");
		if ((features & (int)SralFeatureFlags.SRAL_SUPPORTS_BRAILLE) != 0) Console.WriteLine("  - SUPPORTS_BRAILLE");
		if ((features & (int)SralFeatureFlags.SRAL_SUPPORTS_SPEECH_RATE) != 0) Console.WriteLine("  - SUPPORTS_SPEECH_RATE");
		if ((features & (int)SralFeatureFlags.SRAL_SUPPORTS_SPEECH_VOLUME) != 0) Console.WriteLine("  - SUPPORTS_SPEECH_VOLUME");
		if ((features & (int)SralFeatureFlags.SRAL_SUPPORTS_SELECT_VOICE) != 0) Console.WriteLine("  - SUPPORTS_SELECT_VOICE");
		if ((features & (int)SralFeatureFlags.SRAL_SUPPORTS_PAUSE_SPEECH) != 0) Console.WriteLine("  - SUPPORTS_PAUSE_SPEECH");
		if ((features & (int)SralFeatureFlags.SRAL_SUPPORTS_SSML) != 0) Console.WriteLine("  - SUPPORTS_SSML");
		if ((features & (int)SralFeatureFlags.SRAL_SUPPORTS_SPEAK_TO_MEMORY) != 0) Console.WriteLine("  - SUPPORTS_SPEAK_TO_MEMORY");
		if ((features & (int)SralFeatureFlags.SRAL_SUPPORTS_SPELLING) != 0) Console.WriteLine("  - SUPPORTS_SPELLING");
		Console.WriteLine();
	}

	static void TestSection(string name)
	{
		Console.WriteLine("\n\n========================================");
		Console.WriteLine($"  Testing: {name}");
		Console.WriteLine("========================================");
	}

	static void Check(bool condition, string successMsg, string failMsg)
	{
		if (condition) Console.WriteLine($"[SUCCESS] {successMsg}");
		else Console.WriteLine($"[FAILURE] {failMsg}");
	}

	static void CheckSRAL(bool condition, string actionDesc)
	{
		if (condition) Console.WriteLine($"[SUCCESS] {actionDesc}");
		else Console.WriteLine($"[FAILURE] {actionDesc}");
	}

	static string FlagStatus(bool fl) => fl ? "Enabled" : "Disabled";

	static void Main(string[] args)
	{
		Console.WriteLine("SRAL Tester");
		Console.WriteLine("-------------------------");

		TestSection("SRAL_IsInitialized (Before Initialization)");
		Check(!Sral.SRAL_IsInitialized(), "SRAL_IsInitialized correctly returns false before init.", "SRAL_IsInitialized returned true before init!");

		TestSection("SRAL_Initialize");
		SralEngineFlags enginesToExclude = SralEngineFlags.Uia;
		Console.WriteLine($"Attempting to initialize SRAL, excluding engines: {(int)enginesToExclude} ({Sral.GetEngineName((int)enginesToExclude)})");

		if (Sral.SRAL_Initialize((int)enginesToExclude))
		{
			Console.WriteLine("[SUCCESS] SRAL_Initialize successful.");
		}
		else
		{
			Console.WriteLine("[FAILURE] SRAL_Initialize failed. Some tests may not run. Exiting.");
			return;
		}
		Check(Sral.SRAL_IsInitialized(), "SRAL_IsInitialized correctly returns true after init.", "SRAL_IsInitialized returned false after init!");

		TestSection("Engine Information");
		SralEngineFlags availableEngines = (SralEngineFlags)Sral.SRAL_GetAvailableEngines();
		PrintEngineNames(availableEngines, "Available Engines on this Platform");

		SralEngineFlags activeEngines = (SralEngineFlags)Sral.SRAL_GetActiveEngines();
		PrintEngineNames(activeEngines, "Currently Active/Usable Engines");

		int currentEngineId = Sral.SRAL_GetCurrentEngine();
		Console.WriteLine($"Current Default Engine: {Sral.GetEngineName(currentEngineId)} ({currentEngineId})");

		Console.WriteLine("\nNames of all SRAL_Engines enum members:");
		for (int i = 1; i <= (1 << 14); i <<= 1)
		{
			Console.WriteLine($"  Engine ID {i}: {Sral.GetEngineName(i)}");
		}

		int specificEngineForExTests = (int)SralEngineFlags.None;
		if (activeEngines != SralEngineFlags.None)
		{
			for (int i = 1; i <= (1 << 14); i <<= 1)
			{
				SralEngineFlags flag = (SralEngineFlags)i;
				if ((activeEngines & flag) != 0 && (int)flag != currentEngineId)
				{
					specificEngineForExTests = (int)flag;
					break;
				}
			}
			if (specificEngineForExTests == (int)SralEngineFlags.None)
			{
				for (int i = 1; i <= (1 << 14); i <<= 1)
				{
					SralEngineFlags flag = (SralEngineFlags)i;
					if ((activeEngines & flag) != 0)
					{
						specificEngineForExTests = (int)flag;
						break;
					}
				}
			}
		}

		if (specificEngineForExTests != (int)SralEngineFlags.None)
		{
			Console.WriteLine($"\nWill use engine '{Sral.GetEngineName(specificEngineForExTests)}' ({specificEngineForExTests}) for specific engine (Ex) tests.");
		}
		else
		{
			Console.WriteLine("\nNo specific engine distinct from default for Ex tests.");
		}

		TestSection("Keyboard Hooks");
		if (Sral.SRAL_RegisterKeyboardHooks())
		{
			Console.WriteLine("[SUCCESS] SRAL_RegisterKeyboardHooks registered.");
			PromptUser("Keyboard hooks (Ctrl=Interrupt, Shift=Pause/Resume) are active. Test them with upcoming speech. Affects SAPI/SpeechDispatcher.");
		}
		else
		{
			Console.WriteLine("[INFO] SRAL_RegisterKeyboardHooks failed or did not register. Expected if no suitable engine is active.");
		}

		TestSection("SRAL_GetEngineFeatures");
		Console.WriteLine($"Features for Current Default Engine ({Sral.GetEngineName(currentEngineId)}):");
		int currentEngineFeatures = Sral.SRAL_GetEngineFeatures(0);
		PrintSupportedFeatures(currentEngineFeatures);

		if (specificEngineForExTests != (int)SralEngineFlags.None)
		{
			Console.WriteLine($"Features for Specific Engine selected for Ex tests ({Sral.GetEngineName(specificEngineForExTests)}):");
			int specificEngineFeatures = Sral.SRAL_GetEngineFeatures(specificEngineForExTests);
			PrintSupportedFeatures(specificEngineFeatures);
		}

		if ((currentEngineFeatures & (int)SralFeatureFlags.SRAL_SUPPORTS_SPEECH) != 0)
		{
			TestSection("SRAL_Speak (Default Engine)");
			CheckSRAL(Sral.SRAL_Speak("Testing SRAL Speak, not interrupting previous speech.", false), "SRAL_Speak (no interrupt)");
			Thread.Sleep(2000);
			CheckSRAL(Sral.SRAL_Speak("Testing SRAL Speak, interrupting previous speech.", true), "SRAL_Speak (interrupt)");
			Thread.Sleep(2000);

			if (specificEngineForExTests != (int)SralEngineFlags.None)
			{
				TestSection("SRAL_SpeakEx (Specific Engine)");
				int featuresEx = Sral.SRAL_GetEngineFeatures(specificEngineForExTests);
				if ((featuresEx & (int)SralFeatureFlags.SRAL_SUPPORTS_SPEECH) != 0)
				{
					CheckSRAL(Sral.SRAL_SpeakEx(specificEngineForExTests, "Testing SRAL SpeakEx, not interrupting.", false), "SRAL_SpeakEx (no interrupt)");
					Thread.Sleep(2000);
					CheckSRAL(Sral.SRAL_SpeakEx(specificEngineForExTests, "Testing SRAL SpeakEx, interrupting.", true), "SRAL_SpeakEx (interrupt)");
					Thread.Sleep(2000);
				}
			}
		}
		if ((currentEngineFeatures & (int)SralFeatureFlags.SRAL_SUPPORTS_SSML) != 0)
		{
			TestSection("SRAL_SpeakSsml (Default Engine)");
			string ssmlTest = "<speak>This is <prosody pitch='150%'>SSML</prosody> text.</speak>";
			CheckSRAL(Sral.SRAL_SpeakSsml(ssmlTest, true), "SRAL_SpeakSsml");
			Thread.Sleep(3000);

			if (specificEngineForExTests != (int)SralEngineFlags.None)
			{
				TestSection("SRAL_SpeakSsmlEx (Specific Engine)");
				int featuresEx = Sral.SRAL_GetEngineFeatures(specificEngineForExTests);
				if ((featuresEx & (int)SralFeatureFlags.SRAL_SUPPORTS_SSML) != 0)
				{
					CheckSRAL(Sral.SRAL_SpeakSsmlEx(specificEngineForExTests, ssmlTest, true), "SRAL_SpeakSsmlEx");
					Thread.Sleep(3000);
				}
				else
				{
					Console.WriteLine($"Specific engine {Sral.GetEngineName(specificEngineForExTests)} does not support SSML for SpeakSsmlEx.");
				}
			}
		}
		else
		{
			Console.WriteLine("Current default engine does not support SSML. Skipping SSML tests.");
		}

		if ((currentEngineFeatures & (int)SralFeatureFlags.SRAL_SUPPORTS_SPEAK_TO_MEMORY) != 0)
		{
			TestSection("SRAL_SpeakToMemory (Default Engine)");
			IntPtr pcmDataPtr = Sral.SRAL_SpeakToMemory("Testing speak to memory audio synthesis.",
				out ulong bufferSize, out int channels, out int sampleRate, out int bitsPerSample);

			if (pcmDataPtr != IntPtr.Zero)
			{
				Console.WriteLine("[SUCCESS] SRAL_SpeakToMemory successful.");
				Console.WriteLine($"  Buffer Size: {bufferSize} bytes");
				Console.WriteLine($"  Channels: {channels}");
				Console.WriteLine($"  Sample Rate: {sampleRate} Hz");
				Console.WriteLine($"  Bits Per Sample: {bitsPerSample}");

				byte[] managedBuffer = new byte[bufferSize];
				Marshal.Copy(pcmDataPtr, managedBuffer, 0, (int)bufferSize);
				Console.WriteLine("  Managed copy of PCM byte array successfully instantiated.");

				Sral.SRAL_free(pcmDataPtr);
				Console.WriteLine("  Native PCM buffer freed via SRAL_free.");
			}
			else
			{
				Console.WriteLine("[FAILURE] SRAL_SpeakToMemory failed.");
			}

			if (specificEngineForExTests != (int)SralEngineFlags.None)
			{
				TestSection("SRAL_SpeakToMemoryEx (Specific Engine)");
				int featuresEx = Sral.SRAL_GetEngineFeatures(specificEngineForExTests);
				if ((featuresEx & (int)SralFeatureFlags.SRAL_SUPPORTS_SPEAK_TO_MEMORY) != 0)
				{
					IntPtr pcmDataExPtr = Sral.SRAL_SpeakToMemoryEx(specificEngineForExTests, "Testing speak to memory ex.",
						out ulong exBufferSize, out int exChannels, out int exSampleRate, out int exBitsPerSample);

					if (pcmDataExPtr != IntPtr.Zero)
					{
						Console.WriteLine($"[SUCCESS] SRAL_SpeakToMemoryEx successful for engine {Sral.GetEngineName(specificEngineForExTests)}.");
						Console.WriteLine($"  Buffer Size: {exBufferSize} bytes");
						Console.WriteLine($"  Channels: {exChannels}");
						Console.WriteLine($"  Sample Rate: {exSampleRate} Hz");
						Console.WriteLine($"  Bits Per Sample: {exBitsPerSample}");

						Sral.SRAL_free(pcmDataExPtr);
						Console.WriteLine("  Native Ex PCM buffer freed via SRAL_free.");
					}
					else
					{
						Console.WriteLine($"[FAILURE] SRAL_SpeakToMemoryEx failed for engine {Sral.GetEngineName(specificEngineForExTests)}.");
					}
				}
				else
				{
					Console.WriteLine($"Specific engine {Sral.GetEngineName(specificEngineForExTests)} does not support Speak To Memory for SpeakToMemoryEx.");
				}
			}
		}
		else
		{
			Console.WriteLine("Current default engine does not support Speak To Memory. Skipping these tests.");
		}

		if ((currentEngineFeatures & (int)SralFeatureFlags.SRAL_SUPPORTS_SPEAK_TO_MEMORY) != 0)
		{
			TestSection("SRAL_SpeakToMemory (Default Engine)");
			IntPtr pcmDataPtr = Sral.SRAL_SpeakToMemory("Testing speak to memory audio synthesis.",
				out ulong bufferSize, out int channels, out int sampleRate, out int bitsPerSample);

			if (pcmDataPtr != IntPtr.Zero)
			{
				Console.WriteLine("[SUCCESS] SRAL_SpeakToMemory successful.");
				Console.WriteLine($"  Buffer Size: {bufferSize} bytes | Channels: {channels} | Sample Rate: {sampleRate} Hz | Depth: {bitsPerSample}-bit");
				Sral.SRAL_free(pcmDataPtr);
			}
			else
			{
				Console.WriteLine("[FAILURE] SRAL_SpeakToMemory failed.");
			}

			if (specificEngineForExTests != (int)SralEngineFlags.None)
			{
				int featuresEx = Sral.SRAL_GetEngineFeatures(specificEngineForExTests);
				if ((featuresEx & (int)SralFeatureFlags.SRAL_SUPPORTS_SPEAK_TO_MEMORY) != 0)
				{
					TestSection("SRAL_SpeakToMemoryEx (Specific Engine)");
					IntPtr pcmDataPtrEx = Sral.SRAL_SpeakToMemoryEx(specificEngineForExTests, "Testing speak to memory ex.",
						out ulong bufSizeEx, out int chanEx, out int rateEx, out int bitsEx);

					if (pcmDataPtrEx != IntPtr.Zero)
					{
						Console.WriteLine($"[SUCCESS] SRAL_SpeakToMemoryEx successful for engine {Sral.GetEngineName(specificEngineForExTests)}.");
						Console.WriteLine($"  Buffer Size: {bufSizeEx} bytes | Channels: {chanEx} | Sample Rate: {rateEx} Hz | Depth: {bitsEx}-bit");
						Sral.SRAL_free(pcmDataPtrEx);
					}
					else
					{
						Console.WriteLine($"[FAILURE] SRAL_SpeakToMemoryEx failed for engine {Sral.GetEngineName(specificEngineForExTests)}.");
					}
				}
			}
		}

		TestSection("SRAL Native Memory Allocation Helpers");
		UIntPtr allocationSize = (UIntPtr)1024;
		IntPtr manualBuffer = Sral.SRAL_malloc(allocationSize);

		if (manualBuffer != IntPtr.Zero)
		{
			Console.WriteLine($"[SUCCESS] SRAL_malloc successfully allocated {allocationSize} bytes on the native unmanaged heap.");
			Sral.SRAL_free(manualBuffer);
			Console.WriteLine("[SUCCESS] SRAL_free successfully released unmanaged buffer context safely.");
		}
		else
		{
			Console.WriteLine("[FAILURE] SRAL_malloc could not request buffer boundaries from host memory.");
		}

		if ((currentEngineFeatures & (int)SralFeatureFlags.SRAL_SUPPORTS_BRAILLE) != 0)
		{
			TestSection("SRAL_Braille (Default Engine)");
			PromptUser("Prepare to check Braille display for 'Testing SRAL Braille output.'");
			CheckSRAL(Sral.SRAL_Braille("Testing SRAL Braille output."), "SRAL_Braille");

			if (specificEngineForExTests != (int)SralEngineFlags.None)
			{
				TestSection("SRAL_BrailleEx (Specific Engine)");
				int featuresEx = Sral.SRAL_GetEngineFeatures(specificEngineForExTests);
				if ((featuresEx & (int)SralFeatureFlags.SRAL_SUPPORTS_BRAILLE) != 0)
				{
					PromptUser("Prepare to check Braille display for 'Testing SRAL Braille Ex output.'");
					CheckSRAL(Sral.SRAL_BrailleEx(specificEngineForExTests, "Testing SRAL Braille Ex output."), "SRAL_BrailleEx");
				}
				else
				{
					Console.WriteLine($"Specific engine {Sral.GetEngineName(specificEngineForExTests)} does not support Braille for BrailleEx.");
				}
			}
		}
		else
		{
			Console.WriteLine("Current default engine does not support Braille. Skipping Braille tests.");
		}

		TestSection("SRAL_Output (Default Engine)");
		PromptUser("Prepare for SRAL_Output (Speech and/or Braille) for 'Testing SRAL Output, not interrupting.'");
		CheckSRAL(Sral.SRAL_Output("Testing SRAL Output, not interrupting.", false), "SRAL_Output (no interrupt)");
		Thread.Sleep(2000);
		PromptUser("Prepare for SRAL_Output (Speech and/or Braille) for 'Testing SRAL Output, interrupting.'");
		CheckSRAL(Sral.SRAL_Output("Testing SRAL Output, interrupting now.", true), "SRAL_Output (interrupt)");
		Thread.Sleep(2000);

		if (specificEngineForExTests != (int)SralEngineFlags.None)
		{
			TestSection("SRAL_OutputEx (Specific Engine)");
			PromptUser("Prepare for SRAL_OutputEx with specific engine for 'Testing SRAL OutputEx, not interrupting.'");
			CheckSRAL(Sral.SRAL_OutputEx(specificEngineForExTests, "Testing SRAL OutputEx, not interrupting.", false), "SRAL_OutputEx (no interrupt)");
			Thread.Sleep(2000);
			PromptUser("Prepare for SRAL_OutputEx with specific engine for 'Testing SRAL OutputEx, interrupting.'");
			CheckSRAL(Sral.SRAL_OutputEx(specificEngineForExTests, "Testing SRAL OutputEx, interrupting now.", true), "SRAL_OutputEx (interrupt)");
			Thread.Sleep(2000);
		}

		if ((currentEngineFeatures & (int)SralFeatureFlags.SRAL_SUPPORTS_SPEECH) != 0)
		{
			TestSection("Speech Control (Default Engine)");
			string longSpeech = "This is a moderately long sentence designed to test the pause, resume, and stop functionality of the SRAL library effectively.";
			Console.WriteLine($"Speaking long sentence with default engine: \"{longSpeech}\"");
			Sral.SRAL_Speak(longSpeech, true);
			PromptUser("Speech started. Press Enter to attempt PAUSE (if supported).");
			Console.WriteLine($"IsSpeaking status: {Sral.SRAL_IsSpeaking().ToString().ToLower()}");

			if ((currentEngineFeatures & (int)SralFeatureFlags.SRAL_SUPPORTS_PAUSE_SPEECH) != 0)
			{
				CheckSRAL(Sral.SRAL_PauseSpeech(), "SRAL_PauseSpeech");
				PromptUser("Speech Paused (hopefully). Press Enter to attempt RESUME.");
				CheckSRAL(Sral.SRAL_ResumeSpeech(), "SRAL_ResumeSpeech");
				PromptUser("Speech Resumed (hopefully). Press Enter to STOP.");
			}
			else
			{
				Console.WriteLine("Pause/Resume not supported by current default engine according to features. Will attempt stop directly.");
				PromptUser("Speech should be ongoing. Press Enter to STOP.");
			}
			CheckSRAL(Sral.SRAL_StopSpeech(), "SRAL_StopSpeech");
			Console.WriteLine("Speech should be stopped now.");
			Thread.Sleep(500);

			if (specificEngineForExTests != (int)SralEngineFlags.None)
			{
				TestSection("Speech Control Ex (Specific Engine)");
				int featuresEx = Sral.SRAL_GetEngineFeatures(specificEngineForExTests);
				if ((featuresEx & (int)SralFeatureFlags.SRAL_SUPPORTS_SPEECH) != 0)
				{
					string engineName = Sral.GetEngineName(specificEngineForExTests);
					Console.WriteLine($"Speaking long sentence with engine {engineName}: \"{longSpeech}\"");
					Sral.SRAL_SpeakEx(specificEngineForExTests, longSpeech, true);
					PromptUser("Speech started (Ex). Press Enter to attempt PAUSE (Ex) (if supported).");

					string status = Sral.SRAL_IsSpeakingEx(specificEngineForExTests) ? "true" : "False";
					Console.WriteLine($"IsSpeaking status: {status}");

					if ((featuresEx & (int)SralFeatureFlags.SRAL_SUPPORTS_PAUSE_SPEECH) != 0)
					{
						CheckSRAL(Sral.SRAL_PauseSpeechEx(specificEngineForExTests), "SRAL_PauseSpeechEx");
						PromptUser("Speech Paused (Ex). Press Enter to attempt RESUME (Ex).");
						CheckSRAL(Sral.SRAL_ResumeSpeechEx(specificEngineForExTests), "SRAL_ResumeSpeechEx");
						PromptUser("Speech Resumed (Ex). Press Enter to STOP (Ex).");
					}
					else
					{
						Console.WriteLine($"Pause/Resume not supported by specific engine {engineName}. Will attempt stop directly.");
						PromptUser("Speech should be ongoing (Ex). Press Enter to STOP (Ex).");
					}

					CheckSRAL(Sral.SRAL_StopSpeechEx(specificEngineForExTests), "SRAL_StopSpeechEx");
					Console.WriteLine("Speech should be stopped (Ex).\n");
					Thread.Sleep(500);
				}
				else
				{
					Console.WriteLine($"Specific engine {Sral.GetEngineName(specificEngineForExTests)} does not support speech. Skipping Speech Control Ex tests.");
				}
			}

			TestSection("SRAL Platform Engine Telemetry & Exclusions");
			Console.WriteLine("\nQuerying Categories and Active States of known engines:");
			int activeEnginesMask = Sral.SRAL_GetActiveEngines();
			foreach (SralEngineFlags flag in Enum.GetValues(typeof(SralEngineFlags)))
			{
				if (flag != SralEngineFlags.None && flag != SralEngineFlags.Any)
				{
					int eVal = (int)flag;
					string name = Sral.GetEngineName(eVal);
					
					if (name != "Unknown Engine")
					{
						SralEngineCategory category = Sral.SRAL_GetEngineCategory(eVal);
						bool active = (activeEnginesMask & eVal) == eVal;

						string catStr = "Unknown";
						if (category == SralEngineCategory.ScreenReader) catStr = "Screen Reader";
						else if (category == SralEngineCategory.TextToSpeechEngine) catStr = "Text to Speech";
						else if (category == SralEngineCategory.AccessibilityProvider) catStr = "Accessibility Provider";

						Console.WriteLine($"  - Engine: {name,-25} | Category: {catStr,-22} | Active: {active}");
					}
				}
			}

			TestSection("SRAL Asynchronous Threaded Delay Queue Output");
			if ((currentEngineFeatures & (int)SralFeatureFlags.SRAL_SUPPORTS_SPEECH) != 0)
			{
				Console.WriteLine("Dispatching speech items onto asynchronous background delay processing thread pipelines (Default Engine)...");
				CheckSRAL(Sral.SRAL_DelayOutput("Staged delay message number one.", 0, true, true, false, false), "DelayOutput 1 (Immediate Queueing)");
				CheckSRAL(Sral.SRAL_DelayOutput("Staged delay message number two.", 1500, false, true, false, false), "DelayOutput 2 (Staged Queueing)");
				Console.WriteLine("Waiting for default engine async thread loop processing context to exhaust...");
				Thread.Sleep(3500);

				if (specificEngineForExTests != (int)SralEngineFlags.None)
				{
					int featuresEx = Sral.SRAL_GetEngineFeatures(specificEngineForExTests);
					if ((featuresEx & (int)SralFeatureFlags.SRAL_SUPPORTS_SPEECH) != 0)
					{
						string engineName = Sral.GetEngineName(specificEngineForExTests);
						Console.WriteLine($"Dispatching speech items onto async delay queues for specific engine: {engineName}...");
						CheckSRAL(Sral.SRAL_DelayOutputEx(specificEngineForExTests, "Staged delay message ex number one.", 0, true, true, false, false), "DelayOutputEx 1 (Immediate Queueing)");
						CheckSRAL(Sral.SRAL_DelayOutputEx(specificEngineForExTests, "Staged delay message ex number two.", 1500, false, true, false, false), "DelayOutputEx 2 (Staged Queueing)");
						Console.WriteLine($"Waiting for specific engine {engineName} async thread loop to exhaust...");
						Thread.Sleep(3500);
					}
				}
			}
			else
			{
				Console.WriteLine("Speech feature not supported. Skipping asynchronous delay queue tests.");
			}

			TestSection("Unified Multi-Platform Categories Validation");
			int originalEnginesToExclude = Sral.SRAL_GetEnginesExclude();
			int ttsMask = Sral.SRAL_GetTTSEngines();
			int atMask = Sral.SRAL_GetAssistiveTechEngines();
			Console.WriteLine($"Platform derived pure Text-to-Speech engines bitmask: 0x{ttsMask:X}");
			Console.WriteLine($"Platform derived active Assistive Tech engines bitmask: 0x{atMask:X}");
			Sral.SRAL_SetEnginesExclude(originalEnginesToExclude);
			
			TestSection("Unregister Keyboard Hooks");
			Sral.SRAL_UnregisterKeyboardHooks();
			Console.WriteLine("SRAL_UnregisterKeyboardHooks called. Hooks should now be inactive (if they were active).");
			PromptUser("Try Ctrl/Shift with next speech to confirm hooks are off (if they were previously working).");
			Sral.SRAL_Speak("Testing speech output after attempting to unregister keyboard hooks.", true);
			Thread.Sleep(3000);

			TestSection("SRAL_Uninitialize");
			Sral.SRAL_Uninitialize();
			Console.WriteLine("SRAL_Uninitialize called.");
			Check(!Sral.SRAL_IsInitialized(), "SRAL_IsInitialized correctly returns false after uninit.", "SRAL_IsInitialized returned true after uninit!");

			Console.WriteLine("\nAttempting to speak after uninitialization (should fail or do nothing):");
			if (Sral.SRAL_Speak("This speech should not happen.", false))
			{
				Console.WriteLine("[WARNING] SRAL_Speak appeared to succeed after uninitialization!");
			}
			else
			{
				Console.WriteLine("[INFO] SRAL_Speak correctly failed or did nothing after uninitialization.");
			}

			ErrorHandlingDemo();

			PromptUser("All tests complete. Press Enter to exit.");
		} 

		static void ErrorHandlingDemo()
		{
			Console.WriteLine("\n=== Error Handling Demo ===");
			try
			{
				Console.WriteLine("Attempting operation without initialization...");
				bool result = Sral.SRAL_Speak("This should fail", true);
				Console.WriteLine($"Result: {result} (should be False)");
			}
			catch (Exception e)
			{
				Console.WriteLine($"Error caught: {e.Message}");
			}
		}
	}
}
