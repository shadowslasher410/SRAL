local ffi = require("ffi")

ffi.cdef[[
    typedef struct {
        int index;
        const char* name;
        const char* language;
        const char* gender;
        const char* vendor;
    } SRAL_VoiceInfo;

    void* SRAL_malloc(size_t size);
    void SRAL_free(void* memory);

    bool SRAL_Speak(const char* text, bool interrupt);
    bool SRAL_SpeakSsml(const char* ssml, bool interrupt);
    bool SRAL_Braille(const char* text);
    bool SRAL_Output(const char* text, bool interrupt);

    bool SRAL_StopSpeech(void);
    bool SRAL_PauseSpeech(void);
    bool SRAL_ResumeSpeech(void);
    bool SRAL_IsSpeaking(void);
    void SRAL_Delay(int time);

    int SRAL_GetCurrentEngine(void);
    int SRAL_GetEngineFeatures(int engine);
    bool SRAL_SetEngineParameter(int engine, int param, const void* value);
    bool SRAL_GetEngineParameter(int engine, int param, void* value);
    bool SRAL_Initialize(int engines_exclude);
    void SRAL_Uninitialize(void);

    bool SRAL_SpeakEx(int engine, const char* text, bool interrupt);
    void* SRAL_SpeakToMemory(const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample);
    void* SRAL_SpeakToMemoryEx(int engine, const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample);
    bool SRAL_SpeakSsmlEx(int engine, const char* ssml, bool interrupt);
    bool SRAL_BrailleEx(int engine, const char* text);
    bool SRAL_OutputEx(int engine, const char* text, bool interrupt);
    bool SRAL_StopSpeechEx(int engine);
    bool SRAL_PauseSpeechEx(int engine);
    bool SRAL_ResumeSpeechEx(int engine);
    bool SRAL_IsSpeakingEx(int engine);
    bool SRAL_IsInitialized(void);

    bool SRAL_RegisterKeyboardHooks(void);
    void SRAL_UnregisterKeyboardHooks(void);
    int SRAL_GetAvailableEngines(void);
    int SRAL_GetActiveEngines(void);
    int SRAL_GetTTSEngines(void);
    int SRAL_GetAssistiveTechEngines(void);
    int SRAL_GetEngineCategory(int engine);
    const char* SRAL_GetEngineName(int engine);
    bool SRAL_SetEnginesExclude(int engines_exclude);
    int SRAL_GetEnginesExclude(void);

    bool SRAL_DelayOutput(const char* text, int time, bool interrupt, bool speak, bool braille, bool ssml);
    bool SRAL_DelayOutputEx(int engine, const char* text, int time, bool interrupt, bool speak, bool braille, bool ssml);
]]

local sral = {
    Engines = {
        NONE                         = 0,
        NVDA                         = 0x02,   -- 1 << 1
        JAWS                         = 0x04,   -- 1 << 2
        ZDSR                         = 0x08,   -- 1 << 3
        NARRATOR                     = 0x10,   -- 1 << 4
        UIA                          = 0x20,   -- 1 << 5
        SAPI                         = 0x40,   -- 1 << 6
        SPEECH_DISPATCHER            = 0x80,   -- 1 << 7
        VOICE_OVER                   = 0x0100, -- 1 << 8
        NS_SPEECH                    = 0x0200, -- 1 << 9
        AV_SPEECH                    = 0x0400, -- 1 << 10
        ANDROID_ACCESSIBILITY_MGR    = 0x0800, -- 1 << 11
        ANDROID_TEXT_TO_SPEECH       = 0x1000, -- 1 << 12
        CHROMEVOX                    = 0x2000, -- 1 << 13
        ORCA                         = 0x4000, -- 1 << 14
        CURRENT                      = -1
    },

    EngineCategory = {
        UNKNOWN                = 0,
        SCREEN_READER          = 1,
        TEXT_TO_SPEECH_ENGINE  = 2,
        ACCESSIBILITY_PROVIDER = 3
    },

    SupportedFeatures = {
        SPEECH          = 0x02,   -- 1 << 1
        BRAILLE         = 0x04,   -- 1 << 2
        SPEECH_RATE     = 0x08,   -- 1 << 3
        SPEECH_VOLUME   = 0x10,   -- 1 << 4
        SELECT_VOICE    = 0x20,   -- 1 << 5
        PAUSE_SPEECH    = 0x40,   -- 1 << 6
        SSML            = 0x80,   -- 1 << 7
        SPEAK_TO_MEMORY = 0x0100, -- 1 << 8
        SPELLING        = 0x0200  -- 1 << 9
    },

    EngineParams = {
        SPEECH_RATE                = 0,
        SPEECH_VOLUME              = 1,
        VOICE_INDEX                = 2,
        VOICE_PROPERTIES           = 3,
        VOICE_COUNT                = 4,
        SYMBOL_LEVEL               = 5,
        SAPI_TRIM_THRESHOLD        = 6,
        ENABLE_SPELLING            = 7,
        USE_CHARACTER_DESCRIPTIONS = 8,
        NVDA_IS_CONTROL_EX         = 9,
        ANDROID_JNI_ENV            = 10,
        ANDROID_ACTIVITY           = 11
    }
}

