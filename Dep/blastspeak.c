#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <assert.h>
#include <limits.h>
#include <stdalign.h>

#include <windows.h>
#include <winerror.h>
#include <oaidl.h>
#include <objbase.h>
#include <threads.h>
#include "blastspeak.h"

static_assert(sizeof(IID) == 16, "Target architecture IID structure size must equal 16 bytes.");

alignas(16) static const IID BS_IID_null           = {0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
alignas(16) static const IID BS_IID_IDispatch     = {0x00020400, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
alignas(16) static const IID BS_IID_SpVoice        = {0x96749377, 0x3391, 0x11D2, {0x9E, 0xE3, 0x00, 0xC0, 0x4F, 0x79, 0x73, 0x96}};
alignas(16) static const IID BS_IID_SpMemoryStream = {0x5FB7EF7D, 0xDFF4, 0x468a, {0xB6, 0xB7, 0x2F, 0xCB, 0xD1, 0x88, 0xF9, 0x94}};

static const OLECHAR BS_CATEGORY_ID[] = L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech\\Voices";

enum BS_SAPI_DISPIDS {
    DISPID_VOICE_Voice                                  = 1,
    DISPID_VOICE_AudioOutputStream                      = 2,
    DISPID_VOICE_Rate                                   = 3,
    DISPID_VOICE_Volume                                 = 4,
    DISPID_VOICE_Status                                 = 5,
    DISPID_VOICE_Speak                                  = 12,
    DISPID_VOICE_GetVoices                              = 14,
    DISPID_VOICE_AllowAudioOutputFormatChangesOnNextSet = 21,

    DISPID_TOKEN_GetId                                  = 1,
    DISPID_TOKEN_GetAttribute                           = 3,
    DISPID_TOKEN_GetDescription                         = 4,

    DISPID_CATEGORY_SetId                               = 1,
    DISPID_CATEGORY_EnumerateTokens                     = 3,

    DISPID_STREAM_Format                                = 2,
    DISPID_STREAM_GetData                               = 4,
    DISPID_STREAM_SetData                               = 5,

    DISPID_COLLECTION_Count                             = 1,
    DISPID_COLLECTION_Item                              = 2,

    DISPID_AUDIO_Type                                   = 1,
    DISPID_AUDIO_GetWaveFormatEx                        = 2,
    DISPID_AUDIO_SetWaveFormatEx                        = 3,

    DISPID_WAVE_FormatTag                               = 1,
    DISPID_WAVE_Channels                                = 2,
    DISPID_WAVE_SamplesPerSec                           = 3,
    DISPID_WAVE_AvgBytesPerSec                          = 4,
    DISPID_WAVE_BlockAlign                              = 5,
    DISPID_WAVE_BitsPerSample                           = 6
};

static int blastspeak_get_stream_format(
    blastspeak* restrict const instance, 
    const int retrieve_dispids, 
    unsigned long* restrict const sample_rate, 
    unsigned char* restrict const bits_per_sample, 
    unsigned char* restrict const channels);

#ifndef DEBUG_BLASTSPEAK
#define DEBUG_BLASTSPEAK 1
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define BS_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define BS_COLD_PATH   __attribute__((noinline, cold))
#elif defined(_MSC_VER)
    #define BS_UNLIKELY(x) (x)
    #define BS_COLD_PATH   __declspec(noinline)
#else
    #define BS_UNLIKELY(x) (x)
    #define BS_COLD_PATH
#endif

#if DEBUG_BLASTSPEAK

static mtx_t bs_log_mutex;

void bs_init_debug_mutex(void) {
    mtx_init(&bs_log_mutex, mtx_plain);
}

void bs_destroy_debug_mutex(void) {
    mtx_destroy(&bs_log_mutex);
}

BS_COLD_PATH static void print_error_cold(const char *restrict const context, const HRESULT hr, const UINT puArgErr) {
    typedef struct { HRESULT code; const char *msg; } ErrorMap;
    static const ErrorMap error_table[] = {
        { DISP_E_BADPARAMCOUNT,   "The number of elements provided to DISPPARAMS is different from the number of arguments accepted." },
        { DISP_E_BADVARTYPE,      "One of the arguments in DISPPARAMS is not a valid variant type." },
        { DISP_E_EXCEPTION,       "The application raised an internal member exception (Check EXCEPINFO structure)." },
        { DISP_E_MEMBERNOTFOUND,  "The requested member function or property DISPID does not exist." },
        { DISP_E_NONAMEDARGS,     "This implementation of IDispatch does not support named arguments." },
        { DISP_E_OVERFLOW,        "One of the arguments in DISPPARAMS could not be safely coerced to the specified target type." },
        { DISP_E_PARAMNOTFOUND,   "One of the parameter IDs does not correspond to a valid parameter on the method." },
        { DISP_E_TYPEMISMATCH,    "One or more of the arguments could not be coerced to the expected variant type." },
        { DISP_E_UNKNOWNINTERFACE,"The interface identifier passed in riid is not IID_NULL." },
        { DISP_E_UNKNOWNLCID,     "The member being invoked interprets string arguments according to an unrecognized LCID." },
        { DISP_E_PARAMNOTOPTIONAL,"A required method parameter was omitted." },
        { REGDB_E_CLASSNOTREG,    "SAPI Voice Class not registered. Check if text-to-speech engines are installed on the OS." }
    };

    const char *msg = "System or SAPI internal engine driver failure occurred.";
    const size_t table_size = sizeof(error_table) / sizeof(error_table[0]);
    
    for (size_t i = 0; i < table_size; ++i) {
        if (error_table[i].code == hr) {
            msg = error_table[i].msg;
            break;
        }
    }

    mtx_lock(&bs_log_mutex);
    
    fprintf(stderr, "[BlastSpeak Debug] Failure in %s (HRESULT: 0x%08X): %s\n", context, (unsigned int)hr, msg);
    if (puArgErr != UINT_MAX && (hr == DISP_E_TYPEMISMATCH || hr == DISP_E_PARAMNOTFOUND || hr == DISP_E_BADVARTYPE)) {
        fprintf(stderr, " -> Zero-based parameter array index causing error: %u\n", puArgErr);
    }
    
    fflush(stderr);
    
    mtx_unlock(&bs_log_mutex);
}

#define print_error(context, hr, puArgErr) \
    do { \
        HRESULT _hr_eval = (hr); \
        if (BS_UNLIKELY(FAILED(_hr_eval))) { \
            print_error_cold((context), _hr_eval, (puArgErr)); \
        } \
    } while(0)

#else
#define print_error(context, hr, puArgErr) ((void)0)
#endif


static inline HRESULT InitVariantFromString(PCWSTR restrict const psz, VARIANT* restrict const pvar) {
    if (BS_UNLIKELY(!psz)) {
        V_VT(pvar) = VT_EMPTY;
        return E_INVALIDARG;
    }

    BSTR const bstr = SysAllocString(psz);
    if (BS_UNLIKELY(!bstr)) {
        V_VT(pvar) = VT_EMPTY;
        return E_OUTOFMEMORY;
    }

    V_VT(pvar) = VT_BSTR;
    V_BSTR(pvar) = bstr;
    return S_OK;
}

static inline HRESULT InitVariantFromUInt32(const ULONG ulVal, VARIANT* restrict const pvar) {
    V_VT(pvar) = VT_UI4;
    V_UI4(pvar) = ulVal;
    return S_OK;
}

static char* blastspeak_get_temporary_memory(blastspeak* restrict const instance, const unsigned int bytes) {
    if (bytes <= blastspeak_static_memory_length) {
        return instance->static_memory;
    }
    if (instance->allocated_memory && bytes <= instance->allocated_memory_length) {
        return instance->allocated_memory;
    }
    
    char* const new_mem = (char*)realloc(instance->allocated_memory, (size_t)bytes);
    if (BS_UNLIKELY(!new_mem)) {
        return NULL;
    }
    
    instance->allocated_memory = new_mem;
    instance->allocated_memory_length = bytes;
    return new_mem;
}

static WCHAR* blastspeak_get_wchar_from_utf8(
    blastspeak* restrict const instance, const char* restrict const the_string, const unsigned int length_in_bytes) 
{
    if (BS_UNLIKELY(!the_string)) {
        return NULL;
    }

    const int src_len = (length_in_bytes == 0) ? (int)strlen(the_string) : (int)length_in_bytes;
    if (BS_UNLIKELY(src_len <= 0)) {
        return NULL;
    }

    const int max_wchars = src_len + 1;
    WCHAR* const result = (WCHAR*)blastspeak_get_temporary_memory(instance, (unsigned int)(max_wchars + 1) * sizeof(WCHAR));
    if (BS_UNLIKELY(!result)) {
        return NULL;
    }

    const int written = MultiByteToWideChar(CP_UTF8, 0, the_string, src_len, result, max_wchars);
    if (BS_UNLIKELY(written <= 0)) {
        return NULL;
    }
    result[written] = L'\0';
    return result;
}

static char* blastspeak_get_UTF8_from_BSTR(blastspeak* restrict const instance, const BSTR restrict the_string) {
    if (BS_UNLIKELY(!the_string)) {
        return NULL;
    }

    const int src_len = (int)SysStringLen(the_string);
    if (BS_UNLIKELY(src_len <= 0)) {
        return NULL;
    }

    const int max_bytes = (src_len * 3) + 1;
    char* const result = blastspeak_get_temporary_memory(instance, (unsigned int)(max_bytes + 1));
    if (BS_UNLIKELY(!result)) {
        return NULL;
    }

    const int actual_bytes = WideCharToMultiByte(CP_UTF8, 0, the_string, src_len, result, max_bytes, NULL, NULL);
    if (BS_UNLIKELY(actual_bytes <= 0)) {
        return NULL;
    }

    result[actual_bytes] = '\0';
    return result;
}


int blastspeak_initialize(blastspeak* restrict const instance) {
    if (BS_UNLIKELY(!instance)) {
        return 0;
    }

    instance->allocated_memory = NULL;
    instance->allocated_memory_length = 0;
    instance->static_memory[0] = 0;
    instance->must_reset_output = 0;
    instance->reserved_padding = 0;
    instance->voices = NULL;
    instance->voice_count = 1;
    instance->voice_collection_item_dispid = DISPID_UNKNOWN;
    instance->format = NULL;
    instance->current_voice_token = NULL;
    instance->voice = NULL;
    instance->default_voice_token = NULL;
    instance->com_is_owned = 0;

    const HRESULT com_hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(com_hr)) {
        return 0;
    }

    if (SUCCEEDED(com_hr)) {
        instance->com_is_owned = 1;
    }

    IDispatch* voice_category = NULL;
    DISPID* const restrict v_dispids = (DISPID*)instance->voice_dispids;
    DISPID* const restrict m_dispids = (DISPID*)instance->memory_stream_dispids;
    DISPID* const restrict t_dispids = (DISPID*)instance->voice_token_dispids;

    static_assert(sizeof(instance->voice_dispids) >= (8 * sizeof(DISPID)), "voice_dispids layout tracking shortfall.");
    static_assert(sizeof(instance->memory_stream_dispids) >= (3 * sizeof(DISPID)), "memory_stream_dispids layout tracking shortfall.");
    static_assert(sizeof(instance->voice_token_dispids) >= (2 * sizeof(DISPID)), "voice_token_dispids layout tracking shortfall.");

    memcpy(v_dispids, (const DISPID[]){
        DISPID_VOICE_AllowAudioOutputFormatChangesOnNextSet,
        DISPID_VOICE_AudioOutputStream,
        DISPID_VOICE_GetVoices,
        DISPID_VOICE_Rate,
        DISPID_VOICE_Speak,
        DISPID_VOICE_Status,
        DISPID_VOICE_Voice,
        DISPID_VOICE_Volume
    }, 8 * sizeof(DISPID));

    memcpy(m_dispids, (const DISPID[]){
        DISPID_STREAM_GetData,
        DISPID_STREAM_Format,
        DISPID_STREAM_SetData
    }, 3 * sizeof(DISPID));

    memcpy(t_dispids, (const DISPID[]){
        DISPID_TOKEN_GetAttribute,
        DISPID_TOKEN_GetDescription
    }, 2 * sizeof(DISPID));

    instance->audio_format_getwaveformatex_dispid = DISPID_AUDIO_GetWaveFormatEx;
    instance->audio_format_setwaveformatex_dispid = DISPID_AUDIO_SetWaveFormatEx;

    HRESULT hr = CoCreateInstance(&BS_IID_SpVoice, NULL, CLSCTX_INPROC_SERVER, &BS_IID_IDispatch, (void**)&instance->voice);
    if (BS_UNLIKELY(FAILED(hr))) {
        goto error_cleanup;
    }

    DISPPARAMS parameters = { .rgvarg = NULL, .rgdispidNamedArgs = NULL, .cArgs = 0, .cNamedArgs = 0 };
    VARIANT return_value;
    VariantInit(&return_value);

    hr = instance->voice->lpVtbl->Invoke(
        instance->voice, DISPID_VOICE_Voice, &BS_IID_null, LOCALE_SYSTEM_DEFAULT, 
        DISPATCH_PROPERTYGET, &parameters, &return_value, NULL, NULL
    );
    if (BS_UNLIKELY(FAILED(hr) || V_VT(&return_value) != VT_DISPATCH)) {
        VariantClear(&return_value);
        goto error_cleanup;
    }

    instance->default_voice_token = V_DISPATCH(&return_value);
    V_VT(&return_value) = VT_EMPTY; 

    CLSID voice_category_clsid;
    hr = CLSIDFromProgID(L"SAPI.SpObjectTokenCategory", &voice_category_clsid);
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(&voice_category_clsid, NULL, CLSCTX_INPROC_SERVER, &BS_IID_IDispatch, (void**)&voice_category);
    }

    if (SUCCEEDED(hr) && voice_category) {
        VARIANT arguments[2];
        VariantInit(&arguments[0]);
        VariantInit(&arguments[1]);
        
        V_VT(&arguments[0]) = VT_BOOL;
        V_BOOL(&arguments[0]) = VARIANT_FALSE;

        hr = InitVariantFromString(BS_CATEGORY_ID, &arguments[1]);
        if (SUCCEEDED(hr)) {
            parameters.rgvarg = arguments;
            parameters.cArgs = 2;

            hr = voice_category->lpVtbl->Invoke(
                voice_category, DISPID_CATEGORY_SetId, &BS_IID_null, LOCALE_SYSTEM_DEFAULT, 
                DISPATCH_METHOD, &parameters, &return_value, NULL, NULL
            );
            VariantClear(&arguments[0]);
            VariantClear(&arguments[1]);
            VariantClear(&return_value);
        }

        if (SUCCEEDED(hr)) {
            parameters.rgvarg = NULL;
            parameters.cArgs = 0;

            hr = voice_category->lpVtbl->Invoke(
                voice_category, DISPID_CATEGORY_EnumerateTokens, &BS_IID_null, LOCALE_SYSTEM_DEFAULT, 
                DISPATCH_METHOD, &parameters, &return_value, NULL, NULL
            );
            if (SUCCEEDED(hr) && V_VT(&return_value) == VT_DISPATCH) {
                instance->voices = V_DISPATCH(&return_value);
                V_VT(&return_value) = VT_EMPTY;
            } else {
                VariantClear(&return_value);
            }
        }
        voice_category->lpVtbl->Release(voice_category);
        voice_category = NULL;
    }

    if (instance->voices) {
        parameters.rgvarg = NULL;
        parameters.cArgs = 0;

        hr = instance->voices->lpVtbl->Invoke(
            instance->voices, DISPID_COLLECTION_Count, &BS_IID_null, LOCALE_SYSTEM_DEFAULT, 
            DISPATCH_PROPERTYGET, &parameters, &return_value, NULL, NULL
        );
        if (SUCCEEDED(hr) && V_VT(&return_value) == VT_I4 && V_I4(&return_value) > 0) {
            instance->voice_count = (unsigned int)V_I4(&return_value);
            instance->voice_collection_item_dispid = DISPID_COLLECTION_Item;
        } else {
            instance->voices->lpVtbl->Release(instance->voices);
            instance->voices = NULL;
        }
        VariantClear(&return_value);
    }

    if (BS_UNLIKELY(blastspeak_get_stream_format(instance, 1, &instance->sample_rate, &instance->bits_per_sample, &instance->channels) == 0)) {
        goto error_cleanup;
    }

    return 1;

error_cleanup:
    if (voice_category) voice_category->lpVtbl->Release(voice_category);
    if (instance->voices) { instance->voices->lpVtbl->Release(instance->voices); instance->voices = NULL; }
    if (instance->default_voice_token) { instance->default_voice_token->lpVtbl->Release(instance->default_voice_token); instance->default_voice_token = NULL; }
    if (instance->voice) { instance->voice->lpVtbl->Release(instance->voice); instance->voice = NULL; }
    
    if (instance->com_is_owned) {
        CoUninitialize();
        instance->com_is_owned = 0;
    }
    return 0;
}

