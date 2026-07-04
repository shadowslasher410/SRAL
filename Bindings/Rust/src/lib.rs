use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_void};
use std::ptr;
use std::slice;

pub const SRAL_ENGINE_NONE: c_int = 0;
pub const SRAL_ENGINE_NVDA: c_int = 1 << 1;
pub const SRAL_ENGINE_JAWS: c_int = 1 << 2;
pub const SRAL_ENGINE_ZDSR: c_int = 1 << 3;
pub const SRAL_ENGINE_NARRATOR: c_int = 1 << 4;
pub const SRAL_ENGINE_UIA: c_int = 1 << 5;
pub const SRAL_ENGINE_SAPI: c_int = 1 << 6;
pub const SRAL_ENGINE_SPEECH_DISPATCHER: c_int = 1 << 7;
pub const SRAL_ENGINE_VOICE_OVER: c_int = 1 << 8;
pub const SRAL_ENGINE_NS_SPEECH: c_int = 1 << 9;
pub const SRAL_ENGINE_AV_SPEECH: c_int = 1 << 10;
pub const SRAL_ENGINE_ANDROID_ACCESSIBILITY_MANAGER: c_int = 1 << 11;
pub const SRAL_ENGINE_ANDROID_TEXT_TO_SPEECH: c_int = 1 << 12;
pub const SRAL_ENGINE_CHROMEVOX: c_int = 1 << 13;
pub const SRAL_ENGINE_ORCA: c_int = 1 << 14;
pub const SRAL_ENGINE_CURRENT: c_int = -1;
pub const SRAL_ENGINE_NO_SPECIFIED: c_int = -255;

pub const SRAL_FEATURE_NONE: c_int = 0;
pub const SRAL_FEATURE_SPEECH: c_int = 1 << 1;
pub const SRAL_FEATURE_BRAILLE: c_int = 1 << 2;
pub const SRAL_FEATURE_SPEECH_RATE: c_int = 1 << 3;
pub const SRAL_FEATURE_SPEECH_VOLUME: c_int = 1 << 4;
pub const SRAL_FEATURE_SELECT_VOICE: c_int = 1 << 5;
pub const SRAL_FEATURE_PAUSE_SPEECH: c_int = 1 << 6;
pub const SRAL_FEATURE_SSML: c_int = 1 << 7;
pub const SRAL_FEATURE_SPEAK_TO_MEMORY: c_int = 1 << 8;
pub const SRAL_FEATURE_SPELLING: c_int = 1 << 9;

pub const SRAL_CATEGORY_UNKNOWN: c_int = 0;
pub const SRAL_CATEGORY_SCREEN_READER: c_int = 1;
pub const SRAL_CATEGORY_TEXT_TO_SPEECH_ENGINE: c_int = 2;
pub const SRAL_CATEGORY_ACCESSIBILITY_PROVIDER: c_int = 3;

pub const SRAL_PARAM_SPEECH_RATE: c_int = 0;
pub const SRAL_PARAM_SPEECH_VOLUME: c_int = 1;
pub const SRAL_PARAM_VOICE_INDEX: c_int = 2;
pub const SRAL_PARAM_VOICE_PROPERTIES: c_int = 3;
pub const SRAL_PARAM_VOICE_COUNT: c_int = 4;
pub const SRAL_PARAM_SYMBOL_LEVEL: c_int = 5;
pub const SRAL_PARAM_SAPI_TRIM_THRESHOLD: c_int = 6;
pub const SRAL_PARAM_ENABLE_SPELLING: c_int = 7;
pub const SRAL_PARAM_USE_CHARACTER_DESCRIPTIONS: c_int = 8;
pub const SRAL_PARAM_NVDA_IS_CONTROL_EX: c_int = 9;

#[derive(Debug, Clone)]
pub struct VoiceInfo {
    pub index: i32,
    pub name: String,
    pub language: String,
    pub gender: String,
    pub vendor: String,
}

pub struct PCMData {
    pub buffer: Vec<u8>,
    pub channels: i32,
    pub sample_rate: i32,
    pub bits_per_sample: i32,
}

#[repr(C)]
struct NativeVoiceInfo {
    index: c_int,
    name: *const c_char,
    language: *const c_char,
    gender: *const c_char,
    vendor: *const c_char,
}

