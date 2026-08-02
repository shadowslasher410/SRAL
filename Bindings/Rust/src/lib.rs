use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_void};
use std::ptr;
use std::slice;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(transparent)]
pub struct SralEngines(pub u32);

impl SralEngines {
    pub const NONE: Self = SralEngines(0);
    pub const CURRENT: Self = SralEngines(0);
    pub const NO_SPECIFIED: Self = SralEngines(0);
    pub const NVDA: Self = SralEngines(1 << 1);
    pub const JAWS: Self = SralEngines(1 << 2);
    pub const ZDSR: Self = SralEngines(1 << 3);
    pub const NARRATOR: Self = SralEngines(1 << 4);
    pub const UIA: Self = SralEngines(1 << 5);
    pub const SAPI: Self = SralEngines(1 << 6);
    pub const SPEECH_DISPATCHER: Self = SralEngines(1 << 7);
    pub const ORCA: Self = SralEngines(1 << 8);
    pub const VOICE_OVER: Self = SralEngines(1 << 9);
    pub const NS_SPEECH: Self = SralEngines(1 << 10);
    pub const AV_SPEECH: Self = SralEngines(1 << 11);
    pub const ANDROID_ACCESSIBILITY_MANAGER: Self = SralEngines(1 << 12);
    pub const ANDROID_TEXT_TO_SPEECH: Self = SralEngines(1 << 13);
    pub const CHROME_VOX: Self = SralEngines(1 << 14);
    pub const ACCESS_KIT: Self = SralEngines(1 << 15);
}

impl std::ops::BitAn<SralEngines> for SralEngines {
    type Output = Self;
    fn bitand(self, rhs: Self) -> Self {
        SralEngines(self.0 & rhs.0)
    }
}
impl std::ops::BitOr<SralEngines> for SralEngines {
    type Output = Self;
    fn bitor(self, rhs: Self) -> Self {
        SralEngines(self.0 | rhs.0)
    }
}
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u32)]
pub enum SralEngineCategory {
    Unknown = 0,
    ScreenReader = 1,
    TextToSpeechEngine = 2,
    AccessibilityProvider = 3,
}

pub struct SralFeature;
impl SralFeature {
    pub const SPEECH: u32 = 1 << 1;
    pub const BRAILLE: u32 = 1 << 2;
    pub const SPEECH_RATE: u32 = 1 << 3;
    pub const SPEECH_VOLUME: u32 = 1 << 4;
    pub const SELECT_VOICE: u32 = 1 << 5;
    pub const PAUSE_SPEECH: u32 = 1 << 6;
    pub const SSML: u32 = 1 << 7;
    pub const SPEAK_TO_MEMORY: u32 = 1 << 8;
    pub const SPELLING: u32 = 1 << 9;
}

#[repr(u32)]
pub enum SralParam {
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
    EngineIsPaused = 10,
    AndroidJniEnv = 11,
    AndroidActivity = 12,
}

#[derive(Debug, Clone)]
pub struct SralVoiceInfo {
    pub index: i32,
    pub name: String,
    pub language: string,
    pub gender: String,
    pub vendor: String,
}

#[repr(C)]
struct CSralVoiceInfo {
    name: *const c_char,
    language: *const c_char,
    gender: *const c_char,
    vendor: *const c_char,
    index: i32,
}

#[repr(C)]
struct PcmBufferNative {
    data_pointer: *mut u8,
    data_length: u64,
    channels: i32,
    sample_rate: i32,
    bits_per_sample: i32,
}

#[repr(C)]
struct StringViewNative {
    data: *const c_char,
    length: usize,
}