void blastspeak_destroy(blastspeak* restrict const instance) {
    if (BS_UNLIKELY(!instance)) {
        return;
    }

    if (instance->allocated_memory) {
        free(instance->allocated_memory);
        instance->allocated_memory = NULL;
    }

    if (instance->voice) {
        instance->voice->lpVtbl->Release(instance->voice);
        instance->voice = NULL;
    }
    if (instance->voices) {
        instance->voices->lpVtbl->Release(instance->voices);
        instance->voices = NULL;
    }
    if (instance->default_voice_token) {
        instance->default_voice_token->lpVtbl->Release(instance->default_voice_token);
        instance->default_voice_token = NULL;
    }
    if (instance->format) {
        instance->format->lpVtbl->Release(instance->format);
        instance->format = NULL;
    }
    if (instance->current_voice_token) {
        instance->current_voice_token->lpVtbl->Release(instance->current_voice_token);
        instance->current_voice_token = NULL;
    }

    instance->allocated_memory_length = 0;
    instance->voice_count = 0;
    instance->must_reset_output = 0;

    if (instance->com_is_owned) {
        CoUninitialize();
        instance->com_is_owned = 0;
    }
}

static int blastspeak_speak_internal(blastspeak* restrict const instance, const char* restrict const text) {
    WCHAR* const utf16_string = blastspeak_get_wchar_from_utf8(instance, text, 0);
    if (BS_UNLIKELY(!utf16_string)) {
        return 0;
    }

    VARIANT arguments[2] = {{0}, {0}};
    
    HRESULT hr = InitVariantFromString(utf16_string, &arguments[1]); 
    if (BS_UNLIKELY(FAILED(hr))) {
        return 0;
    }

    V_VT(&arguments[0]) = VT_UI4;
    V_UI4(&arguments[0]) = 1;

    DISPPARAMS parameters = { .rgvarg = arguments, .rgdispidNamedArgs = NULL, .cArgs = 2, .cNamedArgs = 0 };

    hr = instance->voice->lpVtbl->Invoke(
        instance->voice, DISPID_VOICE_Speak, &BS_IID_null, LOCALE_SYSTEM_DEFAULT, 
        DISPATCH_METHOD, &parameters, NULL, NULL, NULL
    );
        
    VariantClear(&arguments[1]);
    return SUCCEEDED(hr);
}