local libsral = nil
local sral_dir = debug.getinfo(1).source:match("@?(.*[/\\])") or ""
local lib_subfolder = sral_dir .. "lib/"
if ffi.os == "Windows" then
    libsral = ffi.load("SRAL.dll")
elseif ffi.os == "OSX" then
    libsral = ffi.load("libsral.dylib")
elseif ffi.os == "iOS" then
    libsral = ffi.C
else
    libsral = ffi.load("libsral.so")
end


function sral.initialize(engines_exclude)
    return libsral.SRAL_Initialize(engines_exclude or 0)
end

function sral.uninitialize()
    libsral.SRAL_Uninitialize()
end

function sral.is_initialized()
    return libsral.SRAL_IsInitialized()
end

function sral.speak(text, interrupt)
    return libsral.SRAL_Speak(text, interrupt == true)
end

function sral.speak_ssml(ssml, interrupt)
    return libsral.SRAL_SpeakSsml(ssml, interrupt == true)
end

function sral.braille(text)
    return libsral.SRAL_Braille(text)
end

function sral.output(text, interrupt)
    return libsral.SRAL_Output(text, interrupt == true)
end

function sral.stop_speech() return libsral.SRAL_StopSpeech() end
function sral.pause_speech() return libsral.SRAL_PauseSpeech() end
function sral.resume_speech() return libsral.SRAL_ResumeSpeech() end
function sral.is_speaking() return libsral.SRAL_IsSpeaking() end
function sral.delay(ms) libsral.SRAL_Delay(ms) end
function sral.get_current_engine() return libsral.SRAL_GetCurrentEngine() end
function sral.get_engine_features(engine) return libsral.SRAL_GetEngineFeatures(engine or 0) end
function sral.get_available_engines() return libsral.SRAL_GetAvailableEngines() end
function sral.get_active_engines() return libsral.SRAL_GetActiveEngines() end
function sral.get_engines_exclude() return libsral.SRAL_GetEnginesExclude() end
function sral.set_engines_exclude(mask) return libsral.SRAL_SetEnginesExclude(mask) end
function sral.get_tts_engines() return libsral.SRAL_GetTTSEngines() end
function sral.get_assistive_tech_engines() return libsral.SRAL_GetAssistiveTechEngines() end
function sral.get_engine_category(engine) return libsral.SRAL_GetEngineCategory(engine) end

function sral.get_engine_name(engine)
    local ptr = libsral.SRAL_GetEngineName(engine)
    return ptr ~= nil and ffi.string(ptr) or "Unknown"
end

