import os
import sys
from ctypes import (
    CDLL, Structure, POINTER, c_bool, c_int, c_uint32, c_uint64, 
    c_char_p, c_void_p, c_size_t, c_uint8, create_string_buffer, cast
)

class SralEngine:
    none = 0
    current = 0
    noSpecified = 0
    nvda = 1 << 1
    jaws = 1 << 2
    zdsr = 1 << 3
    narrator = 1 << 4
    uia = 1 << 5
    sapi = 1 << 6
    speechDispatcher = 1 << 7
    orca = 1 << 8
    voiceOver = 1 << 9
    nsSpeech = 1 << 10
    avSpeech = 1 << 11
    androidAccessibilityManager = 1 << 12
    androidTextToSpeech = 1 << 13
    chromeVox = 1 << 14
    accessKit = 1 << 15

class SralEngineCategory:
    unknown = 0
    screenReader = 1
    textToSpeechEngine = 2
    accessibilityProvider = 3

class SralFeature:
    speech = 1 << 1
    braille = 1 << 2
    speechRate = 1 << 3
    speechVolume = 1 << 4
    selectVoice = 1 << 5
    pauseSpeech = 1 << 6
    ssml = 1 << 7
    speakToMemory = 1 << 8
    spelling = 1 << 9

class SralParam:
    speechRate = 0
    speechVolume = 1
    voiceIndex = 2
    voiceProperties = 3
    voiceCount = 4
    symbolLevel = 5
    sapiTrimThreshold = 6
    enableSpelling = 7
    useCharacterDescriptions = 8
    nvdaIsControlEx = 9
    engineIsPaused = 10
    androidJniEnv = 11
    androidActivity = 12

class CSralVoiceInfo(Structure):
    """Mirrors unmanaged SRAL_VoiceInfo layout precisely."""
    _fields_ = [
        ("name", c_char_p),
        ("language", c_char_p),
        ("gender", c_char_p),
        ("vendor", c_char_p),
        ("index", c_int)
    ]

class PcmBufferNative(Structure):
    """Mirrors unmanaged PCMBuffer footprint layout."""
    _fields_ = [
        ("data_pointer", POINTER(c_uint8)),
        ("data_length", c_uint64),
        ("channels", c_int),
        ("sample_rate", c_int),
        ("bits_per_sample", c_int)
    ]

class StringViewNative(Structure):
    """Mirrors unmanaged sral::allocationbridges::StringView layout."""
    _fields_ = [
        ("data", c_char_p),
        ("length", c_size_t)
    ]

class SralVoiceInfo:
    """Safe, copyable data class returned to python domains."""
    def __init__(self, index, name, language, gender, vendor):
        self.index = index
        self.name = name
        self.language = language
        self.gender = gender
        self.vendor = vendor

class SralPcmBuffer:
    """Exception-proof RAII context wrapper managing native PCM buffers."""
    def __init__(self, native_struct):
        self._struct = native_struct
        self._is_freed = False

    @property
    def is_empty(self):
        return not self._struct.data_pointer or self._is_freed

    @property
    def channels(self): return 0 if self.is_empty else self._struct.channels

    @property
    def sample_rate(self): return 0 if self.is_empty else self._struct.sample_rate

    @property
    def bits_per_sample(self): return 0 if self.is_empty else self._struct.bits_per_sample

    @property
    def data(self):
        """Returns a zero-copy memoryview overlay pointing to unmanaged memory."""
        if self.is_empty:
            return memoryview(b"")
        return memoryview((c_uint8 * self._struct.data_length).from_address(
            cast(self._struct.data_pointer, c_void_p).value
        ))

    def free(self):
        if not self._is_freed:
            if self._struct.data_pointer:
                _dll.SRAL_free(self._struct.data_pointer)
            self._is_freed = True

    def __enter__(self): return self
    def __exit__(self, exc_type, exc_val, exc_tb): self.free()

