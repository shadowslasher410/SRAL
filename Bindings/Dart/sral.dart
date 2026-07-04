import 'dart:ffi';
import 'dart:io';
import 'package:ffi/ffi.dart';
import 'sral_types.dart';

typedef _sral_init_c = Bool Function(Int32);
typedef _sral_init_dart = bool Function(int);

typedef _sral_uninit_c = Void Function();
typedef _sral_uninit_dart = void Function();

typedef _sral_is_init_c = Bool Function();
typedef _sral_is_init_dart = bool Function();

typedef _sral_speak_c = Bool Function(Pointer<Utf8>, Bool);
typedef _sral_speak_dart = bool Function(Pointer<Utf8>, bool);

typedef _sral_speak_ex_c = Bool Function(Int32, Pointer<Utf8>, Bool);
typedef _sral_speak_ex_dart = bool Function(int, Pointer<Utf8>, bool);

typedef _sral_braille_c = Bool Function(Pointer<Utf8>);
typedef _sral_braille_dart = bool Function(Pointer<Utf8>);

typedef _sral_braille_ex_c = Bool Function(Int32, Pointer<Utf8>);
typedef _sral_braille_ex_dart = bool Function(int, Pointer<Utf8>);

typedef _sral_control_c = Bool Function();
typedef _sral_control_dart = bool Function();

typedef _sral_control_ex_c = Bool Function(Int32);
typedef _sral_control_ex_dart = bool Function(int);

typedef _sral_get_int_c = Int32 Function();
typedef _sral_get_int_dart = int Function();

typedef _sral_get_int_ex_c = Int32 Function(Int32);
typedef _sral_get_int_ex_dart = int Function(int);

typedef _sral_param_c = Bool Function(Int32, Int32, Pointer<Void>);
typedef _sral_param_dart = bool Function(int, int, Pointer<Void>);

typedef _sral_name_c = Pointer<Utf8> Function(Int32);
typedef _sral_name_dart = Pointer<Utf8> Function(int);

typedef _sral_delay_c = Void Function(Int32);
typedef _sral_delay_dart = void Function(int);

typedef _sral_delay_out_c = Bool Function(Pointer<Utf8>, Int32, Bool, Bool, Bool, Bool);
typedef _sral_delay_out_dart = bool Function(Pointer<Utf8>, int, bool, bool, bool, bool);

typedef _sral_delay_out_ex_c = Bool Function(Int32, Pointer<Utf8>, Int32, Bool, Bool, Bool, Bool);
typedef _sral_delay_out_ex_dart = bool Function(int, Pointer<Utf8>, int, bool, bool, bool, bool);

typedef _sral_mem_c = Pointer<Void> Function(Pointer<Utf8>, Pointer<Uint64>, Pointer<Int32>, Pointer<Int32>, Pointer<Int32>);
typedef _sral_mem_dart = Pointer<Void> Function(Pointer<Utf8>, Pointer<Uint64>, Pointer<Int32>, Pointer<Int32>, Pointer<Int32>);

typedef _sral_mem_ex_c = Pointer<Void> Function(Int32, Pointer<Utf8>, Pointer<Uint64>, Pointer<Int32>, Pointer<Int32>, Pointer<Int32>);
typedef _sral_mem_ex_dart = Pointer<Void> Function(int, Pointer<Utf8>, Pointer<Uint64>, Pointer<Int32>, Pointer<Int32>, Pointer<Int32>);

typedef _sral_free_c = Void Function(Pointer<Void>);
typedef _sral_free_dart = void Function(Pointer<Void>);

class SRAL {
  late DynamicLibrary _lib;

  late _sral_init_dart _initialize;
  late _sral_uninit_dart _uninitialize;
  late _sral_is_init_dart _isInitialized;
  late _sral_speak_dart _speak;
  late _sral_speak_dart _speakSsml;
  late _sral_braille_dart _braille;
  late _sral_speak_dart _output;
  late _sral_control_dart _stopSpeech;
  late _sral_control_dart _pauseSpeech;
  late _sral_control_dart _resumeSpeech;
  late _sral_control_dart _isSpeaking;
  late _sral_delay_dart _delay;
  late _sral_get_int_dart _getCurrentEngine;
  late _sral_get_int_ex_dart _getEngineFeatures;
  late _sral_param_dart _setEngineParameter;
  late _sral_param_dart _getEngineParameter;
  late _sral_speak_ex_dart _speakEx;
  late _sral_speak_ex_dart _speakSsmlEx;
  late _sral_braille_ex_dart _brailleEx;
  late _sral_speak_ex_dart _outputEx;
  late _sral_control_ex_dart _stopSpeechEx;
  late _sral_control_ex_dart _pauseSpeechEx;
  late _sral_control_ex_dart _resumeSpeechEx;
  late _sral_control_ex_dart _isSpeakingEx;
  late _sral_control_dart _registerKeyboardHooks;
  late _sral_control_dart _unregisterKeyboardHooks;
  late _sral_get_int_dart _getAvailableEngines;
  late _sral_get_int_dart _getActiveEngines;
  late _sral_get_int_dart _getTTSEngines;
  late _sral_get_int_dart _getAssistiveTechEngines;
  late _sral_get_int_ex_dart _getEngineCategory;
  late _sral_name_dart _getEngineName;
  late _sral_init_dart _setEnginesExclude;
  late _sral_get_int_dart _getEnginesExclude;
  late _sral_delay_out_dart _delayOutput;
  late _sral_delay_out_ex_dart _delayOutputEx;
  late _sral_mem_dart _speakToMemory;
  late _sral_mem_ex_dart _speakToMemoryEx;
  late _sral_free_dart _sralFree;

