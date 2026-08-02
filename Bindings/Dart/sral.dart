import 'dart:ffi';
import 'dart:convert';
import 'dart:typed_data';
import 'package:ffi/ffi.dart';
import 'sral_types.dart';

class Sral {
  
  static bool _executeWithView(String input, bool Function(StringView) unmanagedCall) {
    if (input.isEmpty) return false;

    final List<int> units = utf8.encode(input);
    final Pointer<Uint8> pBuffer = calloc<Uint8>(units.length);
    final Pointer<StringView> pView = calloc<StringView>();
    
    try {
      pBuffer.asTypedList(units.length).setAll(0, units);
      pView.ref.data = pBuffer.cast<Utf8>();
      pView.ref.length = units.length;
      
      return unmanagedCall(pView.ref);
    } finally {
      calloc.free(pView);
      calloc.free(pBuffer);
    }
  }

  static SralPcmBuffer _executeWithMemoryBridge(String input, PcmBuffer Function(Pointer<Utf8>) bridgeCall) {
    final Pointer<Utf8> pText = input.toNativeUtf8();
    try {
      final nativeStruct = bridgeCall(pText);
      return SralPcmBuffer(nativeStruct);
    } finally {
      malloc.free(pText);
    }
  }

  static bool initialize({int enginesExclude = SralEngine.none}) => 
      SralNative.sralInitialize(enginesExclude);

  static void uninitialize() => 
      SralNative.sralUninitialize();

  static bool isInitialized() => 
      SralNative.sralIsInitialized();

  static bool speak(String text, {required bool interrupt}) => 
      _executeWithView(text, (v) => SralNative.safeSpeakAllocationBridge(v, interrupt));

  static bool speakSsml(String ssml, {required bool interrupt}) => 
      _executeWithView(ssml, (v) => SralNative.safeSpeakSsmlAllocationBridge(v, interrupt));

  static bool braille(String text) => 
      _executeWithView(text, (v) => SralNative.safeBrailleAllocationBridge(v));

  static bool output(String text, {required bool interrupt}) => 
      _executeWithView(text, (v) => SralNative.safeOutputAllocationBridge(v, interrupt));

  static bool stopSpeech() => 
      SralNative.sralStopSpeech();

  static bool pauseSpeech() => 
      SralNative.sralPauseSpeech();

  static bool resumeSpeech() => 
      SralNative.sralResumeSpeech();

  static bool isSpeaking() => 
      SralNative.sralIsSpeaking();

  static int getCurrentEngine() => 
      SralNative.sralGetCurrentEngine();

  static int getEngineFeatures(int engine) => 
      SralNative.sralGetEngineFeatures(engine);

  static bool speakEx(int engine, String text, {required bool interrupt}) => 
      _executeWithView(text, (v) => SralNative.safeSpeakExAllocationBridge(engine, v, interrupt));

  static bool speakSsmlEx(int engine, String ssml, {required bool interrupt}) => 
      _executeWithView(ssml, (v) => SralNative.safeSpeakSsmlExAllocationBridge(engine, v, interrupt));

  static bool brailleEx(int engine, String text) => 
      _executeWithView(text, (v) => SralNative.safeBrailleExAllocationBridge(engine, v));

  static bool outputEx(int engine, String text, {required bool interrupt}) => 
      _executeWithView(text, (v) => SralNative.safeOutputExAllocationBridge(engine, v, interrupt));

    static bool stopSpeechEx(int engine) => 
      SralNative.sralStopSpeechEx(engine) != 0;

  static bool pauseSpeechEx(int engine) => 
      SralNative.sralPauseSpeechEx(engine) != 0;

  static bool resumeSpeechEx(int engine) => 
      SralNative.sralResumeSpeechEx(engine) != 0;

  static bool isSpeakingEx(int engine) => 
      SralNative.sralIsSpeakingEx(engine) != 0;

  static void delay(int timeMs) => 
      SralNative.sralDelay(timeMs);

  static bool delayOutput(int timeMs, String text, {required bool interrupt}) => 
      _executeWithView(text, (v) => SralNative.safeDelayOutputAllocationBridge(timeMs, v, interrupt));

  static bool delayOutputEx(int engine, int timeMs, String text, {required bool interrupt}) => 
      _executeWithView(text, (v) => SralNative.safeDelayOutputExAllocationBridge(engine, timeMs, v, interrupt));