#[link(name = "SRAL")]
extern "C" {
    fn SRAL_Initialize(engines_exclude: c_int) -> bool;
    fn SRAL_Uninitialize();
    fn SRAL_IsInitialized() -> bool;
    fn SRAL_Speak(text: *const c_char, interrupt: bool) -> bool;
    fn SRAL_SpeakSsml(ssml: *const c_char, interrupt: bool) -> bool;
    fn SRAL_Braille(text: *const c_char) -> bool;
    fn SRAL_Output(text: *const c_char, interrupt: bool) -> bool;
    fn SRAL_StopSpeech() -> bool;
    fn SRAL_PauseSpeech() -> bool;
    fn SRAL_ResumeSpeech() -> bool;
    fn SRAL_IsSpeaking() -> bool;
    fn SRAL_Delay(time: c_int);
    fn SRAL_GetCurrentEngine() -> c_int;
    fn SRAL_GetEngineFeatures(engine: c_int) -> c_int;
    fn SRAL_SetEngineParameter(engine: c_int, param: c_int, value: *const c_void) -> bool;
    fn SRAL_GetEngineParameter(engine: c_int, param: c_int, value: *mut c_void) -> bool;
    fn SRAL_SpeakEx(engine: c_int, text: *const c_char, interrupt: bool) -> bool;
    fn SRAL_IsSpeakingEx(engine: c_int) -> bool;
    fn SRAL_GetAvailableEngines() -> c_int;
    fn SRAL_GetActiveEngines() -> c_int;
    fn SRAL_GetEnginesExclude() -> c_int;
    fn SRAL_SetEnginesExclude(engines_exclude: c_int) -> bool;
    fn SRAL_GetEngineName(engine: c_int) -> *const c_char;
    fn SRAL_RegisterKeyboardHooks() -> bool;
    fn SRAL_UnregisterKeyboardHooks();
    fn SRAL_GetEngineCategory(engine: c_int) -> c_int;
    fn SRAL_DelayOutput(text: *const c_char, time: c_int, interrupt: bool, speak: bool, braille: bool, ssml: bool) -> bool;
    fn SRAL_DelayOutputEx(engine: c_int, text: *const c_char, time: c_int, interrupt: bool, speak: bool, braille: bool, ssml: bool) -> bool;
    fn SRAL_SpeakToMemory(text: *const c_char, buffer_size: *mut u64, channels: *mut c_int, sample_rate: *mut c_int, bits_per_sample: *mut c_int) -> *mut c_void;
    fn SRAL_SpeakToMemoryEx(engine: c_int, text: *const c_char, buffer_size: *mut u64, channels: *mut c_int, sample_rate: *mut c_int, bits_per_sample: *mut c_int) -> *mut c_void;
    fn SRAL_free(memory: *mut c_void);
}

pub struct Sral;

impl Sral {
    pub fn initialize(engines_exclude: i32) -> bool { unsafe { SRAL_Initialize(engines_exclude) } }
    pub fn uninitialize() { unsafe { SRAL_Uninitialize() } }
    pub fn is_initialized() -> bool { unsafe { SRAL_IsInitialized() } }

    pub fn speak(text: &str, interrupt: bool) -> bool {
        let c_text = CString::new(text).unwrap();
        unsafe { SRAL_Speak(c_text.as_ptr(), interrupt) }
    }

    pub fn speak_ssml(ssml: &str, interrupt: bool) -> bool {
        let c_ssml = CString::new(ssml).unwrap();
        unsafe { SRAL_SpeakSsml(c_ssml.as_ptr(), interrupt) }
    }

    pub fn braille(text: &str) -> bool {
        let c_text = CString::new(text).unwrap();
        unsafe { SRAL_Braille(c_text.as_ptr()) }
    }

    pub fn output(text: &str, interrupt: bool) -> bool {
        let c_text = CString::new(text).unwrap();
        unsafe { SRAL_Output(c_text.as_ptr(), interrupt) }
    }

    pub fn stop_speech() -> bool { unsafe { SRAL_StopSpeech() } }
    pub fn pause_speech() -> bool { unsafe { SRAL_PauseSpeech() } }
    pub fn resume_speech() -> bool { unsafe { SRAL_ResumeSpeech() } }
    pub fn is_speaking() -> bool { unsafe { SRAL_IsSpeaking() } }
    pub fn delay(time_ms: i32) { unsafe { SRAL_Delay(time_ms) } }

    pub fn get_current_engine() -> i32 { unsafe { SRAL_GetCurrentEngine() } }
    pub fn get_engine_features(engine: i32) -> i32 { unsafe { SRAL_GetEngineFeatures(engine) } }
    pub fn get_available_engines() -> i32 { unsafe { SRAL_GetAvailableEngines() } }
    pub fn get_active_engines() -> i32 { unsafe { SRAL_GetActiveEngines() } }
    pub fn get_engines_exclude() -> i32 { unsafe { SRAL_GetEnginesExclude() } }
    pub fn set_engines_exclude(mask: i32) -> bool { unsafe { SRAL_SetEnginesExclude(mask) } }
    pub fn get_engine_category(engine: i32) -> i32 { unsafe { SRAL_GetEngineCategory(engine) } }

