//go:build !android && !ios && cgo
// +build !android,!ios,cgo

package main

/*
#cgo CFLAGS: -I${SRCDIR}/../../Include
#cgo darwin LDFLAGS: -L${SRCDIR}/lib -lSRAL -framework AppKit -framework Foundation -framework AVFoundation -lc++
#cgo linux pkg-config: speech-dispatcher
#cgo linux LDFLAGS: -L${SRCDIR}/lib -lSRAL -lbrlapi -lstdc++
#cgo windows CFLAGS: -DSRAL_STATIC
#cgo windows LDFLAGS: -L${SRCDIR}/lib -lSRAL -Wl,--start-group -luiautomationcore -lole32 -loleaut32 -luuid -luser32 -lkernel32 -lgdi32 -ladvapi32 -lshell32 -lstdc++ -Wl,--end-group -static

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
	const char* name;
	const char* language;
	const char* gender;
	const char* vendor;
	int index;
} CSralVoiceInfo;

typedef struct {
	uint8_t* data_pointer;
	uint64_t data_length;
	int channels;
	int sample_rate;
	int bits_per_sample;
} PcmBufferNative;

typedef struct {
	const char* data;
	size_t length;
} StringViewNative;

void SRAL_free(void* memory);
bool SRAL_Initialize(int engines_exclude);
void SRAL_Uninitialize(void);
bool SRAL_IsInitialized(void);
bool SRAL_StopSpeech(void);
bool SRAL_PauseSpeech(void);
bool SRAL_ResumeSpeech(void);
bool SRAL_IsSpeaking(void);
int SRAL_GetCurrentEngine(void);
int SRAL_GetEngineFeatures(int engine);
bool SRAL_SetEngineParameter(int engine, int param, const void* value);
bool SRAL_GetEngineParameter(int engine, int param, void* value);
void SRAL_Delay(int time);
int SRAL_GetAvailableEngines(void);
int SRAL_GetActiveEngines(void);
int SRAL_GetEngineCategory(int engine);
int SRAL_GetTTSEngines(void);
int SRAL_GetAssistiveTechEngines(void);
bool SRAL_SetEnginesExclude(int engines_exclude);
int SRAL_GetEnginesExclude(void);
bool SRAL_StopSpeechEx(int engine);
bool SRAL_PauseSpeechEx(int engine);
bool SRAL_ResumeSpeechEx(int engine);
bool SRAL_IsSpeakingEx(int engine);
bool SRAL_RegisterKeyboardHooks(void);
void SRAL_UnregisterKeyboardHooks(void);
bool SafeSpeakAllocationBridge(StringViewNative text, bool interrupt);
bool SafeSpeakSsmlAllocationBridge(StringViewNative ssml, bool interrupt);
bool SafeBrailleAllocationBridge(StringViewNative text);
bool SafeOutputAllocationBridge(StringViewNative text, bool interrupt);
bool SafeSpeakExAllocationBridge(uint32_t engine, StringViewNative text, bool interrupt);
bool SafeSpeakSsmlExAllocationBridge(uint32_t engine, StringViewNative ssml, bool interrupt);
bool SafeBrailleExAllocationBridge(uint32_t engine, StringViewNative text);
bool SafeOutputExAllocationBridge(uint32_t engine, StringViewNative text, bool interrupt);
bool SafeDelayOutputAllocationBridge(int time, StringViewNative text, bool interrupt);
bool SafeDelayOutputExAllocationBridge(uint32_t engine, int time, StringViewNative text, bool interrupt);
PcmBufferNative DirectMemoryBridge(const char* text);
PcmBufferNative DirectMemoryExBridge(uint32_t engine, const char* text);
StringViewNative GetEngineNameFastBridge(uint32_t engine);
*/
import "C"
import (
	"unsafe"
)

type SralEngines uint32

