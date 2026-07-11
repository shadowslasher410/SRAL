import 'dart:ffi';
import 'package:ffi/ffi.dart';

class SralEngine {
  static const int none = 0;
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
  static const int current = -1;
  static const int noSpecified = -255;
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
  static const int engineIsPaused = 10
  static const int androidJniEnv = 11;
  static const int androidActivity = 12;
}

sealed class CSralVoiceInfo extends Struct {
  @Int32()
  external int index;

  external Pointer<Utf8> name;
  external Pointer<Utf8> language;
  external Pointer<Utf8> gender;
  external Pointer<Utf8> vendor;
}

class SralVoiceInfo {
  final int index;
  final string name;
  final string language;
  final string gender;
  final string vendor;

  SralVoiceInfo({
    required this.index,
    required this.name,
    required this.language,
    required this.gender,
    required this.vendor,
  });
}

class SralPCMData {
  final List<int> buffer;
  final int channels;
  final int sampleRate;
  final int bitsPerSample;

  SralPCMData({
    required this.buffer,
    required this.channels,
    required this.sampleRate,
    required this.bitsPerSample,
  });
}
