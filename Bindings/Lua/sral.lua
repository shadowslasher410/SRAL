local ffi = require("ffi")
local bit = require("bit")

ffi.cdef[[
    void free(void* ptr);

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
]]

local libSRAL
local status, err = pcall(function()
    if ffi.os == "Windows" then
        return ffi.load("SRAL")
    elseif ffi.os == "OSX" then
        return ffi.load("libsral.dylib")
    else
        return ffi.load("libsral.so")
	end
end)

if status then
    libSRAL = err
else
    error("[SRAL ERROR] Failed to load native library driver. Verify path configuration variables: " .. tostring(err))
end

local sral = {}

sral.Engine = {
    NONE = 0,
    CURRENT = 0,
    NO_SPECIFIED = 0,
    NVDA = bit.lshift(1, 1),
    JAWS = bit.lshift(1, 2),
    ZDSR = bit.lshift(1, 3),
    NARRATOR = bit.lshift(1, 4),
    UIA = bit.lshift(1, 5),
    SAPI = bit.lshift(1, 6),
    SPEECH_DISPATCHER = bit.lshift(1, 7),
    ORCA = bit.lshift(1, 8),
    VOICE_OVER = bit.lshift(1, 9),
    NS_SPEECH = bit.lshift(1, 10),
    AV_SPEECH = bit.lshift(1, 11),
    ANDROID_ACCESSIBILITY_MANAGER = bit.lshift(1, 12),
    ANDROID_TEXT_TO_SPEECH = bit.lshift(1, 13),
    CHROME_VOX = bit.lshift(1, 14),
    ACCESS_KIT = bit.lshift(1, 15)
}

sral.EngineCategory = {
    UNKNOWN = 0,
    SCREEN_READER = 1,
    TEXT_TO_SPEECH_ENGINE = 2,
    ACCESSIBILITY_PROVIDER = 3
}

sral.Feature = {
    SPEECH = bit.lshift(1, 1),
    BRAILLE = bit.lshift(1, 2),
    SPEECH_RATE = bit.lshift(1, 3),
    SPEECH_VOLUME = bit.lshift(1, 4),
    SELECT_VOICE = bit.lshift(1, 5),
    PAUSE_SPEECH = bit.lshift(1, 6),
    SSML = bit.lshift(1, 7),
    SPEAK_TO_MEMORY = bit.lshift(1, 8),
    SPELLING = bit.lshift(1, 9)
}

sral.Param = {
    SPEECH_RATE = 0,
    SPEECH_VOLUME = 1,
    VOICE_INDEX = 2,
    VOICE_PROPERTIES = 3,
    VOICE_COUNT = 4,
    SYMBOL_LEVEL = 5,
    SAPI_TRIM_THRESHOLD = 6,
    ENABLE_SPELLING = 7,
    USE_CHARACTER_DESCRIPTIONS = 8,
    NVDA_IS_CONTROL_EX = 9,
    ENGINE_IS_PAUSED = 10,
    ANDROID_JNI_ENV = 11,
    ANDROID_ACTIVITY = 12
}

local function make_string_view(str)
    local sv = ffi.new("StringViewNative")
    sv.data = str
    sv.length = #str
    return sv
end

function sral.initialize(engines_exclude)
    return libSRAL.SRAL_Initialize(engines_exclude or sral.Engine.NONE)
end

function sral.uninitialize()
    libSRAL.SRAL_Uninitialize()
end

function sral.is_initialized()
    return libSRAL.SRAL_IsInitialized()
end

function sral.speak(text, interrupt)
    if not text or text == "" then return false end
    return libSRAL.SafeSpeakAllocationBridge(make_string_view(text), interrupt or false)
end

function sral.speak_ssml(ssml, interrupt)
    if not ssml or ssml == "" then return false end
    return libSRAL.SafeSpeakSsmlAllocationBridge(make_string_view(ssml), interrupt or false)
end

function sral.braille(text)
    if not text or text == "" then return false end
    return libSRAL.SafeBrailleAllocationBridge(make_string_view(text))
end

function sral.output(text, interrupt)
    if not text or text == "" then return false end
    return libSRAL.SafeOutputAllocationBridge(make_string_view(text), interrupt or false)
end

function sral.stop_speech() return libSRAL.SRAL_StopSpeech() end
function sral.pause_speech() return libSRAL.SRAL_PauseSpeech() end
function sral.resume_speech() return libSRAL.SRAL_ResumeSpeech() end
function sral.is_speaking() return libSRAL.SRAL_IsSpeaking() end
function sral.get_current_engine() return libSRAL.SRAL_GetCurrentEngine() end
function sral.get_engine_features(engine) return libSRAL.SRAL_GetEngineFeatures(engine or sral.Engine.NONE) end

function sral.speak_ex(engine, text, interrupt)
    if not text or text == "" then return false end
    return libSRAL.SafeSpeakExAllocationBridge(engine, make_string_view(text), interrupt or false)
end

function sral.speak_ssml_ex(engine, ssml, interrupt)
    if not ssml or ssml == "" then return false end
    return libSRAL.SafeSpeakSsmlExAllocationBridge(engine, make_string_view(ssml), interrupt or false)
end

function sral.braille_ex(engine, text)
    if not text or text == "" then return false end
    return libSRAL.SafeBrailleExAllocationBridge(engine, make_string_view(text))