const (
	EngineNone                        SralEngines = 0
	EngineCurrent                     SralEngines = 0
	EngineNoSpecified                 SralEngines = 0
	EngineNvda                        SralEngines = 1 << 1
	EngineJaws                        SralEngines = 1 << 2
	EngineZdsr                        SralEngines = 1 << 3
	EngineNarrator                    SralEngines = 1 << 4
	EngineUia                         SralEngines = 1 << 5
	EngineSapi                        SralEngines = 1 << 6
	EngineSpeechDispatcher            SralEngines = 1 << 7
	EngineOrca                        SralEngines = 1 << 8
	EngineVoiceOver                   SralEngines = 1 << 9
	EngineNsSpeech                    SralEngines = 1 << 10
	EngineAvSpeech                    SralEngines = 1 << 11
	EngineAndroidAccessibilityManager SralEngines = 1 << 12
	EngineAndroidTextToSpeech         SralEngines = 1 << 13
	EngineChromeVox                   SralEngines = 1 << 14
	EngineAccessKit                   SralEngines = 1 << 15
)

type SralEngineCategory uint32

const (
	CategoryUnknown               SralEngineCategory = 0
	CategoryScreenReader          SralEngineCategory = 1
	CategoryTextToSpeechEngine    SralEngineCategory = 2
	CategoryAccessibilityProvider SralEngineCategory = 3
)

type SralFeature uint32

const (
	FeatureSpeech        SralFeature = 1 << 1
	FeatureBraille       SralFeature = 1 << 2
	FeatureSpeechRate    SralFeature = 1 << 3
	FeatureSpeechVolume  SralFeature = 1 << 4
	FeatureSelectVoice   SralFeature = 1 << 5
	FeaturePauseSpeech   SralFeature = 1 << 6
	FeatureSsml          SralFeature = 1 << 7
	FeatureSpeakToMemory SralFeature = 1 << 8
	FeatureSpelling      SralFeature = 1 << 9
)

type SralParam uint32

const (
	ParamSpeechRate               SralParam = 0
	ParamSpeechVolume             SralParam = 1
	ParamVoiceIndex               SralParam = 2
	ParamVoiceProperties          SralParam = 3
	ParamVoiceCount               SralParam = 4
	ParamSymbolLevel              SralParam = 5
	ParamSapiTrimThreshold        SralParam = 6
	ParamEnableSpelling           SralParam = 7
	ParamUseCharacterDescriptions SralParam = 8
	ParamNvdaIsControlEx          SralParam = 9
	ParamEngineIsPaused           SralParam = 10
	ParamAndroidJniEnv            SralParam = 11
	ParamAndroidActivity          SralParam = 12
)

type SralVoiceInfo struct {
	Index    int
	Name     string
	Language string
	Gender   string
	Vendor   string
}

type SralPcmBuffer struct {
	nativePtr     unsafe.Pointer
	Data          []byte
	Channels      int
	SampleRate    int
	BitsPerSample int
}

func (b *SralPcmBuffer) IsEmpty() bool { return b.nativePtr == nil }
func (b *SralPcmBuffer) Free() {
	if b.nativePtr != nil {
		C.SRAL_free(b.nativePtr)
		b.nativePtr = nil
		b.Data = nil
	}
}

func executeWithView(input string, callback func(C.StringViewNative) C.bool) bool {
	if len(input) == 0 {
		return false
	}
	pStr := C.CString(input)
	defer C.free(unsafe.Pointer(pStr))

	view := C.StringViewNative{
		data:   pStr,
		length: C.size_t(len(input)),
	}
	return bool(callback(view))
}

func Free(memory unsafe.Pointer)          { C.SRAL_free(memory) }
func Initialize(exclude SralEngines) bool { return bool(C.SRAL_Initialize(C.int(exclude))) }
func Uninitialize()                       { C.SRAL_Uninitialize() }
func IsInitialized() bool                 { return bool(C.SRAL_IsInitialized()) }
func StopSpeech() bool                    { return bool(C.SRAL_StopSpeech()) }
func PauseSpeech() bool                   { return bool(C.SRAL_PauseSpeech()) }
func ResumeSpeech() bool                  { return bool(C.SRAL_ResumeSpeech()) }
func IsSpeaking() bool                    { return bool(C.SRAL_IsSpeaking()) }
func GetCurrentEngine() SralEngines       { return SralEngines(C.SRAL_GetCurrentEngine()) }
func GetEngineFeatures(engine SralEngines) uint32 {
	return uint32(C.SRAL_GetEngineFeatures(C.int(engine)))
}