static int blastspeak_reset_output(blastspeak* restrict const instance) {
    VARIANT argument = {0};
    DISPID dispid_named = DISPID_PROPERTYPUT;
    DISPPARAMS parameters = { .rgvarg = &argument, .rgdispidNamedArgs = &dispid_named, .cArgs = 1, .cNamedArgs = 1 };

    HRESULT hr = instance->voice->lpVtbl->Invoke(
        instance->voice, DISPID_VOICE_AudioOutputStream, &BS_IID_null, LOCALE_SYSTEM_DEFAULT, 
        DISPATCH_PROPERTYPUTREF, &parameters, NULL, NULL, NULL
    );
    
    if (BS_UNLIKELY(FAILED(hr))) {
        return 0;
    }
    
    instance->must_reset_output = 0;
    return 1;
}

int blastspeak_speak(blastspeak* restrict const instance, const char* restrict const text) {
    if (instance->must_reset_output && BS_UNLIKELY(!blastspeak_reset_output(instance))) {
        return 0;
    }
    return blastspeak_speak_internal(instance, text);
}

static IDispatch* blastspeak_get_voice(blastspeak* restrict const instance, const unsigned int voice_index) {
    if (BS_UNLIKELY(voice_index >= instance->voice_count)) {
        return NULL;
    }

    if (!instance->voices) {
        if (BS_UNLIKELY(voice_index != 0 || !instance->default_voice_token)) {
            return NULL;
        }
        instance->default_voice_token->lpVtbl->AddRef(instance->default_voice_token);
        return instance->default_voice_token;
    }

    VARIANT argument = {0};
    V_VT(&argument) = VT_UI4;
    V_UI4(&argument) = voice_index;

    DISPPARAMS parameters = { .rgvarg = &argument, .rgdispidNamedArgs = NULL, .cArgs = 1, .cNamedArgs = 0 };
    VARIANT return_value = {0};

    HRESULT hr = instance->voices->lpVtbl->Invoke(
        instance->voices, instance->voice_collection_item_dispid, &BS_IID_null, 
        LOCALE_SYSTEM_DEFAULT, DISPATCH_METHOD, &parameters, &return_value, NULL, NULL
    );
    
    if (BS_UNLIKELY(FAILED(hr) || V_VT(&return_value) != VT_DISPATCH)) {
        VariantClear(&return_value);
        return NULL;
    }

    IDispatch* const voice_token = V_DISPATCH(&return_value);
    V_VT(&return_value) = VT_EMPTY;
    return voice_token;
}

