using System;
using System.Runtime.InteropServices;
using System.Reflection;

namespace SralCSharp
{
	public enum SralEngineCategory : int
	{
		Unknown = 0,
		ScreenReader = 1,
		TextToSpeechEngine = 2,
		AccessibilityProvider = 3
	}

	public enum SralEngineParameters : int
	{
		SpeechRate = 0,
		SpeechVolume = 1,
		VoiceIndex = 2,
		VoiceProperties = 3,
		VoiceCount = 4,
		SymbolLevel = 5,
		SapiTrimThreshold = 6,
		EnableSpelling = 7,
		UseCharacterDescriptions = 8,
		NvdaIsControlEx = 9,
		EngineISPaused = 10,
		AndroidJniEnv = 11,
		AndroidActivity = 12
	};

	[Flags]
	public enum SralEngineFlags : int
	{
		None = 0,
		Nvda = 1 << 1,
		Jaws = 1 << 2,
		Zdsr = 1 << 3,
		Narrator = 1 << 4,
		Uia = 1 << 5,
		Sapi = 1 << 6,
		SpeechDispatcher = 1 << 7,
		Orca = 1 << 8,
		VoiceOver = 1 << 9,
		NsSpeech = 1 << 10,
		AvSpeech = 1 << 11,
		AndroidAccessibilityManager = 1 << 12,
		AndroidTextToSpeech = 1 << 13,
		WebSpeech = 1 << 14,
		ChromeVox = 1 << 15,
		AccessKit = 1 << 16,
		Any = -1
	}

	[Flags]
	public enum SralFeatureFlags : int
	{
		None = 0,
		SRAL_SUPPORTS_SPEECH = 1 << 0,
		SRAL_SUPPORTS_BRAILLE = 1 << 1,
		SRAL_SUPPORTS_SPEECH_RATE = 1 << 2,
		SRAL_SUPPORTS_SPEECH_VOLUME = 1 << 3,
		SRAL_SUPPORTS_SELECT_VOICE = 1 << 4,
		SRAL_SUPPORTS_PAUSE_SPEECH = 1 << 5,
		SRAL_SUPPORTS_SSML = 1 << 6,
		SRAL_SUPPORTS_SPEAK_TO_MEMORY = 1 << 7,
		SRAL_SUPPORTS_SPELLING = 1 << 8
	};

