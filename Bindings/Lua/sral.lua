local ffi = require("ffi")

local lib_name = ffi.os == "Windows" and "SRAL.dll" or (ffi.os == "OSX" and "libSRAL.dylib" or "libSRAL.so")
local sral_lib = ffi.load(lib_name)

ffi.cdef[[
    typedef struct {
        int index;
        const char* name;
        const char* language;
        const char* gender;
        const char* vendor;
    } SRALVoiceInfo;

    bool SRAL_Initialize(int enginesExclude);
    void SRAL_Uninitialize();
    bool SRAL_IsInitialized();
    bool SRAL_Speak(const char* text, bool interrupt);
    bool SRAL_SpeakSsml(const char* ssml, bool interrupt);
    void* SRAL_SpeakToMemory(const char* text, uint64_t* size, int* ch, int* rate, int* bits);
    bool SRAL_Braille(const char* text);
    bool SRAL_Output(const char* text, bool interrupt);
    bool SRAL_StopSpeech();
    bool SRAL_PauseSpeech();
    bool SRAL_ResumeSpeech();
    bool SRAL_IsSpeaking();
    int  SRAL_GetCurrentEngine();
    int  SRAL_GetEngineFeatures(int engine);
    bool SRAL_SetEngineParameter(int engine, int param, void* value);
    bool SRAL_GetEngineParameter(int engine, int param, void* value);
    const char* SRAL_GetEngineName(int engine);
    int  SRAL_GetAvailableEngines();
    int  SRAL_GetActiveEngines();
    bool SRAL_RegisterKeyboardHooks();
    void SRAL_UnregisterKeyboardHooks();
    void SRAL_Delay(int ms);
    bool SRAL_SetEnginesExclude(int enginesExclude);
    int  SRAL_GetEnginesExclude();
    void SRAL_free(void* ptr);

    // Extended Functions
    bool SRAL_SpeakEx(int engine, const char* text, bool interrupt);
    bool SRAL_SpeakSsmlEx(int engine, const char* ssml, bool interrupt);
    void* SRAL_SpeakToMemoryEx(int engine, const char* text, uint64_t* size, int* ch, int* rate, int* bits);
    bool SRAL_BrailleEx(int engine, const char* text);
    bool SRAL_OutputEx(int engine, const char* text, bool interrupt);
    bool SRAL_StopSpeechEx(int engine);
    bool SRAL_PauseSpeechEx(int engine);
    bool SRAL_ResumeSpeechEx(int engine);
    bool SRAL_IsSpeakingEx(int engine);
]]

local SRAL = {
    -- Enum Tables
    Engine = { NONE = 0, NVDA = 2, JAWS = 4, VOICE_OVER = 256, SAPI = 64 },
    Param = { SPEECH_RATE = 0, VOICE_PROPERTIES = 3, VOICE_COUNT = 4, ENABLE_SPELLING = 7 }
}
SRAL.__index = SRAL

function SRAL.new()
    return setmetatable({ _lib = sral_lib }, SRAL)
end

-- --- Utility & Core ---
function SRAL:initialize(ex) return self._lib.SRAL_Initialize(ex or 0) end
function SRAL:uninitialize() self._lib.SRAL_Uninitialize() end
function SRAL:is_initialized() return self._lib.SRAL_IsInitialized() end
function SRAL:delay(ms) self._lib.SRAL_Delay(ms) end
function SRAL:get_engine_name(e) return ffi.string(self._lib.SRAL_GetEngineName(e)) end

-- --- Extended Speech Operations ---
function SRAL:resume_speech_ex(e) return self._lib.SRAL_ResumeSpeechEx(e) end
function SRAL:is_speaking_ex(e) return self._lib.SRAL_IsSpeakingEx(e) end
function SRAL:speak_ex(e, txt, int) return self._lib.SRAL_SpeakEx(e, txt, int == nil or int) end

-- --- Hooks & Engine Management ---
function SRAL:register_keyboard_hooks() return self._lib.SRAL_RegisterKeyboardHooks() end
function SRAL:unregister_keyboard_hooks() self._lib.SRAL_UnregisterKeyboardHooks() end
function SRAL:get_available_engines() return self._lib.SRAL_GetAvailableEngines() end
function SRAL:get_active_engines() return self._lib.SRAL_GetActiveEngines() end
function SRAL:set_engines_exclude(ex) return self._lib.SRAL_SetEnginesExclude(ex) end
function SRAL:get_engines_exclude() return self._lib.SRAL_GetEnginesExclude() end

-- --- Parameter Handling (VOICE_PROPERTIES Logic) ---
function SRAL:get_engine_parameter(engine, param)
    if param == self.Param.VOICE_COUNT then
        local val = ffi.new("int")
        if self._lib.SRAL_GetEngineParameter(engine, param, val) then return tonumber(val[0]) end
    elseif param == self.Param.VOICE_PROPERTIES then
        local count = self:get_engine_parameter(engine, self.Param.VOICE_COUNT) or 0
        if count <= 0 then return {} end
        local voices = ffi.new("SRALVoiceInfo[?]", count)
        if self._lib.SRAL_GetEngineParameter(engine, param, voices) then
            local res = {}
            for i = 0, count - 1 do
                table.insert(res, { index = voices[i].index, name = ffi.string(voices[i].name), lang = ffi.string(voices[i].language) })
            end
            return res
        end
    end
    return nil
end

return SRAL