def _load_dll():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    if sys.platform == "win32":
        possible_paths = [
            os.path.join(script_dir, "SRAL.dll"),
            os.path.join(script_dir, "..", "..", "out", "build", "SRAL.dll"),
            os.path.join(script_dir, "..", "..", "build-cmake-clang", "SRAL.dll")
        ]
        for path in possible_paths:
            if os.path.exists(path):
                return CDLL(path)
        return CDLL("SRAL.dll")
        
    elif sys.platform == "darwin":
        return CDLL("libsral.dylib")
    else:
        return CDLL("libsral.so")

_dll = _load_dll()

_dll.SRAL_free.argtypes = [c_void_p]
_dll.SRAL_free.restype = None

_dll.SRAL_Initialize.argtypes = [c_int]
_dll.SRAL_Initialize.restype = c_bool

_dll.SRAL_Uninitialize.argtypes = []
_dll.SRAL_Uninitialize.restype = None

_dll.SRAL_IsInitialized.argtypes = []
_dll.SRAL_IsInitialized.restype = c_bool

_dll.SRAL_StopSpeech.argtypes = []
_dll.SRAL_StopSpeech.restype = c_bool

_dll.SRAL_PauseSpeech.argtypes = []
_dll.SRAL_PauseSpeech.restype = c_bool

_dll.SRAL_ResumeSpeech.argtypes = []
_dll.SRAL_ResumeSpeech.restype = c_bool

_dll.SRAL_IsSpeaking.argtypes = []
_dll.SRAL_IsSpeaking.restype = c_bool

_dll.SRAL_GetCurrentEngine.argtypes = []
_dll.SRAL_GetCurrentEngine.restype = c_int

_dll.SRAL_GetEngineFeatures.argtypes = [c_int]
_dll.SRAL_GetEngineFeatures.restype = c_int

_dll.SRAL_SetEngineParameter.argtypes = [c_int, c_int, c_void_p]
_dll.SRAL_SetEngineParameter.restype = c_bool

_dll.SRAL_GetEngineParameter.argtypes = [c_int, c_int, c_void_p]
_dll.SRAL_GetEngineParameter.restype = c_bool

_dll.SRAL_Delay.argtypes = [c_int]
_dll.SRAL_Delay.restype = None

_dll.SRAL_GetAvailableEngines.argtypes = []
_dll.SRAL_GetAvailableEngines.restype = c_int

_dll.SRAL_GetActiveEngines.argtypes = []
_dll.SRAL_GetActiveEngines.restype = c_int

_dll.SRAL_GetEngineCategory.argtypes = [c_int]
_dll.SRAL_GetEngineCategory.restype = c_int

_dll.SRAL_GetTTSEngines.argtypes = []
_dll.SRAL_GetTTSEngines.restype = c_int

_dll.SRAL_GetAssistiveTechEngines.argtypes = []
_dll.SRAL_GetAssistiveTechEngines.restype = c_int

_dll.SRAL_SetEnginesExclude.argtypes = [c_int]
_dll.SRAL_SetEnginesExclude.restype = c_bool

_dll.SRAL_GetEnginesExclude.argtypes = []
_dll.SRAL_GetEnginesExclude.restype = c_int

_dll.SRAL_StopSpeechEx.argtypes = [c_int]
_dll.SRAL_StopSpeechEx.restype = c_bool

_dll.SRAL_PauseSpeechEx.argtypes = [c_int]
_dll.SRAL_PauseSpeechEx.restype = c_bool

_dll.SRAL_ResumeSpeechEx.argtypes = [c_int]
_dll.SRAL_ResumeSpeechEx.restype = c_bool

_dll.SRAL_IsSpeakingEx.argtypes = [c_int]
_dll.SRAL_IsSpeakingEx.restype = c_bool

_dll.SRAL_RegisterKeyboardHooks.argtypes = []
_dll.SRAL_RegisterKeyboardHooks.restype = c_bool

