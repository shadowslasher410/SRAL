using System.Runtime.InteropServices;

namespace SRALBindings
{
    // --- Enums ---
    [Flags]
    public enum SRALEngine : int
    {
        None = 0, NVDA = 1 << 1, JAWS = 1 << 2, ZDSR = 1 << 3, Narrator = 1 << 4,
        UIA = 1 << 5, SAPI = 1 << 6, SpeechDispatcher = 1 << 7, 
        VoiceOver = 1 << 8, NSSpeech = 1 << 9, AVSpeech = 1 << 10
    }

    public enum SRALParam : int
    {
        SpeechRate = 0, SpeechVolume = 1, VoiceIndex = 2, VoiceProperties = 3,
        VoiceCount = 4, SymbolLevel = 5, SapiTrimThreshold = 6, 
        EnableSpelling = 7, UseCharacterDescriptions = 8, NvdaIsControlEx = 9
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct SRALVoiceInfo
    {
        public int Index;
        [MarshalAs(UnmanagedType.LPStr)] public string Name;
        [MarshalAs(UnmanagedType.LPStr)] public string Language;
        [MarshalAs(UnmanagedType.LPStr)] public string Gender;
        [MarshalAs(UnmanagedType.LPStr)] public string Vendor;
    }

    // --- Native Methods ---
    internal static partial class SRALNative
    {
        private const string LibName = "SRAL";

        [LibraryImport(LibName, EntryPoint = "SRAL_Initialize")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static partial bool Initialize(int enginesExclude);

        [LibraryImport(LibName, EntryPoint = "SRAL_Uninitialize")]
        public static partial void Uninitialize();

        [LibraryImport(LibName, EntryPoint = "SRAL_IsInitialized")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static partial bool IsInitialized();

        [LibraryImport(LibName, EntryPoint = "SRAL_Speak", StringMarshalling = StringMarshalling.Ansi)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static partial bool Speak([MarshalAs(UnmanagedType.LPStr)] string text, [MarshalAs(UnmanagedType.I1)] bool interrupt);

        [LibraryImport(LibName, EntryPoint = "SRAL_SpeakSsml", StringMarshalling = StringMarshalling.Ansi)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static partial bool SpeakSsml([MarshalAs(UnmanagedType.LPStr)] string ssml, [MarshalAs(UnmanagedType.I1)] bool interrupt);

        [LibraryImport(LibName, EntryPoint = "SRAL_SpeakToMemory", StringMarshalling = StringMarshalling.Ansi)]
        public static partial IntPtr SpeakToMemory([MarshalAs(UnmanagedType.LPStr)] string text, ref ulong size, ref int ch, ref int rate, ref int bits);

        [LibraryImport(LibName, EntryPoint = "SRAL_Braille", StringMarshalling = StringMarshalling.Ansi)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static partial bool Braille([MarshalAs(UnmanagedType.LPStr)] string text);

        [LibraryImport(LibName, EntryPoint = "SRAL_Output", StringMarshalling = StringMarshalling.Ansi)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static partial bool Output([MarshalAs(UnmanagedType.LPStr)] string text, [MarshalAs(UnmanagedType.I1)] bool interrupt);

        [LibraryImport(LibName, EntryPoint = "SRAL_StopSpeech")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static partial bool StopSpeech();

        [LibraryImport(LibName, EntryPoint = "SRAL_PauseSpeech")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static partial bool PauseSpeech();

        [LibraryImport(LibName, EntryPoint = "SRAL_ResumeSpeech")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static partial bool ResumeSpeech();

        [LibraryImport(LibName, EntryPoint = "SRAL_IsSpeaking")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static partial bool IsSpeaking();

        [LibraryImport(LibName, EntryPoint = "SRAL_GetCurrentEngine")]
        public static partial int GetCurrentEngine();

        [LibraryImport(LibName, EntryPoint = "SRAL_GetEngineParameter")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static partial bool GetEngineParameter(int engine, int param, IntPtr value);

        [LibraryImport(LibName, EntryPoint = "SRAL_SetEngineParameter")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static partial bool SetEngineParameter(int engine, int param, IntPtr value);

        [LibraryImport(LibName, EntryPoint = "SRAL_GetEngineName")]
        [return: MarshalAs(UnmanagedType.LPStr)]
        public static partial string GetEngineName(int engine);

        [LibraryImport(LibName, EntryPoint = "SRAL_free")]
        public static partial void Free(IntPtr ptr);

        [LibraryImport(LibName, EntryPoint = "SRAL_Delay")]
        public static partial void Delay(int ms);
    }

    // --- High-Level Wrapper ---
    public class SRAL : IDisposable
    {
        public bool Initialize(SRALEngine exclude = SRALEngine.None) => SRALNative.Initialize((int)exclude);
        public void Uninitialize() => SRALNative.Uninitialize();
        public bool IsInitialized() => SRALNative.IsInitialized();

        public bool Speak(string text, bool interrupt = true) => SRALNative.Speak(text, interrupt);
        public bool Stop() => SRALNative.StopSpeech();
        public bool IsSpeaking() => SRALNative.IsSpeaking();
        public string GetEngineName(SRALEngine engine) => SRALNative.GetEngineName((int)engine);
        public SRALEngine GetCurrentEngine() => (SRALEngine)SRALNative.GetCurrentEngine();

        public void Dispose() { if (IsInitialized()) Uninitialize(); }
    }
}
