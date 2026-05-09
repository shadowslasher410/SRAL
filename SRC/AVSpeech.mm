#include "AVSpeech.h"
#include <stdint.h>
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

class AVSpeechSynthesizerWrapper {
public:
	float rate;
	float volume;
	AVSpeechSynthesizer* synth;
	AVSpeechSynthesisVoice* currentVoice;
	AVSpeechUtterance* utterance;

	AVSpeechSynthesizerWrapper() : rate(AVSpeechUtteranceDefaultSpeechRate), volume(1), synth(nullptr), currentVoice(nullptr), utterance(nullptr) {}
AVSpeechSynthesisVoice* getVoiceObject(NSString* name){
 NSArray<AVSpeechSynthesisVoice*>* voices = [AVSpeechSynthesisVoice speechVoices];
 for (AVSpeechSynthesisVoice* v in voices) {
  if ([v.name isEqualToString : name]) return v;
 }
 return nil;
}

bool Initialize() {
	bool Initialize() {
    currentVoice = [AVSpeechSynthesisVoice voiceWithLanguage:@"en-US"];
    if (currentVoice == nil) {
        currentVoice = [AVSpeechSynthesisVoice voiceWithLanguage:[[NSLocale currentLocale] localeIdentifier]];
    }
    
    if (currentVoice == nil && [[AVSpeechSynthesisVoice speechVoices] count] > 0) {
        currentVoice = [[AVSpeechSynthesisVoice speechVoices] firstObject];
    }

    synth = [[AVSpeechSynthesizer alloc] init];
    AVSpeechUtterance *tempUtterance = [[AVSpeechUtterance alloc] initWithString:@""];
    rate = tempUtterance.rate;
    volume = tempUtterance.volume;
    
    return (synth != nil && currentVoice != nil);
}
}
bool Uninitialize() {
    if (synth != nil) {
        [synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
        
        #if !__has_feature(objc_arc)
        [synth release];
        [currentVoice release];
        [utterance release];
        #endif
        
        synth = nil;
        currentVoice = nil;
        utterance = nil;
    }
    return true;
}

bool Speak(const char* text, bool interrupt) {
    if (!synth || !text) return false;

    NSString *nstext = [NSString stringWithUTF8String:text];
    if (!nstext) return false;

    if (interrupt) {
        [synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    }

    AVSpeechUtterance *newUtterance = [[AVSpeechUtterance alloc] initWithString:nstext];
    newUtterance.rate = this->rate;
    newUtterance.volume = this->volume;
    newUtterance.voice = this->currentVoice;

#if !__has_feature(objc_arc)
    if (this->utterance) {
        [this->utterance release];
    }
    this->utterance = newUtterance;
#else
    this->utterance = newUtterance;
#endif

    [synth speakUtterance:newUtterance];
    
#if !__has_feature(objc_arc)
    // [newUtterance release];
#endif

    return true;
}

bool StopSpeech(){
 if (synth.isSpeaking) return [synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
 return false;
}

bool IsSpeaking() {
	return synth.isSpeaking;
}
bool GetActive(){
 return synth != nil;
}
void SetVolume(int value) {
    float vol = (float)value / 100.0f;
    if (vol > 1.0f) vol = 1.0f;
    if (vol < 0.0f) vol = 0.0f;
    this->volume = vol;
}

int GetVolume() {
    return (int)(this->volume * 100.0f + 0.5f);
}

void SetRate(int value) {
    float min = AVSpeechUtteranceMinimumSpeechRate;
    float max = AVSpeechUtteranceMaximumSpeechRate;

    this->rate = min + ((float)value / 100.0f) * (max - min);
}

int GetRate() {
    float min = AVSpeechUtteranceMinimumSpeechRate;
    float max = AVSpeechUtteranceMaximumSpeechRate;
    
    if (max == min) return 50;
    return (int)(((this->rate - min) / (max - min)) * 100.0f + 0.5f);
}

}
uint64_t GetVoiceCount(){
 NSArray<AVSpeechSynthesisVoice *> *voices = [AVSpeechSynthesisVoice speechVoices];
 return voices.count;
}
const char* GetVoiceName(uint64_t index){
 NSArray<AVSpeechSynthesisVoice *> *voices = [AVSpeechSynthesisVoice speechVoices];
 @try {
  return [[voices objectAtIndex:index].name UTF8String];
 } @catch (NSException *exception) {
  return "";
 }
}
bool SetVoice(uint64_t index){
 NSArray<AVSpeechSynthesisVoice *> *voices = [AVSpeechSynthesisVoice speechVoices];
 AVSpeechSynthesisVoice *oldVoice = currentVoice;
 @try {
  currentVoice = [voices objectAtIndex:index];
  return true;
 } @catch (NSException *exception) {
  currentVoice = oldVoice;
  return false;
 }
}
};
namespace Sral {

bool AvSpeech::Initialize() {
	obj = new AVSpeechSynthesizerWrapper();
	return obj->Initialize();
}

bool AvSpeech::Uninitialize() {
	ReleaseAllStrings();
	if (obj == nullptr) return false; // Check for nullptr
	delete obj;
	obj = nullptr; // Set to nullptr after deletion
	return true; // Return true to indicate successful uninitialization
}

bool AvSpeech::GetActive() {
	return obj != nullptr && obj->GetActive();
}

bool AvSpeech::Speak(const char* text, bool interrupt) {
	return obj ?  obj->Speak(text, interrupt) : false;
}

bool AvSpeech::StopSpeech() {
	return obj ? obj->StopSpeech() : false;
}
bool AvSpeech::IsSpeaking() {
	return obj ? obj->IsSpeaking() : false;
}

bool AvSpeech::SetParameter(int param, const void* value) {
	if (!obj || !value) return false;
	 int val = *reinterpret_cast<const int*>(value);
	switch (param) {
	case SRAL_PARAM_SPEECH_RATE:
		obj->SetRate(val);
		return true;
	case SRAL_PARAM_SPEECH_VOLUME:
		obj->SetVolume(val);
		return true;
	case SRAL_PARAM_VOICE_INDEX: {
            AVSpeechSynthesisVoice* v = obj->GetVoice(val);
            if (v) {
                #if !__has_feature(objc_arc)
                if (obj->currentVoice != v) {
                    [obj->currentVoice release];
                    obj->currentVoice = [v retain];
                }
                #else
                obj->currentVoice = v;
                #endif
                return true;
            }
            return false;
        }
	default:
		return false;
	}
	return true;
}

bool AvSpeech::GetParameter(int param, void* value) {
    if (!obj || !value) return false;

    switch (param) {
    case SRAL_PARAM_SPEECH_RATE:
        *(int*)value = obj->GetRate();
        return true;

    case SRAL_PARAM_SPEECH_VOLUME:
        *(int*)value = obj->GetVolume();
        return true;

    case SRAL_PARAM_VOICE_COUNT:
        *(int*)value = (int)obj->GetVoiceCount();
        return true;

    case SRAL_PARAM_VOICE_PROPERTIES: {
        ReleaseAllStrings();
        int voice_count = (int)obj->GetVoiceCount();
        SRAL_VoiceInfo* voices_info = static_cast<SRAL_VoiceInfo*>(value);
        
        NSArray<AVSpeechSynthesisVoice*>* voices = [AVSpeechSynthesisVoice speechVoices];
        
        for (int i = 0; i < voice_count; ++i) {
            AVSpeechSynthesisVoice* v = [voices objectAtIndex:i];
            
            voices_info[i].index = i;
            voices_info[i].name = AddString([v.name UTF8String]);
            voices_info[i].language = AddString([v.language UTF8String]);
            
            if (v.gender == AVSpeechSynthesisVoiceGenderMale)
                voices_info[i].gender = AddString("Male");
            else if (v.gender == AVSpeechSynthesisVoiceGenderFemale)
                voices_info[i].gender = AddString("Female");
            else
                voices_info[i].gender = AddString("Unknown");

            voices_info[i].vendor = AddString("Apple");
        }
        return true;
    }
    default:
        return false;
    }
}

}