extern "C" {
    fn SRAL_free(memory: *mut c_void);
    fn SRAL_Initialize(engines_exclude: i32) -> bool;
    fn SRAL_Uninitialize();
    fn SRAL_IsInitialized() -> bool;
    fn SRAL_StopSpeech() -> bool;
    fn SRAL_PauseSpeech() -> bool;
    fn SRAL_ResumeSpeech() -> bool;
    fn SRAL_IsSpeaking() -> bool;
    fn SRAL_GetCurrentEngine() -> i32;
    fn SRAL_GetEngineFeatures(engine: i32) -> i32;
    fn SRAL_SetEngineParameter(engine: i32, param: i32, value: *const c_void) -> bool;
    fn SRAL_GetEngineParameter(engine: i32, param: i32, value: *mut c_void) -> bool;
    fn SRAL_Delay(time_ms: i32);
    fn SRAL_GetAvailableEngines() -> i32;
    fn SRAL_GetActiveEngines() -> i32;
    fn SRAL_GetEngineCategory(engine: i32) -> i32;
    fn SRAL_GetTTSEngines() -> i32;
    fn SRAL_GetAssistiveTechEngines() -> i32;
    fn SRAL_SetEnginesExclude(mask: i32) -> bool;
    fn SRAL_GetEnginesExclude() -> i32;
    fn SRAL_StopSpeechEx(engine: i32) -> bool;
    fn SRAL_PauseSpeechEx(engine: i32) -> bool;
    fn SRAL_ResumeSpeechEx(engine: i32) -> bool;
    fn SRAL_IsSpeakingEx(engine: i32) -> bool;
    fn SRAL_RegisterKeyboardHooks() -> bool;
    fn SRAL_UnregisterKeyboardHooks();

    fn SafeSpeakAllocationBridge(text: StringViewNative, interrupt: bool) -> bool;
    fn SafeSpeakSsmlAllocationBridge(ssml: StringViewNative, interrupt: bool) -> bool;
    fn SafeBrailleAllocationBridge(text: StringViewNative) -> bool;
    fn SafeOutputAllocationBridge(text: StringViewNative, interrupt: bool) -> bool;
    fn SafeSpeakExAllocationBridge(engine: u32, text: StringViewNative, interrupt: bool) -> bool;
    fn SafeSpeakSsmlExAllocationBridge(
        engine: u32,
        ssml: StringViewNative,
        interrupt: bool,
    ) -> bool;
    fn SafeBrailleExAllocationBridge(engine: u32, text: StringViewNative) -> bool;
    fn SafeOutputExAllocationBridge(engine: u32, text: StringViewNative, interrupt: bool) -> bool;
    fn SafeDelayOutputAllocationBridge(
        time_ms: i32,
        text: StringViewNative,
        interrupt: bool,
    ) -> bool;
    fn SafeDelayOutputExAllocationBridge(
        engine: u32,
        time_ms: i32,
        text: StringViewNative,
        interrupt: bool,
    ) -> bool;
    fn DirectMemoryBridge(text: *const c_char) -> PcmBufferNative;
    fn DirectMemoryExBridge(engine: u32, text: *const c_char) -> PcmBufferNative;
    fn GetEngineNameFastBridge(engine: u32) -> StringViewNative;
}

pub struct SralPcmBuffer {
    raw: PcmBufferNative,
}

impl SralPcmBuffer {
    pub fn is_empty(&self) -> bool {
        self.raw.data_pointer.is_null()
    }
    pub fn channels(&self) -> i32 {
        self.raw.channels
    }
    pub fn sample_rate(&self) -> i32 {
        self.raw.sample_rate
    }
    pub fn bits_per_sample(&self) -> i32 {
        self.raw.bits_per_sample
    }

    pub fn data(&self) -> &[u8] {
        if self.is_empty() {
            return &[];
        }
        unsafe { slice::from_raw_parts(self.raw.data_pointer, self.raw.data_length as usize) }
    }
}

impl Drop for SralPcmBuffer {
    fn drop(&mut self) {
        if !self.raw.data_pointer.is_null() {
            unsafe {
                SRAL_free(self.raw.data_pointer as *mut c_void);
            }
        }
    }
}

pub struct Sral;

impl Sral {
    fn execute_with_view<F>(text: &str, f: F) -> bool
    where
        F: FnOnce(StringViewNative) -> bool,
    {
        if text.is_empty() {
            return false;
        }
        let view = StringViewNative {
            data: text.as_ptr() as *const c_char,
            length: text.len(),
        };
        f(view)
    }

    pub fn initialize(exclude: SralEngines) -> bool {
        unsafe { SRAL_Initialize(exclude.0 as i32) }
    }
    pub fn uninitialize() {
        unsafe {
            SRAL_Uninitialize();
        }
    }
    pub fn is_initialized() -> bool {
        unsafe { SRAL_IsInitialized() }
    }
    pub fn stop_speech() -> bool {
        unsafe { SRAL_StopSpeech() }
    }
    pub fn pause_speech() -> bool {
        unsafe { SRAL_PauseSpeech() }
    }
    pub fn resume_speech() -> bool {
        unsafe { SRAL_ResumeSpeech() }
    }
    pub fn is_speaking() -> bool {
        unsafe { SRAL_IsSpeaking() }
    }
    pub fn get_current_engine() -> SralEngines {
        unsafe { SralEngines(SRAL_GetCurrentEngine() as u32) }
    }
    pub fn get_engine_features(engine: SralEngines) -> u32 {
        unsafe { SRAL_GetEngineFeatures(engine.0 as i32) as u32 }
    }

    pub fn speak(text: &str, interrupt: bool) -> bool {
        Self::execute_with_view(text, |v| unsafe { SafeSpeakAllocationBridge(v, interrupt) })
    }
    pub fn speak_ssml(ssml: &str, interrupt: bool) -> bool {
        Self::execute_with_view(ssml, |v| unsafe {
            SafeSpeakSsmlAllocationBridge(v, interrupt)
        })
    }
    pub fn braille(text: &str) -> bool {
        Self::execute_with_view(text, |v| unsafe { SafeBrailleAllocationBridge(v) })
    }
    pub fn output(text: &str, interrupt: bool) -> bool {
        Self::execute_with_view(text, |v| unsafe {
            SafeOutputAllocationBridge(v, interrupt)
        })
    }

