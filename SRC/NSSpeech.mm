#include "NsSpeech.h"
#include "Encoding.h"
#import <AppKit/AppKit.h>

class NSSpeechSynthesizerWrapper {
public:
    NSSpeechSynthesizer* synth;
    NSArray<NSVoiceIdentifier>* voices;
    float rate;
    float volume;

     NSSpeechSynthesizerWrapper() : synth(nullptr), voices(nil), rate(175.0f), volume(1.0f) {}

    bool Initialize() {
        synth = [[NSSpeechSynthesizer alloc] init];
        voices = [[NSSpeechSynthesizer availableVoices] retain];
        if (synth) {
            rate = [synth rate];
            volume = [synth volume];
        }
        return synth != nil;
    }

    void Uninitialize() {
        if (synth) {
            [synth stopSpeaking];
#if !__has_feature(objc_arc)
            [synth release];
#endif
            synth = nil;
        }
        if (voices) {
#if !__has_feature(objc_arc)
            [voices release];
#endif
            voices = nil;
        }
    }

    bool Speak(const char* text, bool interrupt) {
       if (!synth || !text) return false;
       if (interrupt) [synth stopSpeaking];
        
       NSString* nsStr = [NSString stringWithUTF8String:text];
       if (!nsStr) return false;

       return [synth startSpeakingString:nsStr] == YES;
    }

    bool Stop() {
        if (synth) [synth stopSpeaking];
        return true;
    }

    bool IsSpeaking() {
        return synth && [synth isSpeaking];
    }

    void SetVolume(int val) {
        this->volume = fmaxf(0.0f, fminf(1.0f, (float)val / 100.0f));
        if (synth) [synth setVolume:this->volume];
    }

    void SetRate(int val) {
        this->rate = (float)val;
        if (synth) [synth setRate:this->rate];
    }

    bool SetVoice(int index) {
        if (!synth || !voices || index < 0 || index >= (int)[voices count]) return false;
        return [synth setVoice:[voices objectAtIndex:index]];
    }

    int GetCurrentVoiceIndex() {
        if (!synth || !voices) return 0;
        NSVoiceIdentifier current = [synth voice];
        NSUInteger idx = [voices indexOfObject:current];
        return (idx == NSNotFound) ? 0 : (int)idx;
    }
};

namespace Sral {

	bool NsSpeech::Initialize() {
    obj = new NSSpeechSynthesizerWrapper();
    return obj->Initialize();
}

bool NsSpeech::Uninitialize() {
    if (obj) {
        obj->Uninitialize();
        delete obj;
        obj = nullptr;
    }
    return true;
}

bool NsSpeech::Speak(const char* text, bool interrupt) {
    return obj ? obj->Speak(text, interrupt) : false;
}

bool NsSpeech::StopSpeech() {
    return obj ? obj->Stop() : false;
}

bool NsSpeech::IsSpeaking() {
    return obj ? obj->IsSpeaking() : false;
}

bool NsSpeech::GetActive() {
    return obj != nullptr;
}

bool NsSpeech::SetParameter(int param, const void* value) {
    if (!obj || !value) return false;
    int val = *reinterpret_cast<const int*>(value);
    switch (param) {
        case SRAL_PARAM_SPEECH_RATE:   obj->SetRate(val); break;
        case SRAL_PARAM_SPEECH_VOLUME: obj->SetVolume(val); break;
        case SRAL_PARAM_VOICE_INDEX:   return obj->SetVoice(val);
        default: return false;
    }
    return true;
}

bool NsSpeech::GetParameter(int param, void* value) {
    if (!obj || !value) return false;
    switch (param) {
        case SRAL_PARAM_SPEECH_RATE:   
            *(int*)value = (int)obj->rate; 
            return true;
        case SRAL_PARAM_SPEECH_VOLUME: 
            *(int*)value = (int)(obj->volume * 100); 
            return true;
        case SRAL_PARAM_VOICE_INDEX:   
            *(int*)value = obj->GetCurrentVoiceIndex(); 
            return true;
        case SRAL_PARAM_VOICE_COUNT:
            *(int*)value = (int)[obj->voices count];
            return true;
        case SRAL_PARAM_VOICE_PROPERTIES: {
            SRAL_VoiceInfo* info = static_cast<SRAL_VoiceInfo*>(value);
            for (int i = 0; i < (int)[obj->voices count]; ++i) {
                NSVoiceIdentifier vId = [obj->voices objectAtIndex:i];
                NSDictionary* attr = [NSSpeechSynthesizer attributesForVoice:vId];
                
                info[i].index = i;
                info[i].name = AddString([[attr objectForKey:NSVoiceName] UTF8String]);
                info[i].language = AddString([[attr objectForKey:NSVoiceLocaleIdentifier] UTF8String]);
                info[i].gender = AddString([[attr objectForKey:NSVoiceGender] UTF8String]);
                info[i].vendor = AddString("Apple");
            }
            return true;
        }
        default: return false;
    }
}

} // namespace Sral