end

function sral.output_ex(engine, text, interrupt)
    if not text or text == "" then return false end
    return libSRAL.SafeOutputExAllocationBridge(engine, make_string_view(text), interrupt or false)
end

function sral.stop_speech_ex(engine) return libSRAL.SRAL_StopSpeechEx(engine) end
function sral.pause_speech_ex(engine) return libSRAL.SRAL_PauseSpeechEx(engine) end
function sral.resume_speech_ex(engine) return libSRAL.SRAL_ResumeSpeechEx(engine) end
function sral.is_speaking_ex(engine) return libSRAL.SRAL_IsSpeakingEx(engine) end
function sral.delay(time_ms) libSRAL.SRAL_Delay(time_ms) end

function sral.delay_output(time_ms, text, interrupt)
    if not text or text == "" then return false end
    return libSRAL.SafeDelayOutputAllocationBridge(time_ms, make_string_view(text), interrupt or false)
end

function sral.delay_output_ex(engine, time_ms, text, interrupt)
    if not text or text == "" then return false end
    return libSRAL.SafeDelayOutputExAllocationBridge(engine, time_ms, make_string_view(text), interrupt or false)
end

function sral.register_keyboard_hooks() return libSRAL.SRAL_RegisterKeyboardHooks() end
function sral.unregister_keyboard_hooks() libSRAL.SRAL_UnregisterKeyboardHooks() end
function sral.get_available_engines() return libSRAL.SRAL_GetAvailableEngines() end
function sral.get_active_engines() return libSRAL.SRAL_GetActiveEngines() end
function sral.get_tts_engines() return libSRAL.SRAL_GetTTSEngines() end
function sral.get_assistive_tech_engines() return libSRAL.SRAL_GetAssistiveTechEngines() end
function sral.set_engines_exclude(mask) return libSRAL.SRAL_SetEnginesExclude(mask) end

function sral.get_engines_exclude()
    local res = libSRAL.SRAL_GetEnginesExclude()
    return res == -1 and nil or res
end

function sral.get_engine_category(engine)
    return libSRAL.SRAL_GetEngineCategory(engine)
end

function sral.get_engine_name(engine)
	local view = libSRAL.GetEngineNameFastBridge(engine)
	if view.data == nil or view.length == 0 then
		return ""
	end
	return ffi.string(view.data, view.length)
end

function sral.set_int_parameter(engine, param, value)
    local v = ffi.new("int[1]", value)
    return libSRAL.SRAL_SetEngineParameter(engine, param, v)
end

function sral.get_int_parameter(engine, param)
    local v = ffi.new("int[1]", -1)
    if libSRAL.SRAL_GetEngineParameter(engine, param, v) then
        return v[0]
    end
    return -1
end

function sral.get_engine_voice_list(engine)
    local voices_list = {}
    local count = ffi.new("int[1]", 0)
    
    if not libSRAL.SRAL_GetEngineParameter(engine, sral.Param.VOICE_COUNT, count) or count[0] <= 0 then
        return voices_list
    end

    local raw_array_ptr = ffi.new("void*[1]")
    if libSRAL.SRAL_GetEngineParameter(engine, sral.Param.VOICE_PROPERTIES, raw_array_ptr) and raw_array_ptr[0] ~= nil then
        
        local array_head = ffi.cast("CSralVoiceInfo*", raw_array_ptr[0])
        
        for i = 0, count[0] - 1 do
            local item = array_head[i]
            table.insert(voices_list, {
                index = item.index,
                name = item.name ~= nil and ffi.string(item.name) or "",
                language = item.language ~= nil and ffi.string(item.language) or "",
                gender = item.gender ~= nil and ffi.string(item.gender) or "",
                vendor = item.vendor ~= nil and ffi.string(item.vendor) or ""
            })
        end
        libSRAL.SRAL_free(raw_array_ptr[0])
    end
    return voices_list
end

function sral.speak_to_memory(text)
    if not text or text == "" then return nil end
    local res = libSRAL.DirectMemoryBridge(text)
    if res.data_pointer == nil then return nil end
    
    return setmetatable({
        channels = res.channels,
        sample_rate = res.sample_rate,
        bits_per_sample = res.bits_per_sample,
        length = tonumber(res.data_length),
        _ptr = res.data_pointer
    }, {
        __index = {
            free = function(self)
                if self._ptr then
                    libSRAL.SRAL_free(self._ptr)
                    self._ptr = nil
                end
            end,
            get_bytes = function(self)
                if not self._ptr then return "" end
                return ffi.string(self._ptr, self.length)
            end
        }
    })
end

function sral.speak_to_memory_ex(engine, text)
    if not text or text == "" then return nil end
    local res = libSRAL.DirectMemoryExBridge(engine, text)
    if res.data_pointer == nil then return nil end
    
    return setmetatable({
        channels = res.channels,
        sample_rate = res.sample_rate,
        bits_per_sample = res.bits_per_sample,
        length = tonumber(res.data_length),
        _ptr = res.data_pointer
    }, {
        __index = {
            free = function(self)
                if self._ptr then
                    libSRAL.SRAL_free(self._ptr)
                    self._ptr = nil
                end
            end,
            get_bytes = function(self)
                if not self._ptr then return "" end
                return ffi.string(self._ptr, self.length)
            end
        }
    })
end

return sral