  static bool registerKeyboardHooks() => 
      SralNative.sralRegisterKeyboardHooks();

  static void unregisterKeyboardHooks() => 
      SralNative.sralUnregisterKeyboardHooks();

  static int getAvailableEngines() => 
      SralNative.sralGetAvailableEngines();

  static int getActiveEngines() => 
      SralNative.sralGetActiveEngines();

  static int getTTSEngines() => 
      SralNative.sralGetTTSEngines();

  static int getAssistiveTechEngines() => 
      SralNative.sralGetAssistiveTechEngines();

  static bool setEnginesExclude(int mask) => 
      SralNative.sralSetEnginesExclude(mask);

  static int? getEnginesExclude() {
    final result = SralNative.sralGetEnginesExclude();
    return result == -1 ? null : result;
  }

  static SralEngineCategory getEngineCategory(int engine) {
    final catIdx = SralNative.sralGetEngineCategory(engine);
    if (catIdx >= 0 && catIdx < SralEngineCategory.values.length) {
      return SralEngineCategory.values[catIdx];
    }
    return SralEngineCategory.unknown;
  }

  static String getEngineName(int engine) {
    final view = SralNative.getEngineNameFastBridge(engine);
    if (view.data == nullptr || view.length == 0) return '';
    
    return view.data.cast<Uint8>().asTypedList(view.length).map((c) => String.fromCharCode(c)).join();
  }

  static bool setEngineParameter<T extends NativeType>(int engine, int param, Pointer<T> value) =>
      SralNative.sralSetEngineParameter(engine, param, value.cast<Void>());

  static bool getEngineParameter<T extends NativeType>(int engine, int param, Pointer<T> outValue) =>
      SralNative.sralGetEngineParameter(engine, param, outValue.cast<Void>());

  static List<SralVoiceInfo> getEngineVoiceList(int engine) {
    final List<SralVoiceInfo> list = [];
    final Pointer<Int32> pCount = calloc<Int32>();
    final Pointer<Pointer<CSralVoiceInfo>> pArrayHandle = calloc<Pointer<CSralVoiceInfo>>();

    try {
      if (!SralNative.sralGetEngineParameter(engine, SralParam.voiceCount, pCount.cast()) || pCount.value <= 0) {
        return list;
      }

      if (SralNative.sralGetEngineParameter(engine, SralParam.voiceProperties, pArrayHandle.cast()) && pArrayHandle.value != nullptr) {
        final Pointer<CSralVoiceInfo> nativeArray = pArrayHandle.value;
        
        for (int i = 0; i < pCount.value; i++) {
          final element = (nativeArray + i).ref;
          
          list.add(SralVoiceInfo(
            index: element.index,
            name: element.name != nullptr ? element.name.toDartString() : '',
            language: element.language != nullptr ? element.language.toDartString() : '',
            gender: element.gender != nullptr ? element.gender.toDartString() : '',
            vendor: element.vendor != nullptr ? element.vendor.toDartString() : '',
          ));
        }
        
        SralNative.sralFree(nativeArray.cast());
      }
    } finally {
      calloc.free(pArrayHandle);
      calloc.free(pCount);
    }
    return list;
  }

  static SralPcmBuffer speakToMemory(String text) => 
      _executeWithMemoryBridge(text, (pText) => SralNative.directMemoryBridge(pText));

  static SralPcmBuffer speakToMemoryEx(int engine, String text) => 
      _executeWithMemoryBridge(text, (pText) => SralNative.directMemoryExBridge(engine, pText));

}

class SralPcmBuffer {
  final PcmBuffer _nativeStruct;
  bool _isDisposed = false;

  SralPcmBuffer(this._nativeStruct);

  bool get isEmpty => _nativeStruct.dataPointer == nullptr || _isDisposed;
  int get channels => _isDisposed ? 0 : _nativeStruct.channels;
  int get sampleRate => _isDisposed ? 0 : _nativeStruct.sampleRate;
  int get bitsPerSample => _isDisposed ? 0 : _nativeStruct.bitsPerSample;

  Uint8List get bytes {
    if (isEmpty) return Uint8List(0);
    return _nativeStruct.dataPointer.asTypedList(_nativeStruct.dataLength);
  }

  void dispose() {
    if (!_isDisposed) {
      if (_nativeStruct.dataPointer != nullptr) {
        SralNative.sralFree(_nativeStruct.dataPointer.cast<Void>());
      }
      _isDisposed = true;
    }
  }
}