_dll.SRAL_UnregisterKeyboardHooks.argtypes = []
_dll.SRAL_UnregisterKeyboardHooks.restype = None

_dll.SafeSpeakAllocationBridge.argtypes = [StringViewNative, c_bool]
_dll.SafeSpeakAllocationBridge.restype = c_bool

_dll.SafeSpeakSsmlAllocationBridge.argtypes = [StringViewNative, c_bool]
_dll.SafeSpeakSsmlAllocationBridge.restype = c_bool

_dll.SafeBrailleAllocationBridge.argtypes = [StringViewNative]
_dll.SafeBrailleAllocationBridge.restype = c_bool

_dll.SafeOutputAllocationBridge.argtypes = [StringViewNative, c_bool]
_dll.SafeOutputAllocationBridge.restype = c_bool

_dll.SafeSpeakExAllocationBridge.argtypes = [c_uint32, StringViewNative, c_bool]
_dll.SafeSpeakExAllocationBridge.restype = c_bool

_dll.SafeSpeakSsmlExAllocationBridge.argtypes = [c_uint32, StringViewNative, c_bool]
_dll.SafeSpeakSsmlExAllocationBridge.restype = c_bool

_dll.SafeBrailleExAllocationBridge.argtypes = [c_uint32, StringViewNative]
_dll.SafeBrailleExAllocationBridge.restype = c_bool

_dll.SafeOutputExAllocationBridge.argtypes = [c_uint32, StringViewNative, c_bool]
_dll.SafeOutputExAllocationBridge.restype = c_bool

_dll.SafeDelayOutputAllocationBridge.argtypes = [c_int, StringViewNative, c_bool]
_dll.SafeDelayOutputAllocationBridge.restype = c_bool

_dll.SafeDelayOutputExAllocationBridge.argtypes = [c_uint32, c_int, StringViewNative, c_bool]
_dll.SafeDelayOutputExAllocationBridge.restype = c_bool

_dll.DirectMemoryBridge.argtypes = [c_char_p]
_dll.DirectMemoryBridge.restype = PcmBufferNative

_dll.DirectMemoryExBridge.argtypes = [c_uint32, c_char_p]
_dll.DirectMemoryExBridge.restype = PcmBufferNative

_dll.GetEngineNameFastBridge.argtypes = [c_uint32]
_dll.GetEngineNameFastBridge.restype = StringViewNative