  SRAL() {
    if (Platform.isWindows) {
      _lib = DynamicLibrary.open('SRAL.dll');
    } else if (Platform.isLinux) {
      _lib = DynamicLibrary.open('libsral.so');
    } else if (Platform.isMacOS) {
      _lib = DynamicLibrary.open('libsral.dylib');
    } else if (Platform.isAndroid) {
      _lib = DynamicLibrary.open('libsral.so');
    } else if (Platform.isIOS) {
      _lib = DynamicLibrary.process();
    } else {
      throw UnsupportedError('Unsupported operational target system environment');
    }

    _initialize = _lib.lookupFunction<_sral_init_c, _sral_init_dart>('SRAL_Initialize');
    _uninitialize = _lib.lookupFunction<_sral_uninit_c, _sral_uninit_dart>('SRAL_Uninitialize');
    _isInitialized = _lib.lookupFunction<_sral_is_init_c, _sral_is_init_dart>('SRAL_IsInitialized');
    _speak = _lib.lookupFunction<_sral_speak_c, _sral_speak_dart>('SRAL_Speak');
    _speakSsml = _lib.lookupFunction<_sral_speak_c, _sral_speak_dart>('SRAL_SpeakSsml');
    _braille = _lib.lookupFunction<_sral_braille_c, _sral_braille_dart>('SRAL_Braille');
    _output = _lib.lookupFunction<_sral_speak_c, _sral_speak_dart>('SRAL_Output');
    _stopSpeech = _lib.lookupFunction<_sral_control_c, _sral_control_dart>('SRAL_StopSpeech');
    _pauseSpeech = _lib.lookupFunction<_sral_control_c, _sral_control_dart>('SRAL_PauseSpeech');
    _resumeSpeech = _lib.lookupFunction<_sral_control_c, _sral_control_dart>('SRAL_ResumeSpeech');
    _isSpeaking = _lib.lookupFunction<_sral_control_c, _sral_control_dart>('SRAL_IsSpeaking');
    _delay = _lib.lookupFunction<_sral_delay_c, _sral_delay_dart>('SRAL_Delay');
    _getCurrentEngine = _lib.lookupFunction<_sral_get_int_c, _sral_get_int_dart>('SRAL_GetCurrentEngine');
    _getEngineFeatures = _lib.lookupFunction<_sral_get_int_ex_c, _sral_get_int_ex_dart>('SRAL_GetEngineFeatures');
    _setEngineParameter = _lib.lookupFunction<_sral_param_c, _sral_param_dart>('SRAL_SetEngineParameter');
    _getEngineParameter = _lib.lookupFunction<_sral_param_c, _sral_param_dart>('SRAL_GetEngineParameter');
    _speakEx = _lib.lookupFunction<_sral_speak_ex_c, _sral_speak_ex_dart>('SRAL_SpeakEx');
    _speakSsmlEx = _lib.lookupFunction<_sral_speak_ex_c, _sral_speak_ex_dart>('SRAL_SpeakSsmlEx');
    _brailleEx = _lib.lookupFunction<_sral_braille_ex_c, _sral_braille_ex_dart>('SRAL_BrailleEx');
    _outputEx = _lib.lookupFunction<_sral_speak_ex_c, _sral_speak_ex_dart>('SRAL_OutputEx');
    _stopSpeechEx = _lib.lookupFunction<_sral_control_ex_c, _sral_control_ex_dart>('SRAL_StopSpeechEx');
    _pauseSpeechEx = _lib.lookupFunction<_sral_control_ex_c, _sral_control_ex_dart>('SRAL_PauseSpeechEx');
    _resumeSpeechEx = _lib.lookupFunction<_sral_control_ex_c, _sral_control_ex_dart>('SRAL_ResumeSpeechEx');
    _isSpeakingEx = _lib.lookupFunction<_sral_control_ex_c, _sral_control_ex_dart>('SRAL_IsSpeakingEx');
    _registerKeyboardHooks = _lib.lookupFunction<_sral_control_c, _sral_control_dart>('SRAL_RegisterKeyboardHooks');
    _unregisterKeyboardHooks = _lib.lookupFunction<_sral_control_c, _sral_control_dart>('SRAL_UnregisterKeyboardHooks');
    _getAvailableEngines = _lib.lookupFunction<_sral_get_int_c, _sral_get_int_dart>('SRAL_GetAvailableEngines');
    _getActiveEngines = _lib.lookupFunction<_sral_get_int_c, _sral_get_int_dart>('SRAL_GetActiveEngines');
    _getTTSEngines = _lib.lookupFunction<_sral_get_int_c, _sral_get_int_dart>('SRAL_GetTTSEngines');
    _getAssistiveTechEngines = _lib.lookupFunction<_sral_get_int_c, _sral_get_int_dart>('SRAL_GetAssistiveTechEngines');
    _getEngineCategory = _lib.lookupFunction<_sral_get_int_ex_c, _sral_get_int_ex_dart>('SRAL_GetEngineCategory');
    _getEngineName = _lib.lookupFunction<_sral_name_c, _sral_name_dart>('SRAL_GetEngineName');
    _setEnginesExclude = _lib.lookupFunction<_sral_init_c, _sral_init_dart>('SRAL_SetEnginesExclude');
    _getEnginesExclude = _lib.lookupFunction<_sral_get_int_c, _sral_get_int_dart>('SRAL_GetEnginesExclude');
    _delayOutput = _lib.lookupFunction<_sral_delay_out_c, _sral_delay_out_dart>('SRAL_DelayOutput');
    _delayOutputEx = _lib.lookupFunction<_sral_delay_out_ex_c, _sral_delay_out_ex_dart>('SRAL_DelayOutputEx');
    _speakToMemory = _lib.lookupFunction<_sral_mem_c, _sral_mem_dart>('SRAL_SpeakToMemory');
    _speakToMemoryEx = _lib.lookupFunction<_sral_mem_ex_c, _sral_mem_ex_dart>('SRAL_SpeakToMemoryEx');
    _sralFree = _lib.lookupFunction<_sral_free_c, _sral_free_dart>('SRAL_free');
  }