    pub fn speak_ex(engine: SralEngines, text: &str, interrupt: bool) -> bool {
        Self::execute_with_view(text, |v| unsafe {
            SafeSpeakExAllocationBridge(engine.0, v, interrupt)
        })
    }
    pub fn stop_speech_ex(engine: SralEngines) -> bool {
        unsafe { SRAL_StopSpeechEx(engine.0 as i32) }
    }
    pub fn delay(time_ms: i32) {
        unsafe {
            SRAL_Delay(time_ms);
        }
    }
    pub fn register_keyboard_hooks() -> bool {
        unsafe { SRAL_RegisterKeyboardHooks() }
    }
    pub fn unregister_keyboard_hooks() {
        unsafe {
            SRAL_UnregisterKeyboardHooks();
        }
    }
    pub fn get_available_engines() -> SralEngines {
        unsafe { SralEngines(SRAL_GetAvailableEngines() as u32) }
    }
    pub fn get_active_engines() -> SralEngines {
        unsafe { SralEngines(SRAL_GetActiveEngines() as u32) }
    }
    pub fn set_engines_exclude(mask: SralEngines) -> bool {
        unsafe { SRAL_SetEnginesExclude(mask.0 as i32) }
    }

    pub fn delay_output(time_ms: i32, text: &str, interrupt: bool) -> bool {
        Self::execute_with_view(text, |v| unsafe {
            SafeDelayOutputAllocationBridge(time_ms, v, interrupt)
        })
    }
    pub fn delay_output_ex(engine: SralEngines, time_ms: i32, text: &str, interrupt: bool) -> bool {
        Self::execute_with_view(text, |v| unsafe {
            SafeDelayOutputExAllocationBridge(engine.0, time_ms, v, interrupt)
        })
    }
    pub fn get_engines_exclude() -> Option {
        let res = unsafe { SRAL_GetEnginesExclude() };
        if res == -1 {
            None
        } else {
            Some(res as u32)
        }
    }
    pub fn get_engine_category(engine: SralEngines) -> SralEngineCategory {
        let cat = unsafe { SRAL_GetEngineCategory(engine.0 as i32) };
        match cat {
            1 => SralEngineCategory::ScreenReader,
            2 => SralEngineCategory::TextToSpeechEngine,
            3 => SralEngineCategory::AccessibilityProvider,
            _ => SralEngineCategory::Unknown,
        }
    }
    pub fn get_engine_name(engine: SralEngines) -> String {
        let view = unsafe { GetEngineNameFastBridge(engine.0) };
        if view.data.is_null() || view.length == 0 {
            return String::new();
        }
        unsafe {
            let bytes = slice::from_raw_parts(view.data as *const u8, view.length);
            String::from_utf8_lossy(bytes).into_owned()
        }
    }
    pub fn get_engine_voice_list(engine: SralEngines) -> Vec {
        let mut list = Vec::new();
        let mut count: i32 = 0;
        let success = unsafe {
            SRAL_GetEngineParameter(
                engine.0 as i32,
                SralParam::VoiceCount as i32,
                &mut count as *mut i32 as *mut c_void,
            )
        };
        if !success || count <= 0 {
            return list;
        }
        let mut raw_array_ptr: *mut c_void = ptr::null_mut();
        let success_ptr = unsafe {
            SRAL_GetEngineParameter(
                engine.0 as i32,
                SralParam::VoiceProperties as i32,
                &mut raw_array_ptr as *mut *mut c_void as *mut c_void,
            )
        };
        if success_ptr && !raw_array_ptr.is_null() {
            let array_head = raw_array_ptr as *const CSralVoiceInfo;
            for i in 0..count {
                unsafe {
                    let item = &*array_head.offset(i as isize);
                    let convert_str = |p: *const c_char| {
                        if p.is_null() {
                            String::new()
                        } else {
                            CStr::from_ptr(p).to_string_lossy().into_owned()
                        }
                    };
                    list.push(SralVoiceInfo {
                        index: item.index,
                        name: convert_str(item.name),
                        language: convert_str(item.language),
                        gender: convert_str(item.gender),
                        vendor: convert_str(item.vendor),
                    });
                }
            }
            unsafe {
                SRAL_free(raw_array_ptr);
            }
        }
        list
    }
    pub fn speak_to_memory(text: &str) -> SralPcmBuffer {
        let c_str = CString::new(text).unwrap_or_default();
        let res = unsafe { DirectMemoryBridge(c_str.as_ptr()) };
        SralPcmBuffer { raw: res }
    }
}