    pub fn get_engine_name(engine: i32) -> String {
        unsafe {
            let ptr = SRAL_GetEngineName(engine);
            if ptr.is_null() {
                "Unknown Engine".to_string()
            } else {
                CStr::from_ptr(ptr).to_string_lossy().into_owned()
            }
        }
    }

    pub fn speak_ex(engine: i32, text: &str, interrupt: bool) -> bool {
        let c_text = CString::new(text).unwrap();
        unsafe { SRAL_SpeakEx(engine, c_text.as_ptr(), interrupt) }
    }

    pub fn is_speaking_ex(engine: i32) -> bool { unsafe { SRAL_IsSpeakingEx(engine) } }
    pub fn register_keyboard_hooks() -> bool { unsafe { SRAL_RegisterKeyboardHooks() } }
    pub fn unregister_keyboard_hooks() { unsafe { SRAL_UnregisterKeyboardHooks() } }

    pub fn delay_output(text: &str, time_ms: i32, interrupt: bool, speak: bool, braille: bool, ssml: bool) -> bool {
        let c_text = CString::new(text).unwrap();
        unsafe { SRAL_DelayOutput(c_text.as_ptr(), time_ms, interrupt, speak, braille, ssml) }
    }

    pub fn delay_output_ex(engine: i32, text: &str, time_ms: i32, interrupt: bool, speak: bool, braille: bool, ssml: bool) -> bool {
        let c_text = CString::new(text).unwrap();
        unsafe { SRAL_DelayOutputEx(engine, c_text.as_ptr(), time_ms, interrupt, speak, braille, ssml) }
    }

    pub fn set_int_parameter(engine: i32, param: i32, value: i32) -> bool {
        unsafe { SRAL_SetEngineParameter(engine, param, &value as *const i32 as *const c_void) }
    }

    pub fn get_int_parameter(engine: i32, param: i32) -> i32 {
        let mut value: i32 = -1;
        unsafe {
            if SRAL_GetEngineParameter(engine, param, &mut value as *mut i32 as *mut c_void) {
                value
            } else {
                -1
            }
        }
    }

        pub fn get_voices(engine: i32) -> Vec<VoiceInfo> {
        let count = Self::get_int_parameter(engine, SRAL_PARAM_VOICE_COUNT);
        if count <= 0 { return Vec::new(); }

        let mut raw_ptr: *mut c_void = ptr::null_mut();
        unsafe {
            if !SRAL_GetEngineParameter(engine, SRAL_PARAM_VOICE_PROPERTIES, &mut raw_ptr as *mut *mut c_void as *mut c_void) || raw_ptr.is_null() {
                return Vec::new();
            }

            let native_slice = slice::from_raw_parts(raw_ptr as *const NativeVoiceInfo, count as usize);
            let mut voices = Vec::with_capacity(count as usize);

            for item in native_slice {
                voices.push(VoiceInfo {
                    index: item.index,
                    name: if item.name.is_null() { String::new() } else { CStr::from_ptr(item.name).to_string_lossy().into_owned() },
                    language: if item.language.is_null() { String::new() } else { CStr::from_ptr(item.language).to_string_lossy().into_owned() },
                    gender: if item.gender.is_null() { String::new() } else { CStr::from_ptr(item.gender).to_string_lossy().into_owned() },
                    vendor: if item.vendor.is_null() { String::new() } else { CStr::from_ptr(item.vendor).to_string_lossy().into_owned() },
                });
            }

            SRAL_free(raw_ptr); 
            voices
        }
    }

    pub fn speak_to_memory(text: &str) -> Option<PCMData> {
        let c_text = CString::new(text).unwrap();
        let mut size: u64 = 0;
        let (mut chan, mut rate, mut bits) = (0, 0, 0);

        unsafe {
            let ptr = SRAL_SpeakToMemory(c_text.as_ptr(), &mut size, &mut chan, &mut rate, &mut bits);
            if ptr.is_null() { return None; }

            let raw_slice = slice::from_raw_parts(ptr as *const u8, size as usize);
            let buffer = raw_slice.to_vec();

            SRAL_free(ptr);
            Some(PCMData { buffer, channels: chan, sample_rate: rate, bits_per_sample: bits })
        }
    }

    pub fn speak_to_memory_ex(engine: i32, text: &str) -> Option<PCMData> {
        let c_text = CString::new(text).unwrap();
        let mut size: u64 = 0;
        let (mut chan, mut rate, mut bits) = (0, 0, 0);

        unsafe {
            let ptr = SRAL_SpeakToMemoryEx(engine, c_text.as_ptr(), &mut size, &mut chan, &mut rate, &mut bits);
            if ptr.is_null() { return None; }

            let raw_slice = slice::from_raw_parts(ptr as *const u8, size as usize);
            let buffer = raw_slice.to_vec();

            SRAL_free(ptr);
            Some(PCMData { buffer, channels: chan, sample_rate: rate, bits_per_sample: bits })
        }
    }
}