  bool initialize(int enginesExclude) => _initialize(enginesExclude);

  void uninitialize() => _uninitialize();

  bool isInitialized() => _isInitialized();

  bool speak(String text, bool interrupt) {
    final cText = text.toNativeUtf8();
    try {
      return _speak(cText, interrupt);
    } finally {
      malloc.free(cText);
    }
  }

  bool speakSsml(String ssml, bool interrupt) {
    final cSsml = ssml.toNativeUtf8();
    try {
      return _speakSsml(cSsml, interrupt);
    } finally {
      malloc.free(cSsml);
    }
  }

  bool braille(String text) {
    final cText = text.toNativeUtf8();
    try {
      return _braille(cText);
    } finally {
      malloc.free(cText);
    }
  }

  bool output(String text, bool interrupt) {
    final cText = text.toNativeUtf8();
    try {
      return _output(cText, interrupt);
    } finally {
      malloc.free(cText);
    }
  }

  bool stopSpeech() => _stopSpeech();
  bool pauseSpeech() => _pauseSpeech();
  bool resumeSpeech() => _resumeSpeech();
  bool isSpeaking() => _isSpeaking();
  void delay(int timeMs) => _delay(timeMs);

  int getCurrentEngine() => _getCurrentEngine();
  int getEngineFeatures(int engine) => _getEngineFeatures(engine);
  int getAvailableEngines() => _getAvailableEngines();
  int getActiveEngines() => _getActiveEngines();
  int getEngineCategory(int engine) => _getEngineCategory(engine);

  String getEngineName(int engine) {
    final ptr = _getEngineName(engine);
    if (ptr == nullptr) return "Unknown Engine";
    return ptr.toDartString();
  }

  bool setEnginesExclude(int enginesExclude) => _setEnginesExclude(enginesExclude);
  int getEnginesExclude() => _getEnginesExclude();
  int getTTSEngines() => _getTTSEngines();
  int getAssistiveTechEngines() => _getAssistiveTechEngines();

  bool speakEx(int engine, String text, bool interrupt) {
    final cText = text.toNativeUtf8();
    try {
      return _speakEx(engine, cText, interrupt);
    } finally {
      malloc.free(cText);
    }
  }

