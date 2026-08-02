import 'dart:ffi';
import 'package:ffi/ffi.dart';
import 'dart:io';

class SralEngine {
  static const int none = 0;
  static const int current = 0;
  static const int noSpecified = 0;
  static const int nvda = 1 << 1;
  static const int jaws = 1 << 2;
  static const int zdsr = 1 << 3;
  static const int narrator = 1 << 4;
  static const int uia = 1 << 5;
  static const int sapi = 1 << 6;
  static const int speechDispatcher = 1 << 7;
  static const int orca = 1 << 8;
  static const int voiceOver = 1 << 9;
  static const int nsSpeech = 1 << 10;
  static const int avSpeech = 1 << 11;
  static const int androidAccessibilityManager = 1 << 12;
  static const int androidTextToSpeech = 1 << 13;
  static const int chromeVox = 1 << 14;
  static const int accessKit = 1 << 15;
}

enum SralEngineCategory {
  unknown,
  screenReader,
  textToSpeechEngine,
  accessibilityProvider
}

class SralFeature {
  static const int speech = 1 << 1;
  static const int braille = 1 << 2;
  static const int speechRate = 1 << 3;
  static const int speechVolume = 1 << 4;
  static const int selectVoice = 1 << 5;
  static const int pauseSpeech = 1 << 6;
  static const int ssml = 1 << 7;
  static const int speakToMemory = 1 << 8;
  static const int spelling = 1 << 9;
}

class SralParam {
  static const int speechRate = 0;
  static const int speechVolume = 1;
  static const int voiceIndex = 2;
  static const int voiceProperties = 3;
  static const int voiceCount = 4;
  static const int symbolLevel = 5;
  static const int sapiTrimThreshold = 6;
  static const int enableSpelling = 7;
  static const int useCharacterDescriptions = 8;
  static const int nvdaIsControlEx = 9;
  static const int engineIsPaused = 10;
  static const int androidJniEnv = 11;
  static const int androidActivity = 12;
}

sealed class CSralVoiceInfo extends Struct {
  external Pointer<Utf8> name;
  external Pointer<Utf8> language;
  external Pointer<Utf8> gender;
  external Pointer<Utf8> vendor;

  @Int32()
  external int index;
}

class SralVoiceInfo {
  final int index;
  final String name;
  final String language;
  final String gender;
  final String vendor;

  SralVoiceInfo({
    required this.index,
    required this.name,
    required this.language,
    required this.gender,
    required this.vendor,
  });
}

sealed class PcmBuffer extends Struct {
  external Pointer<Uint8> dataPointer;
  
  @Size()
  external int dataLength;
  
  @Int32()
  external int channels;
  
  @Int32()
  external int sampleRate;
  
  @Int32()
  external int bitsPerSample;
}

sealed class StringView extends Struct {
  external Pointer<Utf8> data;
  
  @Size()
  external int length;
}

typedef _SralFreeC = Void Function(Pointer<Void> memory);
typedef _SralFreeDart = void Function(Pointer<Void> memory);

typedef _SralInitializeC = Bool Function(Int32 enginesExclude);
typedef _SralInitializeDart = bool Function(int enginesExclude);

typedef _SralVoidBoolC = Bool Function();
typedef _SralVoidBoolDart = bool Function();

typedef _SralVoidC = Void Function();
typedef _SralVoidDart = void Function();

typedef _SralGetIntC = Int32 Function();
typedef _SralGetIntDart = int Function();

typedef _SralEngineIntC = Int32 Function(Int32 engine);
typedef _SralEngineIntDart = int Function(int engine);

typedef _SralParamC = Bool Function(Int32 engine, Int32 param, Pointer<Void> value);
typedef _SralParamDart = bool Function(int engine, int param, Pointer<Void> value);

typedef _SralDelayC = Void Function(Int32 time);
typedef _SralDelayDart = void Function(int time);

typedef _SafeSpeakC = Bool Function(StringView text, Bool interrupt);
typedef _SafeSpeakDart = bool Function(StringView text, bool interrupt);

typedef _SafeBrailleC = Bool Function(StringView text);
typedef _SafeBrailleDart = bool Function(StringView text);

typedef _SafeSpeakExC = Bool Function(Uint32 engine, StringView text, Bool interrupt);
typedef _SafeSpeakExDart = bool Function(int engine, StringView text, bool interrupt);

