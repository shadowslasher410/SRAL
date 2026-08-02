using SralCSharp;

class Program
{
	static void PromptUser(string message)
	{
		Console.Write($"\n>>> {message} (Press Enter to continue)...");
		Console.ReadLine();
	}

	static void PrintEngineNames(SralEngines engineBitmask, string title)
	{
		Console.WriteLine($"{title}:");
		if (engineBitmask == SralEngines.None)
		{
			Console.WriteLine("  (None)\n");
			return;
		}

		bool found = false;
		for (uint i = 1; i <= (1u << 15); i <<= 1)
		{
			SralEngines engine = (SralEngines)i;
			if ((engineBitmask & engine) != SralEngines.None)
			{
				string name = Sral.GetEngineName(engine);
				if (string.IsNullOrEmpty(name))
				{
					name = "Unnamed Platform Engine Token";
				}

				Console.WriteLine($"  - {name} (Bit-Shift Value: {i})");
				found = true;
			}
		}

		if (!found && engineBitmask != SralEngines.None)
		{
			Console.WriteLine($"  (Unknown bitmask representation value: {(uint)engineBitmask})");
		}
		Console.WriteLine();
	}


	static void PrintSupportedFeatures(uint features)
	{
		Console.WriteLine($"Supported Features ({features}):");
		if (features == (uint)SralSupportedFeatures.None)
		{
			Console.WriteLine("  (None)\n");
			return;
		}

		if ((features & (uint)SralSupportedFeatures.SRAL_SUPPORTS_SPEECH) != 0) Console.WriteLine("  - SUPPORTS_SPEECH");
		if ((features & (uint)SralSupportedFeatures.SRAL_SUPPORTS_BRAILLE) != 0) Console.WriteLine("  - SUPPORTS_BRAILLE");
		if ((features & (uint)SralSupportedFeatures.SRAL_SUPPORTS_SPEECH_RATE) != 0) Console.WriteLine("  - SUPPORTS_SPEECH_RATE");
		if ((features & (uint)SralSupportedFeatures.SRAL_SUPPORTS_SPEECH_VOLUME) != 0) Console.WriteLine("  - SUPPORTS_SPEECH_VOLUME");
		if ((features & (uint)SralSupportedFeatures.SRAL_SUPPORTS_SELECT_VOICE) != 0) Console.WriteLine("  - SUPPORTS_SELECT_VOICE");
		if ((features & (uint)SralSupportedFeatures.SRAL_SUPPORTS_PAUSE_SPEECH) != 0) Console.WriteLine("  - SUPPORTS_PAUSE_SPEECH");
		if ((features & (uint)SralSupportedFeatures.SRAL_SUPPORTS_SSML) != 0) Console.WriteLine("  - SUPPORTS_SSML");
		if ((features & (uint)SralSupportedFeatures.SRAL_SUPPORTS_SPEAK_TO_MEMORY) != 0) Console.WriteLine("  - SUPPORTS_SPEAK_TO_MEMORY");
		if ((features & (uint)SralSupportedFeatures.SRAL_SUPPORTS_SPELLING) != 0) Console.WriteLine("  - SUPPORTS_SPELLING");
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
		Check(!Sral.IsInitialized(), "SRAL_IsInitialized correctly returns false before init.", "SRAL_IsInitialized returned true before init!");

		TestSection("SRAL_Initialize");
		SralEngines enginesToExclude = SralEngines.Uia;
		string excludedEngineName = Sral.GetEngineName(enginesToExclude);
		if (string.IsNullOrEmpty(excludedEngineName))
		{
			excludedEngineName = enginesToExclude.ToString();
		}
		Console.WriteLine($"Attempting to initialize SRAL, excluding engines: {(uint)enginesToExclude} ({excludedEngineName})");

		if (Sral.Initialize(enginesToExclude))
		{
			Console.WriteLine("[SUCCESS] SRAL_Initialize successful.");
		}
		else
		{
			Console.WriteLine("[FAILURE] SRAL_Initialize failed. Some tests may not run. Exiting.");
			return;
		}
		Check(Sral.IsInitialized(), "SRAL_IsInitialized correctly returns true after init.", "SRAL_IsInitialized returned false after init!");

		TestSection("Engine Information");
		SralEngines availableEngines = Sral.GetAvailableEngines();
		PrintEngineNames(availableEngines, "Available Engines on this Platform");

		SralEngines activeEngines = Sral.GetActiveEngines();
		PrintEngineNames(activeEngines, "Currently Active/Usable Engines");

		SralEngines currentEngineId = Sral.GetCurrentEngine();
		Console.WriteLine($"Current Default Engine: {Sral.GetEngineName(currentEngineId)} ({currentEngineId})");

		Console.WriteLine("\nNames of all SRAL_Engines enum members:");
		for (uint i = 1; i <= (1u << 15); i <<= 1)
		{
			SralEngines engineEnumVal = (SralEngines)i;
			string engineName = Sral.GetEngineName(engineEnumVal);
			if (!string.IsNullOrEmpty(engineName))
			{
				Console.WriteLine($"  Engine ID {i}: {engineName}");
			}
		}

		SralEngines specificEngineForExTests = SralEngines.None;
		if (activeEngines != SralEngines.None)
		{
			for (uint i = 1; i <= (1u << 15); i <<= 1)
			{
				SralEngines flag = (SralEngines)i;
				if ((activeEngines & flag) != SralEngines.None && flag != currentEngineId)
				{
					specificEngineForExTests = flag;
					break;
				}
			}
			if (specificEngineForExTests == (uint)SralEngines.None)
			{
				for (uint i = 1; i <= (1u << 15); i <<= 1)
				{
					SralEngines flag = (SralEngines)i;
					if ((activeEngines & flag) != SralEngines.None)
					{
						specificEngineForExTests = flag;
						break;
					}
				}
			}
		}

		if (specificEngineForExTests != (uint)SralEngines.None)
		{
			Console.WriteLine($"\nWill use engine '{Sral.GetEngineName(specificEngineForExTests)}' ({specificEngineForExTests}) for specific engine (Ex) tests.");
		}
		else
		{
			Console.WriteLine("\nNo specific engine distinct from default for Ex tests.");
		}

		TestSection("Keyboard Hooks");
		if (Sral.RegisterKeyboardHooks())
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

		uint currentEngineFeaturesRaw = Sral.GetEngineFeatures(currentEngineId);
		SralSupportedFeatures currentEngineFeatures = (SralSupportedFeatures)currentEngineFeaturesRaw;
		PrintSupportedFeatures(currentEngineFeaturesRaw);

		if (specificEngineForExTests != SralEngines.None)
		{
			Console.WriteLine($"Features for Specific Engine selected for Ex tests ({Sral.GetEngineName(specificEngineForExTests)}):");
			uint specificEngineFeatures = Sral.GetEngineFeatures(specificEngineForExTests);
			PrintSupportedFeatures(specificEngineFeatures);
		}

		if ((currentEngineFeatures & SralSupportedFeatures.SRAL_SUPPORTS_SPEECH) != SralSupportedFeatures.None)
		{
			TestSection("SRAL_Speak (Default Engine)");
			CheckSRAL(Sral.Speak("Testing SRAL Speak, not interrupting previous speech.", false), "SRAL_Speak (no interrupt)");
			Thread.Sleep(2000);
			CheckSRAL(Sral.Speak("Testing SRAL Speak, interrupting previous speech.", true), "SRAL_Speak (interrupt)");
			Thread.Sleep(2000);

			if (specificEngineForExTests != SralEngines.None)
			{
				TestSection("SRAL_SpeakEx (Specific Engine)");
				SralSupportedFeatures featuresEx = (SralSupportedFeatures)Sral.GetEngineFeatures(specificEngineForExTests);
				if ((featuresEx & SralSupportedFeatures.SRAL_SUPPORTS_SPEECH) != SralSupportedFeatures.None)
				{
					CheckSRAL(Sral.SpeakEx(specificEngineForExTests, "Testing SRAL SpeakEx, not interrupting.", false), "SRAL_SpeakEx (no interrupt)");
					Thread.Sleep(2000);
					CheckSRAL(Sral.SpeakEx(specificEngineForExTests, "Testing SRAL SpeakEx, interrupting.", true), "SRAL_SpeakEx (interrupt)");
					Thread.Sleep(2000);
				}
			}
		}

		if ((currentEngineFeatures & SralSupportedFeatures.SRAL_SUPPORTS_SSML) != SralSupportedFeatures.None)
		{
			TestSection("SRAL_SpeakSsml (Default Engine)");
			string ssmlTest = "<speak>This is <prosody pitch='150%'>SSML</prosody> text.</speak>";
			CheckSRAL(Sral.SpeakSsml(ssmlTest, true), "SRAL_SpeakSsml");
			Thread.Sleep(3000);

			if (specificEngineForExTests != SralEngines.None)
			{
				TestSection("SRAL_SpeakSsmlEx (Specific Engine)");
				SralSupportedFeatures featuresEx = (SralSupportedFeatures)Sral.GetEngineFeatures(specificEngineForExTests);
				if ((featuresEx & SralSupportedFeatures.SRAL_SUPPORTS_SSML) != SralSupportedFeatures.None)
				{
					CheckSRAL(Sral.SpeakSsmlEx(specificEngineForExTests, ssmlTest, true), "SRAL_SpeakSsmlEx");
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

		if ((currentEngineFeatures & SralSupportedFeatures.SRAL_SUPPORTS_SPEAK_TO_MEMORY) != SralSupportedFeatures.None)
		{
			TestSection("SRAL_SpeakToMemory (Default Engine)");

			PcmBuffer pcmBuffer = Sral.SpeakToMemory("Testing speak to memory audio synthesis.");
			{
				if (!pcmBuffer.IsEmpty)
				{
					Console.WriteLine("[SUCCESS] SRAL_SpeakToMemory successful.");
					Console.WriteLine($"  Channels: {pcmBuffer.Channels}");
					Console.WriteLine($"  Sample Rate: {pcmBuffer.SampleRate} Hz");
					Console.WriteLine($"  Bits Per Sample: {pcmBuffer.BitsPerSample}");
					ReadOnlySpan<byte> managedSpanView = pcmBuffer.Data;
					Console.WriteLine($"  Unmanaged memory block verified with data length: {managedSpanView.Length} bytes.");
				}
				else
				{
					Console.WriteLine("[FAILURE] SRAL_SpeakToMemory failed.");
				}
			}

			if (specificEngineForExTests != SralEngines.None)
			{
				TestSection("SRAL_SpeakToMemoryEx (Specific Engine)");
				SralSupportedFeatures featuresEx = (SralSupportedFeatures)Sral.GetEngineFeatures(specificEngineForExTests);
				if ((featuresEx & SralSupportedFeatures.SRAL_SUPPORTS_SPEAK_TO_MEMORY) != SralSupportedFeatures.None)
				{
					PcmBuffer pcmBufferEx = Sral.SpeakToMemoryEx(specificEngineForExTests, "Testing speak to memory ex.");
					{
						if (!pcmBufferEx.IsEmpty)
						{
							Console.WriteLine($"[SUCCESS] SRAL_SpeakToMemoryEx successful for engine {Sral.GetEngineName(specificEngineForExTests)}.");
							Console.WriteLine($"  Channels: {pcmBufferEx.Channels}");
							Console.WriteLine($"  Sample Rate: {pcmBufferEx.SampleRate} Hz");
							Console.WriteLine($"  Bits Per Sample: {pcmBufferEx.BitsPerSample}");
						}
						else
						{
							Console.WriteLine($"[FAILURE] SRAL_SpeakToMemoryEx failed for engine {Sral.GetEngineName(specificEngineForExTests)}.");
						}
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


		TestSection("SRAL Native Memory Allocation Helpers");

		nuint allocationSize = 1024;
		unsafe
		{
			void* manualBuffer = Sral.Malloc(allocationSize);

			if (manualBuffer != null)
			{
				Console.WriteLine($"[SUCCESS] SRAL_malloc successfully allocated {allocationSize} bytes on the native unmanaged heap.");

				Sral.Free(manualBuffer);
				Console.WriteLine("[SUCCESS] SRAL_free successfully released unmanaged buffer context safely.");
			}
			else
			{
				Console.WriteLine("[FAILURE] SRAL_malloc could not request buffer boundaries from host memory.");
			}
		}

		if ((currentEngineFeatures & SralSupportedFeatures.SRAL_SUPPORTS_BRAILLE) != SralSupportedFeatures.None)
		{
			TestSection("SRAL_Braille (Default Engine)");
			PromptUser("Prepare to check Braille display for 'Testing SRAL Braille output.'");
			CheckSRAL(Sral.Braille("Testing SRAL Braille output."), "SRAL_Braille");

			if (specificEngineForExTests != SralEngines.None)
			{
				TestSection("SRAL_BrailleEx (Specific Engine)");
				SralSupportedFeatures featuresEx = (SralSupportedFeatures)Sral.GetEngineFeatures(specificEngineForExTests);
				if ((featuresEx & SralSupportedFeatures.SRAL_SUPPORTS_BRAILLE) != SralSupportedFeatures.None)
				{
					PromptUser("Prepare to check Braille display for 'Testing SRAL Braille Ex output.'");
					CheckSRAL(Sral.BrailleEx(specificEngineForExTests, "Testing SRAL Braille Ex output."), "SRAL_BrailleEx");
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
		CheckSRAL(Sral.Output("Testing SRAL Output, not interrupting.", false), "SRAL_Output (no interrupt)");
		Thread.Sleep(2000);
		PromptUser("Prepare for SRAL_Output (Speech and/or Braille) for 'Testing SRAL Output, interrupting.'");
		CheckSRAL(Sral.Output("Testing SRAL Output, interrupting now.", true), "SRAL_Output (interrupt)");
		Thread.Sleep(2000);

		if (specificEngineForExTests != SralEngines.None)
		{
			TestSection("SRAL_OutputEx (Specific Engine)");
			PromptUser("Prepare for SRAL_OutputEx with specific engine for 'Testing SRAL OutputEx, not interrupting.'");
			CheckSRAL(Sral.OutputEx(specificEngineForExTests, "Testing SRAL OutputEx, not interrupting.", false), "SRAL_OutputEx (no interrupt)");
			Thread.Sleep(2000);
			PromptUser("Prepare for SRAL_OutputEx with specific engine for 'Testing SRAL OutputEx, interrupting.'");
			CheckSRAL(Sral.OutputEx(specificEngineForExTests, "Testing SRAL OutputEx, interrupting now.", true), "SRAL_OutputEx (interrupt)");
			Thread.Sleep(2000);
		}

		if ((currentEngineFeatures & SralSupportedFeatures.SRAL_SUPPORTS_SPEECH) != SralSupportedFeatures.None)
		{
			TestSection("Speech Control (Default Engine)");
			string longSpeech = "This is a moderately long sentence designed to test the pause, resume, and stop functionality of the SRAL library effectively.";
			Console.WriteLine($"Speaking long sentence with default engine: \"{longSpeech}\"");
			Sral.Speak(longSpeech, true);
			PromptUser("Speech started. Press Enter to attempt PAUSE (if supported).");
			Console.WriteLine($"IsSpeaking status: {Sral.IsSpeaking().ToString().ToLower()}");

			if ((currentEngineFeatures & SralSupportedFeatures.SRAL_SUPPORTS_PAUSE_SPEECH) != SralSupportedFeatures.None)
			{
				CheckSRAL(Sral.PauseSpeech(), "SRAL_PauseSpeech");
				PromptUser("Speech Paused (hopefully). Press Enter to attempt RESUME.");
				CheckSRAL(Sral.ResumeSpeech(), "SRAL_ResumeSpeech");
				PromptUser("Speech Resumed (hopefully). Press Enter to STOP.");
			}
			else
			{
				Console.WriteLine("Pause/Resume not supported by current default engine according to features. Will attempt stop directly.");
				PromptUser("Speech should be ongoing. Press Enter to STOP.");
			}
			CheckSRAL(Sral.StopSpeech(), "SRAL_StopSpeech");
			Console.WriteLine("Speech should be stopped now.");
			Thread.Sleep(500);

			if (specificEngineForExTests != SralEngines.None)
			{
				TestSection("Speech Control Ex (Specific Engine)");
				SralSupportedFeatures featuresEx = (SralSupportedFeatures)Sral.GetEngineFeatures(specificEngineForExTests);
				if ((featuresEx & SralSupportedFeatures.SRAL_SUPPORTS_SPEECH) != SralSupportedFeatures.None)
				{
					string engineName = Sral.GetEngineName(specificEngineForExTests);
					Console.WriteLine($"Speaking long sentence with engine {engineName}: \"{longSpeech}\"");
					Sral.SpeakEx(specificEngineForExTests, longSpeech, true);
					PromptUser("Speech started (Ex). Press Enter to attempt PAUSE (Ex) (if supported).");

					string status = Sral.IsSpeakingEx(specificEngineForExTests) ? "true" : "False";
					Console.WriteLine($"IsSpeaking status: {status}");

					if ((featuresEx & SralSupportedFeatures.SRAL_SUPPORTS_PAUSE_SPEECH) != SralSupportedFeatures.None)
					{
						CheckSRAL(Sral.PauseSpeechEx(specificEngineForExTests), "SRAL_PauseSpeechEx");
						PromptUser("Speech Paused (Ex). Press Enter to attempt RESUME (Ex).");
						CheckSRAL(Sral.ResumeSpeechEx(specificEngineForExTests), "SRAL_ResumeSpeechEx");
						PromptUser("Speech Resumed (Ex). Press Enter to STOP (Ex).");
					}
					else
					{
						Console.WriteLine($"Pause/Resume not supported by specific engine {engineName}. Will attempt stop directly.");
						PromptUser("Speech should be ongoing (Ex). Press Enter to STOP (Ex).");
					}

					CheckSRAL(Sral.StopSpeechEx(specificEngineForExTests), "SRAL_StopSpeechEx");
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

			SralEngines activeEnginesMask = Sral.GetActiveEngines();
			foreach (SralEngines flag in Enum.GetValues(typeof(SralEngines)))
			{
				if (flag != SralEngines.None)
				{
					string name = Sral.GetEngineName(flag);

					if (!string.IsNullOrEmpty(name) && name != "Unknown Engine")
					{
						SralEngineCategory category = Sral.GetEngineCategory(flag);
						bool active = (activeEnginesMask & flag) == flag;

						string catStr = category switch
						{
							SralEngineCategory.ScreenReader => "Screen Reader",
							SralEngineCategory.TextToSpeechEngine => "Text to Speech",
							SralEngineCategory.AccessibilityProvider => "Accessibility Provider",
							_ => "Unknown"
						};

						Console.WriteLine($"  - Engine: {name,-25} | Category: {catStr,-22} | Active: {active}");
					}
				}
			}

			TestSection("SRAL Asynchronous Threaded Delay Queue Output");
			if ((currentEngineFeatures & SralSupportedFeatures.SRAL_SUPPORTS_SPEECH) != SralSupportedFeatures.None)
			{
				Console.WriteLine("Dispatching speech items onto asynchronous background delay processing thread pipelines (Default Engine)...");

				CheckSRAL(Sral.DelayOutput(0, "Staged delay message number one.", true), "DelayOutput 1 (Immediate Queueing)");
				CheckSRAL(Sral.DelayOutput(1500, "Staged delay message number two.", false), "DelayOutput 2 (Staged Queueing)");
				Console.WriteLine("Waiting for default engine async thread loop processing context to exhaust...");
				Thread.Sleep(3500);

				if (specificEngineForExTests != SralEngines.None)
				{
					SralSupportedFeatures featuresEx = (SralSupportedFeatures)Sral.GetEngineFeatures(specificEngineForExTests);
					if ((featuresEx & SralSupportedFeatures.SRAL_SUPPORTS_SPEECH) != SralSupportedFeatures.None)
					{

						{
							string engineName = Sral.GetEngineName(specificEngineForExTests);
							Console.WriteLine($"Dispatching speech items onto async delay queues for specific engine: {engineName}...");
							CheckSRAL(Sral.DelayOutputEx(specificEngineForExTests, 0, "Staged delay message ex number one.", true), "DelayOutputEx 1 (Immediate Queueing)");
							CheckSRAL(Sral.DelayOutputEx(specificEngineForExTests, 1500, "Staged delay message ex number two.", false), "DelayOutputEx 2 (Staged Queueing)");
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
				uint? originalEnginesToExclude = Sral.GetEnginesExclude();
				SralEngines ttsMask = Sral.GetTTSEngines();
				SralEngines atMask = Sral.GetAssistiveTechEngines();
				Console.WriteLine($"Platform derived pure Text-to-Speech engines bitmask: 0x{(uint)ttsMask:X}");
				Console.WriteLine($"Platform derived active Assistive Tech engines bitmask: 0x{(uint)atMask:X}");
				Sral.SetEnginesExclude(originalEnginesToExclude ?? 0u);

				TestSection("Unregister Keyboard Hooks");
				Sral.UnregisterKeyboardHooks();
				Console.WriteLine("SRAL_UnregisterKeyboardHooks called. Hooks should now be inactive (if they were active).");
				PromptUser("Try Ctrl/Shift with next speech to confirm hooks are off (if they were previously working).");
				Sral.Speak("Testing speech output after attempting to unregister keyboard hooks.", true);
				Thread.Sleep(3000);

				TestSection("SRAL_Uninitialize");
				Sral.Uninitialize();
				Console.WriteLine("SRAL_Uninitialize called.");
				Check(!Sral.IsInitialized(), "SRAL_IsInitialized correctly returns false after uninit.", "SRAL_IsInitialized returned true after uninit!");

				Console.WriteLine("\nAttempting to speak after uninitialization (should fail or do nothing):");
				if (Sral.Speak("This speech should not happen.", false))
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
					bool result = Sral.Speak("This should fail", true);
					Console.WriteLine($"Result: {result} (should be False)");
				}
				catch (Exception e)
				{
					Console.WriteLine($"Error caught: {e.Message}");
				}
			}
		}
	}
}