class Sral:
    @staticmethod
    def _execute_with_view(text: str, callback) -> bool:
        if not text:
            return False
        encoded_bytes = text.encode("utf-8")
        view = StringViewNative(encoded_bytes, len(encoded_bytes))
        return callback(view)

    @staticmethod
    def initialize(engines_exclude: int = SralEngine.none) -> bool:
        return _dll.SRAL_Initialize(engines_exclude)

    @staticmethod
    def uninitialize():
        _dll.SRAL_Uninitialize()

    @staticmethod
    def is_initialized() -> bool:
        return _dll.SRAL_IsInitialized()
    
    @staticmethod
    def speak(text: str, interrupt: bool = False) -> bool:
        return Sral._execute_with_view(text, lambda v: _dll.SafeSpeakAllocationBridge(v, interrupt))

    @staticmethod
    def speak_ssml(ssml: str, interrupt: bool = False) -> bool:
        return Sral._execute_with_view(ssml, lambda v: _dll.SafeSpeakSsmlAllocationBridge(v, interrupt))

    @staticmethod
    def braille(text: str) -> bool:
        return Sral._execute_with_view(text, lambda v: _dll.SafeBrailleAllocationBridge(v))

    @staticmethod
    def output(text: str, interrupt: bool = False) -> bool:
        return Sral._execute_with_view(text, lambda v: _dll.SafeOutputAllocationBridge(v, interrupt))

    @staticmethod
    def stop_speech() -> bool: 
        return _dll.SRAL_StopSpeech()

    @staticmethod
    def pause_speech() -> bool: 
        return _dll.SRAL_PauseSpeech()

    @staticmethod
    def resume_speech() -> bool: 
        return _dll.SRAL_ResumeSpeech()

    @staticmethod
    def is_speaking() -> bool: 
        return _dll.SRAL_IsSpeaking()

    @staticmethod
    def get_current_engine() -> int: 
        return _dll.SRAL_GetCurrentEngine()

    @staticmethod
    def get_engine_features(engine: int = SralEngine.none) -> int: 
        return _dll.SRAL_GetEngineFeatures(engine)

    @staticmethod
    def speak_ex(engine: int, text: str, interrupt: bool = False) -> bool:
        return Sral._execute_with_view(text, lambda v: _dll.SafeSpeakExAllocationBridge(engine, v, interrupt))

    @staticmethod
    def speak_ssml_ex(engine: int, ssml: str, interrupt: bool = False) -> bool:
        return Sral._execute_with_view(ssml, lambda v: _dll.SafeSpeakSsmlExAllocationBridge(engine, v, interrupt))

    @staticmethod
    def braille_ex(engine: int, text: str) -> bool:
        return Sral._execute_with_view(text, lambda v: _dll.SafeBrailleExAllocationBridge(engine, v))

    @staticmethod
    def output_ex(engine: int, text: str, interrupt: bool = False) -> bool:
        return Sral._execute_with_view(text, lambda v: _dll.SafeOutputExAllocationBridge(engine, v, interrupt))

    @staticmethod
    def stop_speech_ex(engine: int) -> bool: 
        return _dll.SRAL_StopSpeechEx(engine)

    @staticmethod
    def pause_speech_ex(engine: int) -> bool: 
        return _dll.SRAL_PauseSpeechEx(engine)

    @staticmethod
    def resume_speech_ex(engine: int) -> bool: 
        return _dll.SRAL_ResumeSpeechEx(engine)

    @staticmethod
    def is_speaking_ex(engine: int) -> bool: 
        return _dll.SRAL_IsSpeakingEx(engine)

    @staticmethod
    def delay(time_ms: int): 
        _dll.SRAL_Delay(time_ms)

    @staticmethod
    def delay_output(time_ms: int, text: str, interrupt: bool = False) -> bool:
        return Sral._execute_with_view(text, lambda v: _dll.SafeDelayOutputAllocationBridge(time_ms, v, interrupt))

    @staticmethod
    def delay_output_ex(engine: int, time_ms: int, text: str, interrupt: bool = False) -> bool:
        return Sral._execute_with_view(text, lambda v: _dll.SafeDelayOutputExAllocationBridge(engine, time_ms, v, interrupt))

    @staticmethod
    def register_keyboard_hooks() -> bool: 
        return _dll.SRAL_RegisterKeyboardHooks()

    @staticmethod
    def unregister_keyboard_hooks(): 
        _dll.SRAL_UnregisterKeyboardHooks()

    @staticmethod
    def get_available_engines() -> int: 
        return _dll.SRAL_GetAvailableEngines()

    @staticmethod
    def get_active_engines() -> int: 
        return _dll.SRAL_GetActiveEngines()

    @staticmethod
    def get_tts_engines() -> int: 
        return _dll.SRAL_GetTTSEngines()

    @staticmethod
    def get_assistive_tech_engines() -> int: 
        return _dll.SRAL_GetAssistiveTechEngines()

    @staticmethod
    def set_engines_exclude(mask: int) -> bool: 
        return _dll.SRAL_SetEnginesExclude(mask)

    @staticmethod
    def get_engines_exclude() -> int | None:
        res = _dll.SRAL_GetEnginesExclude()
        return None if res == -1 else res

    @staticmethod
    def get_engine_category(engine: int) -> int:
        return _dll.SRAL_GetEngineCategory(engine)