int blastspeak_set_voice(blastspeak* restrict const instance, const unsigned int voice_index) {
    if (instance->must_reset_output && BS_UNLIKELY(!blastspeak_reset_output(instance))) {
        return 0;
    }
    
    IDispatch* const voice_token = blastspeak_get_voice(instance, voice_index);
    if (BS_UNLIKELY(!voice_token)) {
        return 0;
    }

    VARIANT argument = {0};
    V_VT(&argument) = VT_DISPATCH;
    V_DISPATCH(&argument) = voice_token;

    DISPID dispid_named = DISPID_PROPERTYPUT;
    DISPPARAMS parameters = { .rgvarg = &argument, .rgdispidNamedArgs = &dispid_named, .cArgs = 1, .cNamedArgs = 1 };

    HRESULT hr = instance->voice->lpVtbl->Invoke(
        instance->voice, DISPID_VOICE_Voice, &BS_IID_null, LOCALE_SYSTEM_DEFAULT, 
        DISPATCH_PROPERTYPUTREF, &parameters, NULL, NULL, NULL
    );
    
    if (BS_UNLIKELY(FAILED(hr))) {
        voice_token->lpVtbl->Release(voice_token);
        return 0;
    }
    
    if (instance->current_voice_token) {
        instance->current_voice_token->lpVtbl->Release(instance->current_voice_token);
    }
    instance->current_voice_token = voice_token;
    
    V_VT(&argument) = VT_EMPTY;
    hr = instance->voice->lpVtbl->Invoke(
        instance->voice, DISPID_VOICE_AudioOutputStream, &BS_IID_null, LOCALE_SYSTEM_DEFAULT, 
        DISPATCH_PROPERTYPUTREF, &parameters, NULL, NULL, NULL
    );
    
    if (BS_UNLIKELY(FAILED(hr))) {
        return 0;
    }
    
    return blastspeak_get_stream_format(instance, 0, &instance->sample_rate, &instance->bits_per_sample, &instance->channels) != 0;
}