	[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
	public struct SralVoiceInfo
	{
		public int Index;
		public string Name;
		public string Language;
		public string Gender;
		public string Vendor;
	}

	public static class Sral
	{
#if IOS || __IOS__
					private const string DllName = "__Internal";
#else
		private const string DllName = "SRAL";
#endif

		static Sral()
		{
#if !IOS && !__IOS__
			NativeLibrary.SetDllImportResolver(typeof(Sral).Assembly, ResolveSralNativeBinary);
#endif
		}

#if !IOS && !__IOS__
		private static IntPtr ResolveSralNativeBinary(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
		{
			if (libraryName != "SRAL")
			{
				return IntPtr.Zero;
			}

			string fileName;
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				fileName = "SRAL.dll";
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
			{
				fileName = "libsral.so";
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
			{
				fileName = "libsral.dylib";
			}
			else
			{
				return IntPtr.Zero;
			}

			string baseDir = AppContext.BaseDirectory;

			string cmakeBuildPath = Path.Combine(baseDir, "..", "..", "..", "out", "build", fileName);
			string relativeAssetPath = Path.Combine(baseDir, "runtimes", fileName);
			string applicationRootPath = Path.Combine(baseDir, fileName);

			if (File.Exists(cmakeBuildPath) && NativeLibrary.TryLoad(cmakeBuildPath, out IntPtr handleCMake))
			{
				return handleCMake;
			}
			if (File.Exists(relativeAssetPath) && NativeLibrary.TryLoad(relativeAssetPath, out IntPtr handleAsset))
			{
				return handleAsset;
			}
			if (File.Exists(applicationRootPath) && NativeLibrary.TryLoad(applicationRootPath, out IntPtr handleRoot))
			{
				return handleRoot;
			}

			return IntPtr.Zero;
		}
#endif

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern bool SRAL_Initialize(int enginesExclude);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern void SRAL_Uninitialize();

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern bool SRAL_IsInitialized();

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern bool SRAL_Speak(string text, bool interrupt);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern bool SRAL_SpeakSsml(string ssml, bool interrupt);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern bool SRAL_Braille(string text);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern bool SRAL_Output(string text, bool interrupt);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern bool SRAL_StopSpeech();

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern bool SRAL_PauseSpeech();

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern bool SRAL_ResumeSpeech();

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern bool SRAL_IsSpeaking();

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern void SRAL_Delay(int time);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern int SRAL_GetCurrentEngine();

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern int SRAL_GetEngineFeatures(int engine);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern SralEngineCategory SRAL_GetEngineCategory(int engine);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		private static extern IntPtr SRAL_GetEngineName(int engine);
		public static string GetEngineName(int engine)
		{
			IntPtr ptr = SRAL_GetEngineName(engine);
			return ptr == IntPtr.Zero ? "Unknown Engine" : (Marshal.PtrToStringAnsi(ptr) ?? "Unknown Engine");
		}

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern bool SRAL_SetEnginesExclude(int enginesExclude);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern int SRAL_GetEnginesExclude();

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern int SRAL_GetAvailableEngines();

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern int SRAL_GetActiveEngines();

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern int SRAL_GetTTSEngines();

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern int SRAL_GetAssistiveTechEngines();

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern bool SRAL_RegisterKeyboardHooks();

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern void SRAL_UnregisterKeyboardHooks();

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern bool SRAL_SpeakEx(int engine, string text, bool interrupt);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern bool SRAL_SpeakSsmlEx(int engine, string ssml, bool interrupt);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern bool SRAL_BrailleEx(int engine, string text);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern bool SRAL_OutputEx(int engine, string text, bool interrupt);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern bool SRAL_StopSpeechEx(int engine);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern bool SRAL_PauseSpeechEx(int engine);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern bool SRAL_ResumeSpeechEx(int engine);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern bool SRAL_IsSpeakingEx(int engine);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern IntPtr SRAL_SpeakToMemory(string text, out ulong bufferSize, out int channels, out int sampleRate, out int bitsPerSample);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern IntPtr SRAL_SpeakToMemoryEx(int engine, string text, out ulong bufferSize, out int channels, out int sampleRate, out int bitsPerSample);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern IntPtr SRAL_malloc(UIntPtr size);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern void SRAL_free(IntPtr memory);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern bool SRAL_DelayOutput(string text, int time, bool interrupt, bool speak, bool braille, bool ssml);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern bool SRAL_DelayOutputEx(int engine, string text, int time, bool interrupt, bool speak, bool braille, bool ssml);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		private static extern bool SRAL_SetEngineParameter(int engine, int param, ref int value);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		private static extern bool SRAL_GetEngineParameter(int engine, int param, ref int value);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		private static extern bool SRAL_SetEngineParameter(int engine, int param, IntPtr value);

		[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
		public static extern bool SRAL_GetEngineParameter(int engine, int param, out IntPtr value);

		public static bool SetIntParameter(int engine, SralEngineParameters param, int value)
		{
			return SRAL_SetEngineParameter(engine, (int)param, ref value);
		}

		public static int GetIntParameter(int engine, SralEngineParameters param)
		{
			int val = 0;
			return SRAL_GetEngineParameter(engine, (int)param, ref val) ? val : -1;
		}

		public static bool GetVoicesParameter(int engine, SralEngineParameters param, out SralVoiceInfo[] voices)
		{
			voices = Array.Empty<SralVoiceInfo>();

			int count = GetIntParameter(engine, SralEngineParameters.VoiceCount);
			if (count <= 0) return false;

			if (SRAL_GetEngineParameter(engine, (int)param, out IntPtr arrayPtr) && arrayPtr != IntPtr.Zero)
			{
				voices = new SralVoiceInfo[count];

				for (int i = 0; i < count; i++)
				{
					IntPtr currentPtrLocation = IntPtr.Add(arrayPtr, i * IntPtr.Size);

					IntPtr structPtr = Marshal.ReadIntPtr(currentPtrLocation);

					if (structPtr != IntPtr.Zero)
					{
						voices[i] = Marshal.PtrToStructure<SralVoiceInfo>(structPtr);
						SRAL_free(structPtr);
					}
				}
				SRAL_free(arrayPtr);
				return true;
			}
			return false;
		}

		public static bool SetAndroidContext(IntPtr jniEnv, IntPtr activityContext)
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Create("ANDROID")))
			{
				bool envOk = SRAL_SetEngineParameter(-1, (int)SralEngineParameters.AndroidJniEnv, jniEnv);
				bool actOk = SRAL_SetEngineParameter(-1, (int)SralEngineParameters.AndroidActivity, activityContext);
				return envOk && actOk;
			}
			return false;
		}

		public static bool IsApplePlatform()
		{
			return RuntimeInformation.IsOSPlatform(OSPlatform.OSX) || RuntimeInformation.IsOSPlatform(OSPlatform.Create("IOS"));
		}
	}
}
