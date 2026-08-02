using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Runtime.CompilerServices;

namespace SralCSharp
{
	public enum SralEngineCategory : uint
	{
		Unknown = 0,
		ScreenReader = 1,
		TextToSpeechEngine = 2,
		AccessibilityProvider = 3
	}

	public enum SralEngineParameters : uint
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
	public enum SralEngines : uint
	{
		None = 0,
		Nvda = 1u << 1,
		Jaws = 1u << 2,
		Zdsr = 1u << 3,
		Narrator = 1u << 4,
		Uia = 1u << 5,
		Sapi = 1u << 6,
		SpeechDispatcher = 1u << 7,
		Orca = 1u << 8,
		VoiceOver = 1u << 9,
		NSSpeech = 1u << 10,
		AvSpeech = 1u << 11,
		AndroidAccessibilityManager = 1u << 12,
		AndroidTextToSpeech = 1u << 13,
		ChromeVox = 1u << 14,
		AccessKit = 1u << 15,
	}

	[Flags]
	public enum SralSupportedFeatures : uint
	{
		None = 0,
		SRAL_SUPPORTS_SPEECH = 1u << 1,
		SRAL_SUPPORTS_BRAILLE = 1u << 2,
		SRAL_SUPPORTS_SPEECH_RATE = 1u << 3,
		SRAL_SUPPORTS_SPEECH_VOLUME = 1u << 4,
		SRAL_SUPPORTS_SELECT_VOICE = 1u << 5,
		SRAL_SUPPORTS_PAUSE_SPEECH = 1u << 6,
		SRAL_SUPPORTS_SSML = 1u << 7,
		SRAL_SUPPORTS_SPEAK_TO_MEMORY = 1u << 8,
		SRAL_SUPPORTS_SPELLING = 1u << 9
	};

	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct SralVoiceInfo
	{
		public byte* Name;
		public byte* Language;
		public byte* Gender;
		public byte* Vendor;
		public int Index;

		public readonly string? NameString => Name == null ? null : Marshal.PtrToStringAnsi((nint)Name);
		public readonly string? LanguageString => Language == null ? null : Marshal.PtrToStringAnsi((nint)Language);
		public readonly string? GenderString => Gender == null ? null : Marshal.PtrToStringAnsi((nint)Gender);
		public readonly string? VendorString => Vendor == null ? null : Marshal.PtrToStringAnsi((nint)Vendor);
	}

	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct PcmBuffer
	{
		public byte* DataPointer;
		public nuint DataLength;
		public int Channels;
		public int SampleRate;
		public int BitsPerSample;

		public readonly bool IsEmpty => DataPointer == null || DataLength == 0;

		public readonly ReadOnlySpan<byte> Data => IsEmpty
			? ReadOnlySpan<byte>.Empty
			: new ReadOnlySpan<byte>(DataPointer, (int)DataLength);
	}

	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct StringView
	{
		public byte* Data;
		public nuint Length;
	}