int blastspeak_get_voice_description(blastspeak* restrict const instance, const unsigned int voice_index, char* restrict const out_buffer, const size_t max_bytes) {
    if (BS_UNLIKELY(!out_buffer || max_bytes == 0)) {
        return 0;
    }

    IDispatch* const voice_token = blastspeak_get_voice(instance, voice_index);
    if (BS_UNLIKELY(!voice_token)) {
        return 0;
    }

    DISPPARAMS parameters = { .rgvarg = NULL, .rgdispidNamedArgs = NULL, .cArgs = 0, .cNamedArgs = 0 };
    VARIANT return_value = {0};

    HRESULT hr = voice_token->lpVtbl->Invoke(
        voice_token, DISPID_TOKEN_GetDescription, &BS_IID_null, LOCALE_SYSTEM_DEFAULT, 
        DISPATCH_METHOD, &parameters, &return_value, NULL, NULL
    );
    voice_token->lpVtbl->Release(voice_token);
    
    if (BS_UNLIKELY(FAILED(hr) || V_VT(&return_value) != VT_BSTR)) {
        VariantClear(&return_value);
        return 0;
    }

    int success = 0;
    char* const temp_utf8 = blastspeak_get_UTF8_from_BSTR(instance, V_BSTR(&return_value));
    if (temp_utf8) {
        snprintf(out_buffer, max_bytes, "%s", temp_utf8);
        success = 1;
    }
    
    VariantClear(&return_value);
    return success;
}