function sral.speak_ex(engine, text, interrupt) return libsral.SRAL_SpeakEx(engine, text, interrupt == true) end
function sral.speak_ssml_ex(engine, ssml, interrupt) return libsral.SRAL_SpeakSsmlEx(engine, ssml, interrupt == true) end
function sral.braille_ex(engine, text) return libsral.SRAL_BrailleEx(engine, text) end
function sral.output_ex(engine, text, interrupt) return libsral.SRAL_OutputEx(engine, text, interrupt == true) end
function sral.stop_speech_ex(engine) return libsral.SRAL_StopSpeechEx(engine) end
function sral.pause_speech_ex(engine) return libsral.SRAL_PauseSpeechEx(engine) end
function sral.resume_speech_ex(engine) return libsral.SRAL_ResumeSpeechEx(engine) end
function sral.is_speaking_ex(engine) return libsral.SRAL_IsSpeakingEx(engine) end

function sral.register_keyboard_hooks() return libsral.SRAL_RegisterKeyboardHooks() end
function sral.unregister_keyboard_hooks() libsral.SRAL_UnregisterKeyboardHooks() end

if ffi.os ~= "Linux" or not os.getenv("ANDROID_ROOT") then
    function sral.delay_output(text, time, interrupt, speak, braille, ssml)
        return libsral.SRAL_DelayOutput(text, time, interrupt == true, speak == true, braille == true, ssml == true)
    end
    function sral.delay_output_ex(engine, text, time, interrupt, speak, braille, ssml)
        return libsral.SRAL_DelayOutputEx(engine, text, time, interrupt == true, speak == true, braille == true, ssml == true)
    end
end

function sral.set_int_parameter(engine, param, value)
    local int_ptr = ffi.new("int", value)
    return libsral.SRAL_SetEngineParameter(engine, param, int_ptr)
end

function sral.get_int_parameter(engine, param)
    local int_ptr = ffi.new("int", -1)
    if libsral.SRAL_GetEngineParameter(engine, param, int_ptr) then
        return int_ptr[0]
    end
    return -1
end

local function parse_memory_stream(ptr, size_ptr, chan_ptr, rate_ptr, bits_ptr)
    if ptr == nil or ptr == ffi.cast("void*", 0) then return nil end
    return {
        buffer = ptr,
        size = tonumber(size_ptr[0]),
        channels = tonumber(chan_ptr[0]),
        sample_rate = tonumber(rate_ptr[0]),
        bits_per_sample = tonumber(bits_ptr[0])
    }
end

function sral.speak_to_memory(text)
    local size, chan, rate, bits = ffi.new("uint64_t[1]"), ffi.new("int[1]"), ffi.new("int[1]"), ffi.new("int[1]")
    local ptr = libsral.SRAL_SpeakToMemory(text, size, chan, rate, bits)
    return parse_memory_stream(ptr, size, chan, rate, bits)
end

function sral.speak_to_memory_ex(engine, text)
    local size, chan, rate, bits = ffi.new("uint64_t[1]"), ffi.new("int[1]"), ffi.new("int[1]"), ffi.new("int[1]")
    local ptr = libsral.SRAL_SpeakToMemoryEx(engine, text, size, chan, rate, bits)
    return parse_memory_stream(ptr, size, chan, rate, bits)
end

function sral.get_voices(engine)
    local count = sral.get_int_parameter(engine, sral.EngineParams.VOICE_COUNT)
    if count <= 0 then return {} end

    local voice_ptr_container = ffi.new("void*[1]")
    if not libsral.SRAL_GetEngineParameter(engine, sral.EngineParams.VOICE_PROPERTIES, voice_ptr_container) then
        return {}
    end

    local raw_array = ffi.cast("SRAL_VoiceInfo*", voice_ptr_container[0])
    if raw_array == nil then return {} end

    local voices = {}
    for i = 0, count - 1 do
        table.insert(voices, {
            index = raw_array[i].index,
            name = ffi.string(raw_array[i].name),
            language = ffi.string(raw_array[i].language),
            gender = ffi.string(raw_array[i].gender),
            vendor = ffi.string(raw_array[i].vendor)
        })
    end

    libsral.SRAL_free(voice_ptr_container[0])
    return voices
end

return sral