	public static unsafe partial class Sral
	{
		private const string LibraryName = "SRAL";
		static Sral()
		{
			NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), ResolveNativeLibraryLocation);
		}

		private static nint ResolveNativeLibraryLocation(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
		{
			if (libraryName != LibraryName) return nint.Zero;
			if (OperatingSystem.IsIOS() || OperatingSystem.IsTvOS() || OperatingSystem.IsWatchOS())
			{
				return NativeLibrary.GetMainProgramHandle();
			}

			string fileName;
			if (OperatingSystem.IsWindows())
			{
				fileName = "SRAL.dll";
			}
			else if (OperatingSystem.IsMacOS() || OperatingSystem.IsMacCatalyst())
			{
				fileName = "libsral.dylib";
			}
			else
			{
				fileName = "libsral.so";
			}

			if (NativeLibrary.TryLoad(fileName, assembly, searchPath, out nint handle))
			{
				return handle;
			}

			string fallbackPath = Path.Combine(AppContext.BaseDirectory, fileName);
			if (NativeLibrary.TryLoad(fallbackPath, out handle))
			{
				return handle;
			}

			return IntPtr.Zero;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		private static bool ExecuteWithView(string input, Func<StringView, bool> unmanagedCall)
		{
			if (string.IsNullOrEmpty(input)) return false;

			int byteCount = Encoding.UTF8.GetByteCount(input);
			byte* buffer = stackalloc byte[byteCount];

			fixed (char* pStr = input)
			{
				Encoding.UTF8.GetBytes(pStr, input.Length, buffer, byteCount);
			}

			StringView view = new() { Data = buffer, Length = (nuint)byteCount };
			return unmanagedCall(view);
		}

		public static void* Malloc(nuint size) => SralNative.SRAL_Malloc(size);

   		public static void* Malloc(int size) => SralNative.SRAL_Malloc((nuint)size);

		public static void Free(void* memory) => SralNative.SRAL_Free(memory);

		public static void Free(nint memory) => SralNative.SRAL_Free((void*)memory);

		public static bool Initialize(SralEngines enginesExclude = SralEngines.None) => SralNative.SRAL_Initialize((int)enginesExclude);

		public static void Uninitialize() => SralNative.SRAL_Uninitialize();

		public static bool IsInitialized() => SralNative.SRAL_IsInitialized();

		public static bool Speak(string text, bool interrupt = false) =>
			ExecuteWithView(text, v => SralNative.SafeSpeakAllocationBridge(v, interrupt));

		public static bool SpeakSsml(string ssml, bool interrupt = false) =>
			ExecuteWithView(ssml, v => SralNative.SafeSpeakSsmlAllocationBridge(v, interrupt));

		public static bool Braille(string text) =>
			ExecuteWithView(text, v => SralNative.SafeBrailleAllocationBridge(v));

		public static bool Output(string text, bool interrupt = false) =>
			ExecuteWithView(text, v => SralNative.SafeOutputAllocationBridge(v, interrupt));

		public static bool StopSpeech() => SralNative.SRAL_StopSpeech();

		public static bool PauseSpeech() => SralNative.SRAL_PauseSpeech();

		public static bool ResumeSpeech() => SralNative.SRAL_ResumeSpeech();

		public static bool IsSpeaking() => SralNative.SRAL_IsSpeaking();

		public static SralEngines GetCurrentEngine() => (SralEngines)SralNative.SRAL_GetCurrentEngine();

		public static uint GetEngineFeatures(SralEngines engine = SralEngines.None) => (uint)SralNative.SRAL_GetEngineFeatures((int)engine);

		public static bool SpeakEx(SralEngines engine, string text, bool interrupt = false) =>
			ExecuteWithView(text, v => SralNative.SafeSpeakExAllocationBridge(engine, v, interrupt));

		public static bool SpeakSsmlEx(SralEngines engine, string ssml, bool interrupt = false) =>
			ExecuteWithView(ssml, v => SralNative.SafeSpeakSsmlExAllocationBridge(engine, v, interrupt));

		public static bool BrailleEx(SralEngines engine, string text) =>
			ExecuteWithView(text, v => SralNative.SafeBrailleExAllocationBridge(engine, v));

		public static bool OutputEx(SralEngines engine, string text, bool interrupt = false) =>
			ExecuteWithView(text, v => SralNative.SafeOutputExAllocationBridge(engine, v, interrupt));

		public static bool StopSpeechEx(SralEngines engine) => SralNative.SRAL_StopSpeechEx((int)engine);

		public static bool PauseSpeechEx(SralEngines engine) => SralNative.SRAL_PauseSpeechEx((int)engine);

		public static bool ResumeSpeechEx(SralEngines engine) => SralNative.SRAL_ResumeSpeechEx((int)engine);

		public static bool IsSpeakingEx(SralEngines engine) => SralNative.SRAL_IsSpeakingEx((int)engine);
		public static void Delay(int timeMs) => SralNative.SRAL_Delay(timeMs);

		public static bool DelayOutput(int timeMs, string text, bool interrupt = false) =>
			ExecuteWithView(text, v => SralNative.SafeDelayOutputAllocationBridge(timeMs, v, interrupt));

		public static bool DelayOutputEx(SralEngines engine, int timeMs, string text, bool interrupt = false) =>
			ExecuteWithView(text, v => SralNative.SafeDelayOutputExAllocationBridge(engine, timeMs, v, interrupt));

		public static bool RegisterKeyboardHooks() => SralNative.SRAL_RegisterKeyboardHooks();
		public static void UnregisterKeyboardHooks() => SralNative.SRAL_UnregisterKeyboardHooks();

		public static SralEngines GetAvailableEngines() => (SralEngines)SralNative.SRAL_GetAvailableEngines();
		public static SralEngines GetActiveEngines() => (SralEngines)SralNative.SRAL_GetActiveEngines();
		public static SralEngineCategory GetEngineCategory(SralEngines engine) => SralNative.SRAL_GetEngineCategory((int)engine);
		public static SralEngines GetTTSEngines() => (SralEngines)SralNative.SRAL_GetTTSEngines();
		public static SralEngines GetAssistiveTechEngines() => (SralEngines)SralNative.SRAL_GetAssistiveTechEngines();
		public static bool SetEnginesExclude(uint enginesExcludeMask) => SralNative.SRAL_SetEnginesExclude((int)enginesExcludeMask);
		public static uint? GetEnginesExclude() { int result = SralNative.SRAL_GetEnginesExclude(); return result == -1 ? null : (uint)result; }
		public static string GetEngineName(SralEngines engine)
		{
			StringView view = SralNative.GetEngineNameFastBridge(engine);
			if (view.Data == null || view.Length == 0) return string.Empty;
			return Encoding.UTF8.GetString(view.Data, (int)view.Length);
		}

		public static bool SetEngineParameter<T>(SralEngines engine, SralEngineParameters param, T value) where T : unmanaged =>
		SralNative.SRAL_SetEngineParameter((int)engine, (int)param, &value);

		public static bool SetEngineParameterContext(SralEngines engine, SralEngineParameters param, nint platformContextAddress) =>
			SralNative.SRAL_SetEngineParameter((int)engine, (int)param, (void*)platformContextAddress);

		public static bool GetEngineParameter<T>(SralEngines engine, SralEngineParameters param, out T value) where T : unmanaged
		{
			Unsafe.SkipInit(out value);
			fixed (T* pVal = &value)
			{
				return SralNative.SRAL_GetEngineParameter((int)engine, (int)param, pVal);
			}
		}


		public static bool GetEngineVoiceList(SralEngines engine, out SralVoiceInfo* voicesArray, out int voiceCount)
		{
			voicesArray = null;
			int count = 0;

			if (!SralNative.SRAL_GetEngineParameter((int)engine, (int)SralEngineParameters.VoiceCount, &count) || count <= 0)
			{
				voiceCount = 0;
				return false;
			}

			void* rawArrayPtr = null;
			if (SralNative.SRAL_GetEngineParameter((int)engine, (int)SralEngineParameters.VoiceProperties, &rawArrayPtr) && rawArrayPtr != null)
			{
				voicesArray = (SralVoiceInfo*)rawArrayPtr;
				voiceCount = count;
				return true;
			}

			voiceCount = 0;
			return false;
		}


		public static PcmBuffer SpeakToMemory(string text) => string.IsNullOrEmpty(text) ? default : SralNative.DirectMemoryBridge(text);
		public static PcmBuffer SpeakToMemoryEx(SralEngines engine, string text) => string.IsNullOrEmpty(text) ? default : SralNative.DirectMemoryExBridge(engine, text);
	}

	internal static unsafe partial class SralNative
	{
		private const string LibraryName = "SRAL";

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial void* SRAL_Malloc(nuint size);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial void SRAL_Free(void* memory);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SRAL_Initialize(int enginesExclude);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial void SRAL_Uninitialize();

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SRAL_IsInitialized();

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SRAL_StopSpeech();

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SRAL_PauseSpeech();

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SRAL_ResumeSpeech();

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SRAL_IsSpeaking();

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial int SRAL_GetCurrentEngine();

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial int SRAL_GetEngineFeatures(int engine);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SRAL_SetEngineParameter(int engine, int param, void* value);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SRAL_GetEngineParameter(int engine, int param, void* value);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial void SRAL_Delay(int time);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial int SRAL_GetAvailableEngines();

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial int SRAL_GetActiveEngines();

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial SralEngineCategory SRAL_GetEngineCategory(int engine);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial int SRAL_GetTTSEngines();

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial int SRAL_GetAssistiveTechEngines();

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SRAL_SetEnginesExclude(int enginesExclude);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial int SRAL_GetEnginesExclude();

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SRAL_StopSpeechEx(int engine);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SRAL_PauseSpeechEx(int engine);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SRAL_ResumeSpeechEx(int engine);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SRAL_IsSpeakingEx(int engine);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SRAL_RegisterKeyboardHooks();

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial void SRAL_UnregisterKeyboardHooks();

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SafeSpeakAllocationBridge(StringView text, [MarshalAs(UnmanagedType.U1)] bool interrupt);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SafeSpeakSsmlAllocationBridge(StringView ssml, [MarshalAs(UnmanagedType.U1)] bool interrupt);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SafeBrailleAllocationBridge(StringView text);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SafeOutputAllocationBridge(StringView text, [MarshalAs(UnmanagedType.U1)] bool interrupt);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SafeSpeakExAllocationBridge(SralEngines engine, StringView text, [MarshalAs(UnmanagedType.U1)] bool interrupt);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SafeSpeakSsmlExAllocationBridge(SralEngines engine, StringView ssml, [MarshalAs(UnmanagedType.U1)] bool interrupt);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SafeBrailleExAllocationBridge(SralEngines engine, StringView text);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SafeOutputExAllocationBridge(SralEngines engine, StringView text, [MarshalAs(UnmanagedType.U1)] bool interrupt);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SafeDelayOutputAllocationBridge(int time, StringView text, [MarshalAs(UnmanagedType.U1)] bool interrupt);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		[return: MarshalAs(UnmanagedType.U1)]
		public static partial bool SafeDelayOutputExAllocationBridge(SralEngines engine, int time, StringView text, [MarshalAs(UnmanagedType.U1)] bool interrupt);

		[LibraryImport(LibraryName, StringMarshalling = StringMarshalling.Utf8)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial PcmBuffer DirectMemoryBridge(string text);

		[LibraryImport(LibraryName, StringMarshalling = StringMarshalling.Utf8)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial PcmBuffer DirectMemoryExBridge(SralEngines engine, string text);

		[LibraryImport(LibraryName)]
		[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
		public static partial StringView GetEngineNameFastBridge(SralEngines engine);
	}
}