int blastspeak_get_voice_attribute(blastspeak* restrict const instance, const unsigned int voice_index, const char* restrict const attribute, char* restrict const out_buffer, const size_t max_bytes) {
    if (BS_UNLIKELY(!attribute || !out_buffer || max_bytes == 0)) {
        return 0;
    }

    IDispatch* const voice_token = blastspeak_get_voice(instance, voice_index);
    if (BS_UNLIKELY(!voice_token)) {
        return 0;
    }

    WCHAR* const utf16_string = blastspeak_get_wchar_from_utf8(instance, attribute, 0);
    if (BS_UNLIKELY(!utf16_string)) {
        voice_token->lpVtbl->Release(voice_token);
        return 0;
    }

    VARIANT argument = {0};
    HRESULT hr = InitVariantFromString(utf16_string, &argument);
    if (BS_UNLIKELY(FAILED(hr))) {
        voice_token->lpVtbl->Release(voice_token);
        return 0;
    }

    DISPPARAMS parameters = { .rgvarg = &argument, .rgdispidNamedArgs = NULL, .cArgs = 1, .cNamedArgs = 0 };
    VARIANT return_value = {0};

    hr = voice_token->lpVtbl->Invoke(
        voice_token, DISPID_TOKEN_GetAttribute, &BS_IID_null, LOCALE_SYSTEM_DEFAULT, 
        DISPATCH_METHOD, &parameters, &return_value, NULL, NULL
    );
        
    VariantClear(&argument);
    voice_token->lpVtbl->Release(voice_token);
    
    if (BS_UNLIKELY(FAILED(hr) || V_VT(&return_value) != VT_BSTR)) {
        VariantClear(&return_value);
        return 0;
    }

    int success = 0;
    char* const temp_utf8 = blastspeak_get_UTF8_from_BSTR(instance, V_BSTR(&return_value));
    if (temp_utf8) {
        snprintf(out_buffer, max_bytes, "%s", temp_utf8);
        success = 1;
    }

    VariantClear(&return_value);
    return success;
}