typedef _SafeBrailleExC = Bool Function(Uint32 engine, StringView text);
typedef _SafeBrailleExDart = bool Function(int engine, StringView text);

typedef _SafeDelayC = Bool Function(Int32 time, StringView text, Bool interrupt);
typedef _SafeDelayDart = bool Function(int time, StringView text, bool interrupt);

typedef _SafeDelayExC = Bool Function(Uint32 engine, Int32 time, StringView text, Bool interrupt);
typedef _SafeDelayExDart = bool Function(int engine, int time, StringView text, bool interrupt);

typedef _DirectMemoryC = PcmBuffer Function(Pointer<Utf8> text);
typedef _DirectMemoryDart = PcmBuffer Function(Pointer<Utf8> text);

typedef _DirectMemoryExC = PcmBuffer Function(Uint32 engine, Pointer<Utf8> text);
typedef _DirectMemoryExDart = PcmBuffer Function(int engine, Pointer<Utf8> text);

typedef _GetEngineNameFastC = StringView Function(Uint32 engine);
typedef _GetEngineNameFastDart = StringView Function(int engine);

class SralNative {
  static final DynamicLibrary _lib = _loadNativeLibrary();

  static DynamicLibrary _loadNativeLibrary() {
    if (Platform.isWindows) return DynamicLibrary.open('SRAL.dll');
    if (Platform.isMacOS) return DynamicLibrary.open('libssral.dylib');
    if (Platform.isIOS) return DynamicLibrary.process();
    return DynamicLibrary.open('libsral.so');
  }