func Speak(text string, interrupt bool) bool {
	return executeWithView(text, func(v C.StringViewNative) C.bool { return C.SafeSpeakAllocationBridge(v, C.bool(interrupt)) })
}
func SpeakSsml(ssml string, interrupt bool) bool {
	return executeWithView(ssml, func(v C.StringViewNative) C.bool { return C.SafeSpeakSsmlAllocationBridge(v, C.bool(interrupt)) })
}
func Braille(text string) bool {
	return executeWithView(text, func(v C.StringViewNative) C.bool { return C.SafeBrailleAllocationBridge(v) })
}
func Output(text string, interrupt bool) bool {
	return executeWithView(text, func(v C.StringViewNative) C.bool { return C.SafeOutputAllocationBridge(v, C.bool(interrupt)) })
}

func SpeakEx(engine SralEngines, text string, interrupt bool) bool {
	return executeWithView(text, func(v C.StringViewNative) C.bool {
		return C.SafeSpeakExAllocationBridge(C.uint32_t(engine), v, C.bool(interrupt))
	})
}
func SpeakSsmlEx(engine SralEngines, ssml string, interrupt bool) bool {
	return executeWithView(ssml, func(v C.StringViewNative) C.bool {
		return C.SafeSpeakSsmlExAllocationBridge(C.uint32_t(engine), v, C.bool(interrupt))
	})
}
func BrailleEx(engine SralEngines, text string) bool {
	return executeWithView(text, func(v C.StringViewNative) C.bool { return C.SafeBrailleExAllocationBridge(C.uint32_t(engine), v) })
}
func OutputEx(engine SralEngines, text string, interrupt bool) bool {
	return executeWithView(text, func(v C.StringViewNative) C.bool {
		return C.SafeOutputExAllocationBridge(C.uint32_t(engine), v, C.bool(interrupt))
	})
}

func StopSpeechEx(engine SralEngines) bool   { return bool(C.SRAL_StopSpeechEx(C.int(engine))) }
func PauseSpeechEx(engine SralEngines) bool  { return bool(C.SRAL_PauseSpeechEx(C.int(engine))) }
func ResumeSpeechEx(engine SralEngines) bool { return bool(C.SRAL_ResumeSpeechEx(C.int(engine))) }
func IsSpeakingEx(engine SralEngines) bool   { return bool(C.SRAL_IsSpeakingEx(C.int(engine))) }
func Delay(timeMs int)                       { C.SRAL_Delay(C.int(timeMs)) }
func RegisterKeyboardHooks() bool            { return bool(C.SRAL_RegisterKeyboardHooks()) }
func UnregisterKeyboardHooks()               { C.SRAL_UnregisterKeyboardHooks() }

func DelayOutput(timeMs int, text string, interrupt bool) bool {
	return executeWithView(text, func(v C.StringViewNative) C.bool {
		return C.SafeDelayOutputAllocationBridge(C.int(timeMs), v, C.bool(interrupt))
	})
}
func DelayOutputEx(engine SralEngines, timeMs int, text string, interrupt bool) bool {
	return executeWithView(text, func(v C.StringViewNative) C.bool {
		return C.SafeDelayOutputExAllocationBridge(C.uint32_t(engine), C.int(timeMs), v, C.bool(interrupt))
	})
}