int blastspeak_get_voice_languages(blastspeak* restrict const instance, const unsigned int voice_index, char* restrict const out_buffer, const size_t max_bytes) {
    if (BS_UNLIKELY(!out_buffer || max_bytes == 0)) return 0;
    out_buffer[0] = '\0';
    
    char raw_attr_buffer[256] = {0}; 
    if (!blastspeak_get_voice_attribute(instance, voice_index, "language", raw_attr_buffer, sizeof(raw_attr_buffer))) {
        return 0;
    }

    long codes[blastspeak_max_languages_per_voice];
    int languages = 0;
    const char* scan_ptr = raw_attr_buffer;

    while (languages < blastspeak_max_languages_per_voice) {
        char* next_ptr = NULL;
        long parsed_code = strtol(scan_ptr, &next_ptr, 16);
        
        if (scan_ptr == next_ptr || parsed_code == 0L) {
            while (*scan_ptr == ' ' || *scan_ptr == ';') {
                scan_ptr++;
            }
            if (*scan_ptr == '\0') break;
            scan_ptr++; 
            continue;
        }
        scan_ptr = next_ptr;

        if (BS_UNLIKELY(parsed_code < -32768L || parsed_code > 32767L)) {
            return 0;
        }

        int duplicate = 0;
        for (int i = 0; i < languages; ++i) {
            if (codes[i] == parsed_code) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        codes[languages++] = parsed_code;

        while (*scan_ptr == ' ' || *scan_ptr == ';') {
            scan_ptr++;
        }
        if (*scan_ptr == '\0') {
            break;
        }
    }

    if (languages == 0) {
        return 0;
    }

    char* dst = out_buffer;
    const char* const end_limit = out_buffer + max_bytes - 1;

    for (int i = 0; i < languages; ++i) {
        if (i > 0 && dst < end_limit) {
            *dst++ = ' ';
        }

        const size_t remaining_space = (size_t)(end_limit - dst);
        if (BS_UNLIKELY(remaining_space == 0)) {
            break;
        }

        int written = GetLocaleInfoA((LCID)codes[i], LOCALE_SISO639LANGNAME, dst, (int)(remaining_space + 1));
        if (BS_UNLIKELY(written == 0)) {
            out_buffer[0] = '\0';
            return 0;
        }
        dst += (written - 1);

        if (SUBLANGID((USHORT)codes[i]) == 0) {
            continue;
        }

        const size_t space_for_sublang = (size_t)(end_limit - dst);
        if (space_for_sublang > 1) {
            *dst = '-';
            int sub_written = GetLocaleInfoA((LCID)codes[i], LOCALE_SISO3166CTRYNAME, dst + 1, (int)space_for_sublang);
            if (sub_written > 0) {
                dst += sub_written;
            }
        }
    }
    *dst = '\0';
    return 1;
}

static int blastspeak_set_long_property(blastspeak* restrict const instance, const DISPID dispid, const long value) {
    if (BS_UNLIKELY(!instance || !instance->voice)) return 0;

    VARIANT argument = {0};
    V_VT(&argument) = VT_I4;
    V_I4(&argument) = value;

    DISPID dispid_named = DISPID_PROPERTYPUT;
    DISPPARAMS parameters = { .rgvarg = &argument, .rgdispidNamedArgs = &dispid_named, .cArgs = 1, .cNamedArgs = 1 };

    HRESULT hr = instance->voice->lpVtbl->Invoke(
        instance->voice, dispid, &BS_IID_null, LOCALE_SYSTEM_DEFAULT, 
        DISPATCH_PROPERTYPUT, &parameters, NULL, NULL, NULL
    );
        
    return SUCCEEDED(hr);
}

static int blastspeak_get_long_property(blastspeak* restrict const instance, const DISPID dispid, long* restrict const value, IDispatch* restrict object) {
    if (!object) {
        object = instance->voice;
    }
    if (BS_UNLIKELY(!object || !value)) return 0;

    DISPPARAMS parameters = { .rgvarg = NULL, .rgdispidNamedArgs = NULL, .cArgs = 0, .cNamedArgs = 0 };
    VARIANT return_value = {0};

    HRESULT hr = object->lpVtbl->Invoke(
        object, dispid, &BS_IID_null, LOCALE_SYSTEM_DEFAULT, 
        DISPATCH_PROPERTYGET, &parameters, &return_value, NULL, NULL
    );
        
    if (BS_UNLIKELY(FAILED(hr))) {
        return 0;
    }
    
    int success = 0;
    const VARTYPE vt = V_VT(&return_value);
    if (vt == VT_I4) {
        *value = V_I4(&return_value);
        success = 1;
    } else if (vt == VT_I2) {
        *value = V_I2(&return_value);
        success = 1;
    }

    VariantClear(&return_value);
    return success;
}

static int blastspeak_get_stream_format(blastspeak* restrict const instance, const int retrieve_dispids, unsigned long* restrict const sample_rate, unsigned char* restrict const bits_per_sample, unsigned char* restrict const channels) {
    (void)retrieve_dispids; 

    if (instance->format) {
        instance->format->lpVtbl->Release(instance->format);
        instance->format = NULL;
    }

    DISPPARAMS parameters = { .rgvarg = NULL, .rgdispidNamedArgs = NULL, .cArgs = 0, .cNamedArgs = 0 };
    VARIANT return_value = {0};

    HRESULT hr = instance->voice->lpVtbl->Invoke(
        instance->voice, DISPID_VOICE_AudioOutputStream, &BS_IID_null, 
        LOCALE_SYSTEM_DEFAULT, DISPATCH_PROPERTYGET, &parameters, &return_value, NULL, NULL
    );
    if (BS_UNLIKELY(FAILED(hr) || V_VT(&return_value) != VT_DISPATCH)) {
        VariantClear(&return_value);
        return 0;
    }
    IDispatch* const audio_device_stream = V_DISPATCH(&return_value);
    V_VT(&return_value) = VT_EMPTY;

    hr = audio_device_stream->lpVtbl->Invoke(
        audio_device_stream, DISPID_STREAM_Format, &BS_IID_null, 
        LOCALE_SYSTEM_DEFAULT, DISPATCH_PROPERTYGET, &parameters, &return_value, NULL, NULL
    );
    audio_device_stream->lpVtbl->Release(audio_device_stream);
    
    if (BS_UNLIKELY(FAILED(hr) || V_VT(&return_value) != VT_DISPATCH)) {
        VariantClear(&return_value);
        return 0;
    }
    instance->format = V_DISPATCH(&return_value);
    V_VT(&return_value) = VT_EMPTY;

    hr = instance->format->lpVtbl->Invoke(
        instance->format, DISPID_AUDIO_GetWaveFormatEx, &BS_IID_null, 
        LOCALE_SYSTEM_DEFAULT, DISPATCH_METHOD, &parameters, &return_value, NULL, NULL
    );
    if (BS_UNLIKELY(FAILED(hr) || V_VT(&return_value) != VT_DISPATCH)) {
        VariantClear(&return_value);
        return 0;
    }
    IDispatch* const formatex = V_DISPATCH(&return_value);
    V_VT(&return_value) = VT_EMPTY;

    long temp;
    if (BS_UNLIKELY(blastspeak_get_long_property(instance, DISPID_WAVE_FormatTag, &temp, formatex) == 0 || temp != 1)) {
        goto error;
    }

    if (BS_UNLIKELY(blastspeak_get_long_property(instance, DISPID_WAVE_BitsPerSample, &temp, formatex) == 0 || (temp != 8 && temp != 16))) {
        goto error;
    }
    *bits_per_sample = (unsigned char)temp;

    if (BS_UNLIKELY(blastspeak_get_long_property(instance, DISPID_WAVE_Channels, &temp, formatex) == 0 || (temp != 1 && temp != 2))) {
        goto error;
    }
    *channels = (unsigned char)temp;

    if (BS_UNLIKELY(blastspeak_get_long_property(instance, DISPID_WAVE_SamplesPerSec, &temp, formatex) == 0 || temp < 8000 || temp > 192000)) {
        goto error;
    }
    *sample_rate = (unsigned long)temp;

    formatex->lpVtbl->Release(formatex);
    return 1;

error:
    if (formatex) {
        formatex->lpVtbl->Release(formatex);
    }
    return 0;
}

int blastspeak_get_voice_rate(blastspeak* restrict const instance, long* restrict const result) {
    return blastspeak_get_long_property(instance, DISPID_VOICE_Rate, result, NULL);
}

int blastspeak_set_voice_rate(blastspeak* restrict const instance, const long value) {
    if (BS_UNLIKELY(value < -10 || value > 10)) {
        return 0;
    }
    return blastspeak_set_long_property(instance, DISPID_VOICE_Rate, value);
}

int blastspeak_get_voice_volume(blastspeak* restrict const instance, long* restrict const result) {
    return blastspeak_get_long_property(instance, DISPID_VOICE_Volume, result, NULL);
}

int blastspeak_set_voice_volume(blastspeak* restrict const instance, const long value) {
    if (BS_UNLIKELY(value < 0 || value > 100)) {
        return 0;
    }
    return blastspeak_set_long_property(instance, DISPID_VOICE_Volume, value);
}

char* blastspeak_speak_to_memory(blastspeak* restrict const instance, unsigned long* restrict const bytes, const char* restrict const text) {
    if (BS_UNLIKELY(!instance || !bytes || !text || !instance->format)) {
        return NULL;
    }
    *bytes = 0;

    IDispatch* stream = NULL;
    IDispatch* stream_format = NULL;
    IDispatch* formatex = NULL;
    char* data = NULL;

    HRESULT hr = CoCreateInstance(&BS_IID_SpMemoryStream, NULL, CLSCTX_INPROC_SERVER, &BS_IID_IDispatch, (void**)&stream);
    if (BS_UNLIKELY(FAILED(hr))) {
        return NULL;
    }

    const DISPPARAMS empty_params = { .rgvarg = NULL, .rgdispidNamedArgs = NULL, .cArgs = 0, .cNamedArgs = 0 };
    VARIANT return_value = {0};

    hr = stream->lpVtbl->Invoke(
        stream, DISPID_STREAM_Format, &BS_IID_null, LOCALE_SYSTEM_DEFAULT, 
        DISPATCH_PROPERTYGET, (DISPPARAMS*)&empty_params, &return_value, NULL, NULL
    );
    if (BS_UNLIKELY(FAILED(hr) || V_VT(&return_value) != VT_DISPATCH)) {
        goto cleanup;
    }
        stream_format = V_DISPATCH(&return_value);
    V_VT(&return_value) = VT_EMPTY;

    hr = instance->format->lpVtbl->Invoke(
        instance->format, instance->audio_format_getwaveformatex_dispid, &BS_IID_null, 
        LOCALE_SYSTEM_DEFAULT, DISPATCH_METHOD, (DISPPARAMS*)&empty_params, &return_value, NULL, NULL
    );
    if (BS_UNLIKELY(FAILED(hr) || V_VT(&return_value) != VT_DISPATCH)) {
        goto cleanup;
    }
    formatex = V_DISPATCH(&return_value);
    V_VT(&return_value) = VT_EMPTY;

    VARIANT method_arg = {0};
    V_VT(&method_arg) = VT_DISPATCH;
    V_DISPATCH(&method_arg) = formatex;
    const DISPPARAMS method_params = { .rgvarg = &method_arg, .rgdispidNamedArgs = NULL, .cArgs = 1, .cNamedArgs = 0 };

    hr = stream_format->lpVtbl->Invoke(
        stream_format, instance->audio_format_setwaveformatex_dispid, &BS_IID_null, 
        LOCALE_SYSTEM_DEFAULT, DISPATCH_METHOD, (DISPPARAMS*)&method_params, NULL, NULL, NULL
    );
    if (BS_UNLIKELY(FAILED(hr))) {
        goto cleanup;
    }

    VARIANT prop_arg = {0};
    V_VT(&prop_arg) = VT_DISPATCH;
    V_DISPATCH(&prop_arg) = stream;
    const DISPID putref_dispid = DISPID_PROPERTYPUT;
    const DISPPARAMS prop_params = { .rgvarg = &prop_arg, .rgdispidNamedArgs = (DISPID*)&putref_dispid, .cArgs = 1, .cNamedArgs = 1 };

    hr = instance->voice->lpVtbl->Invoke(
        instance->voice, DISPID_VOICE_AudioOutputStream, &BS_IID_null, LOCALE_SYSTEM_DEFAULT, 
        DISPATCH_PROPERTYPUTREF, (DISPPARAMS*)&prop_params, NULL, NULL, NULL
    );
    if (BS_UNLIKELY(FAILED(hr))) {
        goto cleanup;
    }

    instance->must_reset_output = 1;

    if (BS_UNLIKELY(!blastspeak_speak_internal(instance, text))) {
        goto cleanup;
    }

    hr = stream->lpVtbl->Invoke(
        stream, DISPID_STREAM_GetData, &BS_IID_null, LOCALE_SYSTEM_DEFAULT, 
        DISPATCH_METHOD, (DISPPARAMS*)&empty_params, &return_value, NULL, NULL
    );
    if (BS_UNLIKELY(FAILED(hr) || V_VT(&return_value) != (VT_ARRAY | VT_UI1))) {
        goto cleanup;
    }

    LPSAFEARRAY const psa = V_ARRAY(&return_value);
    if (BS_UNLIKELY(!psa || psa->cDims != 1)) {
        goto cleanup;
    }

    const ULONG elements = psa->rgsabound[0].cElements;
    if (elements > 0) {
        void* pv_data = NULL;
        if (SUCCEEDED(SafeArrayAccessData(psa, &pv_data)) && pv_data) {
            data = blastspeak_get_temporary_memory(instance, (unsigned int)elements);
            if (data) {
                memcpy(data, pv_data, (size_t)elements);
                *bytes = (unsigned long)elements;
            }
            SafeArrayUnaccessData(psa);
        }
    }

cleanup:
    if (V_VT(&return_value) != VT_EMPTY) {
        VariantClear(&return_value);
    }
    if (instance->must_reset_output) {
        blastspeak_reset_output(instance);
    }
    if (formatex) {
        formatex->lpVtbl->Release(formatex);
    }
    if (stream_format) {
        stream_format->lpVtbl->Release(stream_format);
    }
    if (stream) {
        stream->lpVtbl->Release(stream);
    }

    return data;
}

#ifdef __cplusplus
}
#endif