  static final _SralFreeDart sralFree = _lib.lookupFunction<_SralFreeC, _SralFreeDart>('SRAL_free');
  static final _SralInitializeDart sralInitialize = _lib.lookupFunction<_SralInitializeC, _SralInitializeDart>('SRAL_Initialize');
  static final _SralVoidDart sralUninitialize = _lib.lookupFunction<_SralVoidC, _SralVoidDart>('SRAL_Uninitialize');
  static final _SralVoidBoolDart sralIsInitialized = _lib.lookupFunction<_SralVoidBoolC, _SralVoidBoolDart>('SRAL_IsInitialized');
  static final _SralVoidBoolDart sralStopSpeech = _lib.lookupFunction<_SralVoidBoolC, _SralVoidBoolDart>('SRAL_StopSpeech');
  static final _SralVoidBoolDart sralPauseSpeech = _lib.lookupFunction<_SralVoidBoolC, _SralVoidBoolDart>('SRAL_PauseSpeech');
  static final _SralVoidBoolDart sralResumeSpeech = _lib.lookupFunction<_SralVoidBoolC, _SralVoidBoolDart>('SRAL_ResumeSpeech');
  static final _SralVoidBoolDart sralIsSpeaking = _lib.lookupFunction<_SralVoidBoolC, _SralVoidBoolDart>('SRAL_IsSpeaking');
  static final _SralGetIntDart sralGetCurrentEngine = _lib.lookupFunction<_SralGetIntC, _SralGetIntDart>('SRAL_GetCurrentEngine');
  static final _SralEngineIntDart sralGetEngineFeatures = _lib.lookupFunction<_SralEngineIntC, _SralEngineIntDart>('SRAL_GetEngineFeatures');
  static final _SralParamDart sralSetEngineParameter = _lib.lookupFunction<_SralParamC, _SralParamDart>('SRAL_SetEngineParameter');
  static final _SralParamDart sralGetEngineParameter = _lib.lookupFunction<_SralParamC, _SralParamDart>('SRAL_GetEngineParameter');
  static final _SralDelayDart sralDelay = _lib.lookupFunction<_SralDelayC, _SralDelayDart>('SRAL_Delay');
  static final _SralGetIntDart sralGetAvailableEngines = _lib.lookupFunction<_SralGetIntC, _SralGetIntDart>('SRAL_GetAvailableEngines');
  static final _SralGetIntDart sralGetActiveEngines = _lib.lookupFunction<_SralGetIntC, _SralGetIntDart>('SRAL_GetActiveEngines');
  static final _SralEngineIntDart sralGetEngineCategory = _lib.lookupFunction<_SralEngineIntC, _SralEngineIntDart>('SRAL_GetEngineCategory');
  static final _SralGetIntDart sralGetTTSEngines = _lib.lookupFunction<_SralGetIntC, _SralGetIntDart>('SRAL_GetTTSEngines');
  static final _SralGetIntDart sralGetAssistiveTechEngines = _lib.lookupFunction<_SralGetIntC, _SralGetIntDart>('SRAL_GetAssistiveTechEngines');
  static final _SralInitializeDart sralSetEnginesExclude = _lib.lookupFunction<_SralInitializeC, _SralInitializeDart>('SRAL_SetEnginesExclude');
  static final _SralGetIntDart sralGetEnginesExclude = _lib.lookupFunction<_SralGetIntC, _SralGetIntDart>('SRAL_GetEnginesExclude');
  static final _SralEngineIntDart sralStopSpeechEx = _lib.lookupFunction<_SralEngineIntC, _SralEngineIntDart>('SRAL_StopSpeechEx');
  static final _SralEngineIntDart sralPauseSpeechEx = _lib.lookupFunction<_SralEngineIntC, _SralEngineIntDart>('SRAL_PauseSpeechEx');
  static final _SralEngineIntDart sralResumeSpeechEx = _lib.lookupFunction<_SralEngineIntC, _SralEngineIntDart>('SRAL_ResumeSpeechEx');
  static final _SralEngineIntDart sralIsSpeakingEx = _lib.lookupFunction<_SralEngineIntC, _SralEngineIntDart>('SRAL_IsSpeakingEx');
  static final _SralVoidBoolDart sralRegisterKeyboardHooks = _lib.lookupFunction<_SralVoidBoolC, _SralVoidBoolDart>('SRAL_RegisterKeyboardHooks');
  static final _SralVoidDart sralUnregisterKeyboardHooks = _lib.lookupFunction<_SralVoidC, _SralVoidDart>('SRAL_UnregisterKeyboardHooks');
  static final _SafeSpeakDart safeSpeakAllocationBridge = _lib.lookupFunction<_SafeSpeakC, _SafeSpeakDart>('SafeSpeakAllocationBridge');
  static final _SafeSpeakDart safeSpeakSsmlAllocationBridge = _lib.lookupFunction<_SafeSpeakC, _SafeSpeakDart>('SafeSpeakSsmlAllocationBridge');
  static final _SafeBrailleDart safeBrailleAllocationBridge = _lib.lookupFunction<_SafeBrailleC, _SafeBrailleDart>('SafeBrailleAllocationBridge');
  static final _SafeSpeakDart safeOutputAllocationBridge = _lib.lookupFunction<_SafeSpeakC, _SafeSpeakDart>('SafeOutputAllocationBridge');
  static final _SafeSpeakExDart safeSpeakExAllocationBridge = _lib.lookupFunction<_SafeSpeakExC, _SafeSpeakExDart>('SafeSpeakExAllocationBridge');
  static final _SafeSpeakExDart safeSpeakSsmlExAllocationBridge = _lib.lookupFunction<_SafeSpeakExC, _SafeSpeakExDart>('SafeSpeakSsmlExAllocationBridge');
  static final _SafeBrailleExDart safeBrailleExAllocationBridge = _lib.lookupFunction<_SafeBrailleExC, _SafeBrailleExDart>('SafeBrailleExAllocationBridge');
  static final _SafeSpeakExDart safeOutputExAllocationBridge = _lib.lookupFunction<_SafeSpeakExC, _SafeSpeakExDart>('SafeOutputExAllocationBridge');
  static final _SafeDelayDart safeDelayOutputAllocationBridge = _lib.lookupFunction<_SafeDelayC, _SafeDelayDart>('SafeDelayOutputAllocationBridge');
  static final _SafeDelayExDart safeDelayOutputExAllocationBridge = _lib.lookupFunction<_SafeDelayExC, _SafeDelayExDart>('SafeDelayOutputExAllocationBridge');
  static final _DirectMemoryDart directMemoryBridge = _lib.lookupFunction<_DirectMemoryC, _DirectMemoryDart>('DirectMemoryBridge');
  static final _DirectMemoryExDart directMemoryExBridge = _lib.lookupFunction<_DirectMemoryExC, _DirectMemoryExDart>('DirectMemoryExBridge');
  static final _GetEngineNameFastDart getEngineNameFastBridge = _lib.lookupFunction<_GetEngineNameFastC, _GetEngineNameFastDart>('GetEngineNameFastBridge');
}