  bool speakSsmlEx(int engine, String ssml, bool interrupt) {
    final cSsml = ssml.toNativeUtf8();
    try {
      return _speakSsmlEx(engine, cSsml, interrupt);
    } finally {
      malloc.free(cSsml);
    }
  }

  bool brailleEx(int engine, String text) {
    final cText = text.toNativeUtf8();
    try {
      return _brailleEx(engine, cText);
    } finally {
      malloc.free(cText);
    }
  }

  bool outputEx(int engine, String text, bool interrupt) {
    final cText = text.toNativeUtf8();
    try {
      return _outputEx(engine, cText, interrupt);
    } finally {
      malloc.free(cText);
    }
  }

  bool stopSpeechEx(int engine) => _stopSpeechEx(engine);
  bool pauseSpeechEx(int engine) => _pauseSpeechEx(engine);
  bool resumeSpeechEx(int engine) => _resumeSpeechEx(engine);
  bool isSpeakingEx(int engine) => _isSpeakingEx(engine);

  bool registerKeyboardHooks() => _registerKeyboardHooks();
  void unregisterKeyboardHooks() => _unregisterKeyboardHooks();

  bool setIntParameter(int engine, int param, int value) {
    final pValue = malloc<Int32>()..value = value;
    try {
      return _setEngineParameter(engine, param, pValue.cast<Void>());
    } finally {
      malloc.free(pValue);
    }
  }

  int getIntParameter(int engine, int param) {
    final pValue = malloc<Int32>()..value = -1;
    try {
      if (_getEngineParameter(engine, param, pValue.cast<Void>())) {
        return pValue.value;
      }
      return -1;
    } finally {
      malloc.free(pValue);
    }
  }

  List<SralVoiceInfo> getVoices(int engine) {
    final pCount = malloc<Int32>()..value = 0;
    try {
      // 4 corresponds directly to SRAL_PARAM_VOICE_COUNT
      if (!_getEngineParameter(engine, 4, pCount.cast<Void>()) || pCount.value <= 0) {
        return [];
      }

      final pVoicePtr = malloc<Pointer<NativeSralVoiceInfo>>();
      try {
        if (!_getEngineParameter(engine, 3, pVoicePtr.cast<Void>()) || pVoicePtr.value == nullptr) {
          return [];
        }

        final count = pCount.value;
        final Pointer<NativeSralVoiceInfo> rawArray = pVoicePtr.value;
        final List<SralVoiceInfo> voices = [];

        for (int i = 0; i < count; i++) {
          final structRef = rawArray[i];
          voices.add(SralVoiceInfo(
            index: structRef.index,
            name: structRef.name == nullptr ? "" : structRef.name.toDartString(),
            language: structRef.language == nullptr ? "" : structRef.language.toDartString(),
            gender: structRef.gender == nullptr ? "" : structRef.gender.toDartString(),
            vendor: structRef.vendor == nullptr ? "" : structRef.vendor.toDartString(),
          ));
        }

        // Release array buffer using your engine's custom memory deallocator contract
        _sralFree(rawArray.cast<Void>());
        return voices;
      } finally {
        malloc.free(pVoicePtr);
      }
    } finally {
      malloc.free(pCount);
    }
  }

  SralPCMData? speakToMemory(String text) {
    final cText = text.toNativeUtf8();
    final pSize = malloc<Uint64>()..value = 0;
    final pChan = malloc<Int32>()..value = 0;
    final pRate = malloc<Int32>()..value = 0;
    final pBits = malloc<Int32>()..value = 0;

    try {
      final ptr = _speakToMemory(cText, pSize, pChan, pRate, pBits);
      if (ptr == nullptr) return null;

      final size = pSize.value;
      final data = ptr.cast<Uint8>().asTypedList(size);
      final bufferCopy = Uint8List.fromList(data);

      _sralFree(ptr.cast<Void>());
      
      return SralPCMData(
        buffer: bufferCopy,
        channels: pChan.value,
        sampleRate: pRate.value,
        bitsPerSample: pBits.value,
      );
    } finally {
      malloc.free(cText);
      malloc.free(pSize);
      malloc.free(pChan);
      malloc.free(pRate);
      malloc.free(pBits);
    }
  }

  bool delayOutput(String text, int timeMs, bool interrupt, bool speak, bool braille, bool ssml) {
    final cText = text.toNativeUtf8();
    try {
      return _delayOutput(cText, timeMs, interrupt, speak, braille, ssml);
    } finally {
      malloc.free(cText);
    }
  }

  bool delayOutputEx(int engine, String text, int timeMs, bool interrupt, bool speak, bool braille, bool ssml) {
    final cText = text.toNativeUtf8();
    try {
      return _delayOutputEx(engine, cText, timeMs, interrupt, speak, braille, ssml);
    } finally {
      malloc.free(cText);
    }
  }
}