func GetAvailableEngines() SralEngines        { return SralEngines(C.SRAL_GetAvailableEngines()) }
func GetActiveEngines() SralEngines           { return SralEngines(C.SRAL_GetActiveEngines()) }
func GetTTSEngines() SralEngines              { return SralEngines(C.SRAL_GetTTSEngines()) }
func GetAssistiveTechEngines() SralEngines    { return SralEngines(C.SRAL_GetAssistiveTechEngines()) }
func SetEnginesExclude(mask SralEngines) bool { return bool(C.SRAL_SetEnginesExclude(C.int(mask))) }
func GetEnginesExclude() (uint32, bool) {
	res := int(C.SRAL_GetEnginesExclude())
	if res == -1 {
		return 0, false
	}
	return uint32(res), true
}
func GetEngineCategory(engine SralEngines) SralEngineCategory {
	return SralEngineCategory(C.SRAL_GetEngineCategory(C.int(engine)))
}
func GetEngineName(engine SralEngines) string {
	view := C.GetEngineNameFastBridge(C.uint32_t(engine))
	if view.data == nil || view.length == 0 {
		return ""
	}
	return C.GoStringN(view.data, C.int(view.length))
}
func SetEngineParameter(engine SralEngines, param SralParam, value unsafe.Pointer) bool {
	return bool(C.SRAL_SetEngineParameter(C.int(engine), C.int(param), value))
}
func GetEngineParameter(engine SralEngines, param SralParam, outValue unsafe.Pointer) bool {
	return bool(C.SRAL_GetEngineParameter(C.int(engine), C.int(param), outValue))
}
func GetEngineVoiceList(engine SralEngines) []SralVoiceInfo {
	list := []SralVoiceInfo{}
	var count C.int
	if !bool(C.SRAL_GetEngineParameter(C.int(engine), C.int(ParamVoiceCount), unsafe.Pointer(&count))) || count <= 0 {
		return list
	}
	var rawArrayPtr unsafe.Pointer
	if bool(C.SRAL_GetEngineParameter(C.int(engine), C.int(ParamVoiceProperties), unsafe.Pointer(&rawArrayPtr))) && rawArrayPtr != nil {
		stride := unsafe.Sizeof(C.CSralVoiceInfo{})
		for i := 0; i < int(count); i++ {
			element := (*C.CSralVoiceInfo)(unsafe.Pointer(uintptr(rawArrayPtr) + uintptr(i)*stride))
			list = append(list, SralVoiceInfo{Index: int(element.index), Name: C.GoString(element.name), Language: C.GoString(element.language), Gender: C.GoString(element.gender), Vendor: C.GoString(element.vendor)})
		}
		C.SRAL_free(rawArrayPtr)
	}
	return list
}
func SpeakToMemory(text string) SralPcmBuffer {
	if len(text) == 0 {
		return SralPcmBuffer{}
	}
	pText := C.CString(text)
	defer C.free(unsafe.Pointer(pText))
	res := C.DirectMemoryBridge(pText)
	if res.data_pointer == nil {
		return SralPcmBuffer{}
	}
	return SralPcmBuffer{nativePtr: unsafe.Pointer(res.data_pointer), Data: unsafe.Slice((*byte)(unsafe.Pointer(res.data_pointer)), int(res.data_length)), Channels: int(res.channels), SampleRate: int(res.sample_rate), BitsPerSample: int(res.bits_per_sample)}
}
func SpeakToMemoryEx(engine SralEngines, text string) SralPcmBuffer {
	if len(text) == 0 {
		return SralPcmBuffer{}
	}
	pText := C.CString(text)
	defer C.free(unsafe.Pointer(pText))
	res := C.DirectMemoryExBridge(C.uint32_t(engine), pText)
	if res.data_pointer == nil {
		return SralPcmBuffer{}
	}
	return SralPcmBuffer{nativePtr: unsafe.Pointer(res.data_pointer), Data: unsafe.Slice((*byte)(unsafe.Pointer(res.data_pointer)), int(res.data_length)), Channels: int(res.channels), SampleRate: int(res.sample_rate), BitsPerSample: int(res.bits_per_sample)}
}
