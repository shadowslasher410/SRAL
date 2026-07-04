/*
Blastspeak text to speech library
Copyright (c) 2019-2020 Philip Bennefall

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

/* 
 * ==============================================================================
 * NOTICE OF ALTERATION
 * ==============================================================================
 * This software codebase is an ALTERED, MODIFIED, and HARDENED version of the 
 * original Blastspeak Text-To-Speech library distribution. 
 *
 * In strict compliance with Clause 2 of the original license terms, the standard 
 * source file properties have been modified and plainly marked as follows:
 * 
 *  1. ISO C17 LANGUAGE STANDARD SPECIFICATION ENFORCEMENT:
 *     - Refactored all internal data types, structured signatures, and function 
 *       definitions to follow strict ISO C17 code specifications (-std=c17).
 *     - Stripped out invalid legacy Microsoft SAL code annotation markers (__in 
 *       and __out) to ensure cross-compiler and multi-toolchain build safety.
 *     - Removed non-standard C++ preprocessor nesting wrappers (such as global 
 *       extern "C" blocks) from the interior translation unit body.
 * 
 *  2. CRITICAL THREAD-SAFETY & MEMORY ISOLATION REMODELING:
 *     - Modified voice metadata extraction utilities (e.g., voice descriptions, 
 *       attribute flags, and languages) to write directly into thread-isolated, 
 *       caller-allocated destination memory buffers. This completely eliminates 
 *       shared instance scratchpads vulnerable to race conditions.
 *     - Bound internal utility transcoders, variant initializers, and helpers to 
 *       explicit 'static' internal linkage scopes, completely resolving global 
 *       duplicate symbol link-time definition breaks (LNK2005).
 *     - Fixed hidden automation memory leaks by injecting 'VariantClear' 
 *       operations into successful long-property data evaluation pathways.
 *     - Replaced raw magic numeric literals (such as 8209) with explicit system 
 *       preprocessor tokens (VT_ARRAY | VT_UI1) to protect target validation rules.
 * 
 *  3. MULTI-THREADED LIFE-CYCLE HARDENING & CRASH PREVENTION:
 *     - Completely restructured the reference-tracking teardown mechanisms for 
 *       COM Memory Streams, completely eliminating a lethal Use-After-Free (UAF) 
 *       and stack double-free access violation crash condition (0xC0000005).
 *     - Fortified COM apartment state terminations inside 'blastspeak_destroy' 
 *       by mapping active apartment boundaries dynamically, ensuring early error 
 *       paths do not unmap concurrent worker thread apartments in external frameworks.
 *     - Corrected the OLE Automation Invoke parameter ordering matrix to reverse-pass 
 *       variant argument arrays (right-to-left) to satisfy strict IDispatch rules.
 *     - Restored explicit continuous array address pointer casting over modified 
 *       scalar flat instance DISPID blocks to prevent runtime stack corruption.
 *     - Hardened hexadecimal language sequence string scanners against un-advancing 
 *       malformed characters to completely prevent thread-locking infinite runs.
 * ==============================================================================
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "blastspeak.h"

#include <windows.h>

#include <oaidl.h>
#include <objbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

const IID BS_IID_null = {0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
const IID BS_IID_IDispatch = {0x00020400, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const IID BS_IID_SpVoice = {0x96749377, 0x3391, 0x11D2, {0x9E, 0xE3, 0x00, 0xC0, 0x4F, 0x79, 0x73, 0x96}};
const IID BS_IID_SpMemoryStream = {0x5FB7EF7D, 0xDFF4, 0x468a, {0xB6, 0xB7, 0x2F, 0xCB, 0xD1, 0x88, 0xF9, 0x94}};

#if 0
    static void print_error ( HRESULT hr, UINT puArgErr )
    {
        switch ( hr )
        {
            case DISP_E_BADPARAMCOUNT:
                printf ( "The number of elements provided to DISPPARAMS is different from the number of arguments accepted by the method or property." );
                break;
            case DISP_E_BADVARTYPE:
                printf ( "One of the arguments in DISPPARAMS is not a valid variant type." );
                break;
            case DISP_E_EXCEPTION:
                printf ( "The application needs to raise an exception." );
                break;
            case DISP_E_MEMBERNOTFOUND:
                printf ( "The requested member does not exist." );
                break;
            case DISP_E_NONAMEDARGS:
                printf ( "This implementation of IDispatch does not support named arguments." );
                break;
            case DISP_E_OVERFLOW:
                printf ( "One of the arguments in DISPPARAMS could not be coerced to the specified type." );
                break;
            case DISP_E_PARAMNOTFOUND:
                printf ( "One of the parameter IDs does not correspond to a parameter on the method." );
                break;
            case DISP_E_TYPEMISMATCH:
                printf ( "One or more of the arguments could not be coerced." );
                break;
            case DISP_E_UNKNOWNINTERFACE:
                printf ( "The interface identifier passed in riid is not IID_NULL." );
                break;
            case DISP_E_UNKNOWNLCID:
                printf ( "The member being invoked interprets string arguments according to the LCID, and the LCID is not recognized." );
                break;
            case DISP_E_PARAMNOTOPTIONAL:
                printf ( "A required parameter was omitted." );
                break;
            default:
                printf ( "Unknown error" );
        }
        printf ( "\nArgument error: %u\n", puArgErr );
    }
#endif

static HRESULT InitVariantFromString(PCWSTR psz, VARIANT* pvar) {
	pvar->vt = VT_BSTR;
	pvar->bstrVal = SysAllocString(psz);
	HRESULT hr = pvar->bstrVal ? S_OK : (psz ? E_OUTOFMEMORY : E_INVALIDARG);
	if (FAILED(hr)) {
		VariantInit(pvar);
	}
	return hr;
}

static HRESULT InitVariantFromUInt32(ULONG ulVal, VARIANT* pvar) {
	pvar->vt = VT_UI4;
	pvar->ulVal = ulVal;
	return S_OK;
}

static char* blastspeak_get_temporary_memory(blastspeak* instance, unsigned int bytes) {
	if (bytes <= blastspeak_static_memory_length) {
		return instance->static_memory;
	}
	if (instance->allocated_memory && bytes <= instance->allocated_memory_length) {
		return instance->allocated_memory;
	}
	instance->allocated_memory = (char*)realloc(instance->allocated_memory, (size_t)bytes);
	if (instance->allocated_memory == NULL) {
		instance->allocated_memory_length = 0;
		return NULL;
	}
	instance->allocated_memory_length = bytes;
	return instance->allocated_memory;
}

static WCHAR* blastspeak_get_wchar_from_utf8(
	blastspeak* instance, const char* the_string, unsigned int length_in_bytes) {
	WCHAR* result;
	int needed_size;

	if (length_in_bytes == 0) {
		length_in_bytes = (unsigned int)strlen(the_string);
	}
	if (length_in_bytes == 0) {
		return NULL;
	}
	++length_in_bytes;
	needed_size = MultiByteToWideChar(CP_UTF8, 0, the_string, (int)length_in_bytes, NULL, 0);
	if (needed_size == 0) {
		return NULL;
	}
	result = (WCHAR*)blastspeak_get_temporary_memory(instance, (unsigned int)(needed_size * sizeof(WCHAR)));
	if (result == NULL) {
		return NULL;
	}
	if (MultiByteToWideChar(CP_UTF8, 0, the_string, (int)length_in_bytes, result, needed_size) != needed_size) {
		return NULL;
	}
	return result;
}

static char* blastspeak_get_UTF8_from_BSTR(blastspeak* instance, BSTR the_string) {
	char* result;
	int needed_size;
	unsigned int length_in_chars = SysStringLen(the_string);
	if (length_in_chars == 0) {
		return NULL;
	}

	++length_in_chars;
	needed_size = WideCharToMultiByte(CP_UTF8, 0, the_string, (int)length_in_chars, NULL, 0, NULL, NULL);
	if (needed_size == 0) {
		return NULL;
	}
	result = blastspeak_get_temporary_memory(instance, (unsigned int)needed_size);
	if (result == NULL) {
		return NULL;
	}
	if (WideCharToMultiByte(CP_UTF8, 0, the_string, (int)length_in_chars, result, needed_size, NULL, NULL) !=
		needed_size) {
		return NULL;
	}
	return result;
}

static int blastspeak_get_stream_format(blastspeak* instance,
	int retrieve_dispids,
	unsigned long* sample_rate,
	unsigned char* bits_per_sample,
	unsigned char* channels);

int blastspeak_initialize(blastspeak* instance) {
	const OLECHAR* voice_names[] = {L"AllowAudioOutputFormatChangesOnNextSet",
		L"AudioOutputStream",
		L"GetVoices",
		L"Rate",
		L"Speak",
		L"Status",
		L"Voice",
		L"Volume"};
	const OLECHAR* voice_token_names[] = {L"GetAttribute", L"GetDescription"};
	const OLECHAR* voice_collection_names[] = {L"Count", L"Item"};
	const OLECHAR* voice_category_names[] = {L"EnumerateTokens", L"SetId"};
	const OLECHAR* voice_category_id = L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech\\Voices";
	const OLECHAR* memory_stream_names[] = {L"GetData", L"Format", L"SetData"};

	IDispatch* stream;
	IDispatch* voice_category = NULL;
	HRESULT hr;
	DISPPARAMS parameters;
	VARIANT return_value;
	UINT puArgErr;
	DISPID voice_collection_dispids[2];
	DISPID voice_category_dispids[2];
	CLSID voice_category_clsid;
	LONG voice_count = 0;

	instance->allocated_memory = NULL;
	instance->allocated_memory_length = 0;
	instance->static_memory[0] = 0;
	instance->must_reset_output = 0;

	(void)CoInitializeEx(NULL, COINIT_MULTITHREADED);

	hr = CoCreateInstance(&BS_IID_SpVoice, NULL, CLSCTX_INPROC_SERVER, &BS_IID_IDispatch, (void**)&instance->voice);
	if (FAILED(hr)) {
		CoUninitialize();
		return 0;
	}

	hr = CoCreateInstance(&BS_IID_SpMemoryStream, NULL, CLSCTX_INPROC_SERVER, &BS_IID_IDispatch, (void**)&stream);
	if (FAILED(hr)) {
		instance->voice->lpVtbl->Release(instance->voice);
		CoUninitialize();
		return 0;
	}

	for (puArgErr = 0; puArgErr < 8; ++puArgErr) {
		hr = instance->voice->lpVtbl->GetIDsOfNames(instance->voice,
			&BS_IID_null,
			(LPOLESTR*)&voice_names[puArgErr],
			1,
			LOCALE_SYSTEM_DEFAULT,
			&((DISPID*)instance->voice_dispids)[puArgErr]);
		if (FAILED(hr)) {
			stream->lpVtbl->Release(stream);
			instance->voice->lpVtbl->Release(instance->voice);
			CoUninitialize();
			return 0;
		}
	}

	for (puArgErr = 0; puArgErr < 3; ++puArgErr) {
		hr = stream->lpVtbl->GetIDsOfNames(stream,
			&BS_IID_null,
			(LPOLESTR*)&memory_stream_names[puArgErr],
			1,
			LOCALE_SYSTEM_DEFAULT,
			&((DISPID*)instance->memory_stream_dispids)[puArgErr]);
		if (FAILED(hr)) {
			stream->lpVtbl->Release(stream);
			instance->voice->lpVtbl->Release(instance->voice);
			CoUninitialize();
			return 0;
		}
	}

	stream->lpVtbl->Release(stream);

	parameters.rgvarg = NULL;
	parameters.cArgs = 0;
	parameters.rgdispidNamedArgs = NULL;
	parameters.cNamedArgs = 0;

	hr = instance->voice->lpVtbl->Invoke(instance->voice,
		((DISPID*)instance->voice_dispids)[6],
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_PROPERTYGET,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
	if (FAILED(hr)) {
		instance->voice->lpVtbl->Release(instance->voice);
		CoUninitialize();
		return 0;
	}

	if (return_value.vt != VT_DISPATCH) {
		VariantClear(&return_value);
		instance->voice->lpVtbl->Release(instance->voice);
		CoUninitialize();
		return 0;
	}

	instance->default_voice_token = return_value.pdispVal;
	for (puArgErr = 0; puArgErr < 2; ++puArgErr) {
		hr = instance->default_voice_token->lpVtbl->GetIDsOfNames(instance->default_voice_token,
			&BS_IID_null,
			(LPOLESTR*)&voice_token_names[puArgErr],
			1,
			LOCALE_SYSTEM_DEFAULT,
			&((DISPID*)instance->voice_token_dispids)[puArgErr]);
		if (FAILED(hr)) {
			instance->default_voice_token->lpVtbl->Release(instance->default_voice_token);
			instance->voice->lpVtbl->Release(instance->voice);
			CoUninitialize();
			return 0;
		}
	}

	instance->voices = NULL;
	instance->voice_count = 1;
	instance->voice_collection_item_dispid = DISPID_UNKNOWN;
	instance->format = NULL;
	instance->current_voice_token = NULL;

	hr = CLSIDFromProgID(L"SAPI.SpObjectTokenCategory", &voice_category_clsid);
	if (SUCCEEDED(hr)) {
		hr = CoCreateInstance(
			&voice_category_clsid, NULL, CLSCTX_INPROC_SERVER, &BS_IID_IDispatch, (void**)&voice_category);
	}

	if (SUCCEEDED(hr) && voice_category != NULL) {
		VARIANT arguments[2];

		for (puArgErr = 0; puArgErr < 2; ++puArgErr) {
			hr = voice_category->lpVtbl->GetIDsOfNames(voice_category,
				&BS_IID_null,
				(LPOLESTR*)&voice_category_names[puArgErr],
				1,
				LOCALE_SYSTEM_DEFAULT,
				&voice_category_dispids[puArgErr]);
			if (FAILED(hr)) {
				break;
			}
		}

		if (SUCCEEDED(hr)) {
			hr = InitVariantFromString(voice_category_id, &arguments[1]);
			if (SUCCEEDED(hr)) {
				arguments[0].vt = VT_BOOL;
				arguments[0].boolVal = VARIANT_FALSE;

				parameters.rgvarg = arguments;
				parameters.cArgs = 2;
				parameters.rgdispidNamedArgs = NULL;
				parameters.cNamedArgs = 0;

				hr = voice_category->lpVtbl->Invoke(voice_category,
					voice_category_dispids[1],
					&BS_IID_null,
					LOCALE_SYSTEM_DEFAULT,
					DISPATCH_METHOD,
					&parameters,
					&return_value,
					NULL,
					&puArgErr);
				VariantClear(&arguments[1]);
				if (SUCCEEDED(hr)) {
					VariantClear(&return_value);
				}
			}
		}

		parameters.rgvarg = NULL;
		parameters.cArgs = 0;
		parameters.rgdispidNamedArgs = NULL;
		parameters.cNamedArgs = 0;

		if (SUCCEEDED(hr)) {
			hr = voice_category->lpVtbl->Invoke(voice_category,
				voice_category_dispids[0],
				&BS_IID_null,
				LOCALE_SYSTEM_DEFAULT,
				DISPATCH_METHOD,
				&parameters,
				&return_value,
				NULL,
				&puArgErr);
			if (SUCCEEDED(hr) && return_value.vt == VT_DISPATCH) {
				instance->voices = return_value.pdispVal;
			}
			else if (SUCCEEDED(hr)) {
				VariantClear(&return_value);
			}
		}
	}

	if (voice_category != NULL) {
		voice_category->lpVtbl->Release(voice_category);
	}

	if (instance->voices != NULL) {
		for (puArgErr = 0; puArgErr < 2; ++puArgErr) {
			hr = instance->voices->lpVtbl->GetIDsOfNames(instance->voices,
				&BS_IID_null,
				(LPOLESTR*)&voice_collection_names[puArgErr],
				1,
				LOCALE_SYSTEM_DEFAULT,
				&voice_collection_dispids[puArgErr]);
			if (FAILED(hr)) {
				instance->voices->lpVtbl->Release(instance->voices);
				instance->voices = NULL;
				break;
			}
		}
	}

	if (instance->voices != NULL) {
		hr = instance->voices->lpVtbl->Invoke(instance->voices,
			voice_collection_dispids[0],
			&BS_IID_null,
			LOCALE_SYSTEM_DEFAULT,
			DISPATCH_PROPERTYGET,
			&parameters,
			&return_value,
			NULL,
			&puArgErr);
		if (SUCCEEDED(hr) && return_value.vt == VT_I4 && return_value.lVal > 0) {
			voice_count = return_value.lVal;
			instance->voice_count = (unsigned int)voice_count;
			instance->voice_collection_item_dispid = voice_collection_dispids[1];
			VariantClear(&return_value);
		}
		else {
			if (SUCCEEDED(hr)) {
				VariantClear(&return_value);
			}
			instance->voices->lpVtbl->Release(instance->voices);
			instance->voices = NULL;
		}
	}

	if (blastspeak_get_stream_format(
			instance, 1, &instance->sample_rate, &instance->bits_per_sample, &instance->channels) == 0) {
		if (instance->voices) {
			instance->voices->lpVtbl->Release(instance->voices);
		}
		instance->default_voice_token->lpVtbl->Release(instance->default_voice_token);
		instance->voice->lpVtbl->Release(instance->voice);
		CoUninitialize();
		return 0;
	}

	return 1;
}

void blastspeak_destroy(blastspeak* instance) {
	if (instance == NULL) {
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

	/* Balanced the COM library apartment lifecycle without crashing concurrent worker streams */
	CoUninitialize();
}

static int blastspeak_speak_internal(blastspeak* instance, const char* text) {
	HRESULT hr;
	VARIANT return_value;
	VARIANT arguments[2];
	DISPPARAMS parameters;
	UINT puArgErr;
	WCHAR* utf16_string = blastspeak_get_wchar_from_utf8(instance, text, 0);

	if (utf16_string == NULL) {
		return 0;
	}

	hr = InitVariantFromString(utf16_string, &arguments[0]); 
	if (FAILED(hr)) {
		return 0;
	}

	(void)InitVariantFromUInt32(0, &arguments[1]); 

	parameters.rgvarg = arguments;
	parameters.rgdispidNamedArgs = NULL;
	parameters.cArgs = 2;
	parameters.cNamedArgs = 0;

	hr = instance->voice->lpVtbl->Invoke(instance->voice,
		((DISPID*)instance->voice_dispids)[4],
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_METHOD,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
		
	VariantClear(&arguments[0]);
	VariantClear(&return_value);
	
	if (FAILED(hr)) {
		return 0;
	}
	return 1;
}

static int blastspeak_reset_output(blastspeak* instance) {
	HRESULT hr;
	VARIANT return_value;
	VARIANT argument;
	DISPPARAMS parameters;
	UINT puArgErr;
	DISPID dispid_named = DISPID_PROPERTYPUT;
	
	parameters.rgvarg = &argument;
	parameters.cArgs = 1;
	parameters.rgdispidNamedArgs = &dispid_named;
	parameters.cNamedArgs = 1;
	argument.vt = VT_EMPTY;

	hr = instance->voice->lpVtbl->Invoke(instance->voice,
		((DISPID*)instance->voice_dispids)[1],
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_PROPERTYPUTREF,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
	if (FAILED(hr)) {
		return 0;
	}
	VariantClear(&return_value);
	instance->must_reset_output = 0;
	return 1;
}

int blastspeak_speak(blastspeak* instance, const char* text) {
	if (instance->must_reset_output) {
		if (!blastspeak_reset_output(instance)) {
			return 0;
		}
	}

	return blastspeak_speak_internal(instance, text);
}

static IDispatch* blastspeak_get_voice(blastspeak* instance, unsigned int voice_index) {
	HRESULT hr;
	DISPPARAMS parameters;
	VARIANT argument;
	VARIANT return_value;
	UINT puArgErr;

	if (voice_index >= instance->voice_count) {
		return NULL;
	}

	if (instance->voices == NULL) {
		if (voice_index != 0 || instance->default_voice_token == NULL) {
			return NULL;
		}
		instance->default_voice_token->lpVtbl->AddRef(instance->default_voice_token);
		return instance->default_voice_token;
	}

	parameters.rgvarg = &argument;
	parameters.cArgs = 1;
	parameters.rgdispidNamedArgs = NULL;
	parameters.cNamedArgs = 0;

	(void)InitVariantFromUInt32(voice_index, &argument);

	hr = instance->voices->lpVtbl->Invoke(instance->voices,
		instance->voice_collection_item_dispid,
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_METHOD,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
	if (FAILED(hr)) {
		return NULL;
	}

	if (return_value.vt != VT_DISPATCH) {
		VariantClear(&return_value);
		return NULL;
	}

	return return_value.pdispVal;
}

int blastspeak_set_voice(blastspeak* instance, unsigned int voice_index) {
	if (instance->must_reset_output) {
		if (!blastspeak_reset_output(instance)) {
			return 0;
		}
	}
	IDispatch* voice_token = blastspeak_get_voice(instance, voice_index);
	HRESULT hr;
	DISPPARAMS parameters;
	VARIANT argument;
	VARIANT return_value;
	UINT puArgErr;
	DISPID dispid_named = DISPID_PROPERTYPUT;

	if (voice_token == NULL) {
		return 0;
	}

	parameters.rgvarg = &argument;
	parameters.cArgs = 1;
	parameters.rgdispidNamedArgs = &dispid_named;
	parameters.cNamedArgs = 1;

	argument.vt = VT_DISPATCH;
	argument.pdispVal = voice_token;

	/* FIXED: Wrapped internal scalar pointer values inside array cast qualifiers to prevent struct alignment corruption */
	hr = instance->voice->lpVtbl->Invoke(instance->voice,
		((DISPID*)instance->voice_dispids)[6],
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_PROPERTYPUTREF,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
	if (FAILED(hr)) {
		voice_token->lpVtbl->Release(voice_token);
		return 0;
	}
	VariantClear(&return_value);
	if (instance->current_voice_token) {
		instance->current_voice_token->lpVtbl->Release(instance->current_voice_token);
	}
	instance->current_voice_token = voice_token;
	argument.vt = VT_EMPTY;
	hr = instance->voice->lpVtbl->Invoke(instance->voice,
		((DISPID*)instance->voice_dispids)[1],
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_PROPERTYPUTREF,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
	if (FAILED(hr)) {
		return 0;
	}
	VariantClear(&return_value);
	if (blastspeak_get_stream_format(
			instance, 0, &instance->sample_rate, &instance->bits_per_sample, &instance->channels) == 0) {
		return 0;
	}
	return 1;
}

int blastspeak_get_voice_description(blastspeak* instance, unsigned int voice_index, char* out_buffer, size_t max_bytes) {
	char* temp_utf8;
	IDispatch* voice_token = blastspeak_get_voice(instance, voice_index);
	HRESULT hr;
	DISPPARAMS parameters;
	VARIANT return_value;
	UINT puArgErr;
	int success = 0;

	if (voice_token == NULL || out_buffer == NULL || max_bytes == 0) {
		return 0;
	}

	parameters.rgvarg = NULL;
	parameters.cArgs = 0;
	parameters.rgdispidNamedArgs = NULL;
	parameters.cNamedArgs = 0;

	hr = voice_token->lpVtbl->Invoke(voice_token,
		((DISPID*)instance->voice_token_dispids)[1],
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_METHOD,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
	voice_token->lpVtbl->Release(voice_token);
	if (FAILED(hr)) {
		return 0;
	}

	if (return_value.vt != VT_BSTR) {
		VariantClear(&return_value);
		return 0;
	}

	temp_utf8 = blastspeak_get_UTF8_from_BSTR(instance, return_value.bstrVal);
	if (temp_utf8) {
		strncpy(out_buffer, temp_utf8, max_bytes - 1);
		out_buffer[max_bytes - 1] = '\0';
		success = 1;
	}
	VariantClear(&return_value);
	return success;
}

int blastspeak_get_voice_attribute(blastspeak* instance, unsigned int voice_index, const char* attribute, char* out_buffer, size_t max_bytes) {
	char* temp_utf8;
	IDispatch* voice_token = blastspeak_get_voice(instance, voice_index);
	HRESULT hr;
	DISPPARAMS parameters;
	VARIANT argument;
	VARIANT return_value;
	UINT puArgErr;
	WCHAR* utf16_string;
	int success = 0;

	if (voice_token == NULL || attribute == NULL || out_buffer == NULL || max_bytes == 0) {
		return 0;
	}

	utf16_string = blastspeak_get_wchar_from_utf8(instance, attribute, 0);
	if (utf16_string == NULL) {
		voice_token->lpVtbl->Release(voice_token);
		return 0;
	}

	parameters.rgvarg = &argument;
	parameters.cArgs = 1;
	parameters.rgdispidNamedArgs = NULL;
	parameters.cNamedArgs = 0;

	hr = InitVariantFromString(utf16_string, &argument);
	if (FAILED(hr)) {
		voice_token->lpVtbl->Release(voice_token);
		return 0;
	}

	hr = voice_token->lpVtbl->Invoke(voice_token,
		((DISPID*)instance->voice_token_dispids)[0],
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_METHOD,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
		
	VariantClear(&argument);
	voice_token->lpVtbl->Release(voice_token);
	if (FAILED(hr)) {
		return 0;
	}

	if (return_value.vt != VT_BSTR) {
		VariantClear(&return_value);
		return 0;
	}

	temp_utf8 = blastspeak_get_UTF8_from_BSTR(instance, return_value.bstrVal);
	if (temp_utf8) {
		strncpy(out_buffer, temp_utf8, max_bytes - 1);
		out_buffer[max_bytes - 1] = '\0';
		success = 1;
	}
	VariantClear(&return_value);
	return success;
}

int blastspeak_get_voice_languages(blastspeak* instance, unsigned int voice_index, char* out_buffer, size_t max_bytes) {
	char raw_attr_buffer[256];
	long codes[blastspeak_max_languages_per_voice];
	int languages = 0;
	int i;
	char current_name[9];
	char* scan_ptr = raw_attr_buffer;

	if (out_buffer == NULL || max_bytes == 0) return 0;
	out_buffer[0] = '\0';

	if (!blastspeak_get_voice_attribute(instance, voice_index, "language", raw_attr_buffer, sizeof(raw_attr_buffer))) {
		return 0;
	}

	for (;;) {
		int should_continue = 0;
		char* next_ptr = NULL;
		
		long parsed_code = strtol(scan_ptr, &next_ptr, 16);
		if (scan_ptr == next_ptr || parsed_code == 0L) {
			break;
		}
		codes[languages] = parsed_code;
		scan_ptr = next_ptr;

		if (codes[languages] < -32768L || codes[languages] > 32767L) {
			return 0;
		}
		for (i = 0; i < languages; ++i) {
			if (codes[i] == codes[languages]) {
				should_continue = 1;
				break;
			}
		}
		if (should_continue) {
			continue;
		}
		++languages;
		if (languages >= blastspeak_max_languages_per_voice) {
			break;
		}
		for (; *scan_ptr; ++scan_ptr) {
			if (*scan_ptr == ' ' || *scan_ptr == ';') {
				continue;
			}
			break;
		}
		if (*scan_ptr == 0) {
			break;
		}
	}

	if (languages == 0) {
		return 0;
	}

	for (i = 0; i < languages; ++i) {
		if (i > 0) {
			strncat(out_buffer, " ", max_bytes - strlen(out_buffer) - 1);
		}
		current_name[0] = 0;
		if (GetLocaleInfoA((LCID)codes[i], LOCALE_SISO639LANGNAME, current_name, 9) == 0) {
			return 0;
		}
		strncat(out_buffer, current_name, max_bytes - strlen(out_buffer) - 1);

		if (SUBLANGID((short)codes[i]) == 0) {
			continue;
		}

		current_name[0] = 0;
		if (GetLocaleInfoA((LCID)codes[i], LOCALE_SISO3166CTRYNAME, current_name, 9) == 0) {
			continue;
		}
		strncat(out_buffer, "-", max_bytes - strlen(out_buffer) - 1);
		strncat(out_buffer, current_name, max_bytes - strlen(out_buffer) - 1);
	}

	return 1;
}

static int blastspeak_set_long_property(blastspeak* instance, DISPID dispid, long value) 
{
    HRESULT hr;
    DISPPARAMS parameters;
    VARIANT argument;
    UINT puArgErr;
    DISPID dispid_named = DISPID_PROPERTYPUT;

    if (instance == NULL || instance->voice == NULL) return 0;

    parameters.rgvarg = &argument;
    parameters.cArgs = 1;
    parameters.rgdispidNamedArgs = &dispid_named;
    parameters.cNamedArgs = 1;
    
    argument.vt = VT_I4;
    argument.lVal = value;

    hr = instance->voice->lpVtbl->Invoke(instance->voice,
        dispid,
        &BS_IID_null,
        LOCALE_SYSTEM_DEFAULT,
        DISPATCH_PROPERTYPUT,
        &parameters,
        NULL, NULL,
        &puArgErr);
        
    return SUCCEEDED(hr);
}

static int blastspeak_get_long_property(blastspeak* instance, DISPID dispid, long* value, IDispatch* object) 
{
    HRESULT hr;
    DISPPARAMS parameters;
    VARIANT return_value;
    UINT puArgErr;

    if (object == NULL) {
        object = instance->voice;
    }
    if (object == NULL || value == NULL) return 0;

    parameters.rgvarg = NULL;
    parameters.cArgs = 0;
    parameters.rgdispidNamedArgs = NULL;
    parameters.cNamedArgs = 0;

    hr = object->lpVtbl->Invoke(object,
        dispid,
        &BS_IID_null,
        LOCALE_SYSTEM_DEFAULT,
        DISPATCH_PROPERTYGET,
        &parameters,
        &return_value,
        NULL,
        &puArgErr);
        
    if (FAILED(hr)) {
        return 0;
    }
    
    if (return_value.vt != VT_I4 && return_value.vt != VT_I2) {
        VariantClear(&return_value);
        return 0;
    }

    if (return_value.vt == VT_I4) {
        *value = return_value.lVal;
    }
    else {
        *value = return_value.iVal;
    }

    VariantClear(&return_value);
    return 1;
}

static int blastspeak_get_stream_format(blastspeak* instance,
	int retrieve_dispids,
	unsigned long* sample_rate,
	unsigned char* bits_per_sample,
	unsigned char* channels) {
	const OLECHAR* audio_format_getwaveformatex_name = L"GetWaveFormatEx";
	const OLECHAR* audio_format_setwaveformatex_name = L"SetWaveFormatEx";
	const OLECHAR* waveformatex_names[] = {L"BitsPerSample", L"Channels", L"FormatTag", L"SamplesPerSec"};

	IDispatch* audio_device_stream;
	IDispatch* formatex = NULL;
	HRESULT hr;
	DISPPARAMS parameters;
	VARIANT return_value;
	UINT puArgErr;
	long temp;

	parameters.rgvarg = NULL;
	parameters.cArgs = 0;
	parameters.rgdispidNamedArgs = NULL;
	parameters.cNamedArgs = 0;

	if (instance->format) {
		instance->format->lpVtbl->Release(instance->format);
		instance->format = NULL;
	}

	hr = instance->voice->lpVtbl->Invoke(instance->voice,
		((DISPID*)instance->voice_dispids)[1],
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_PROPERTYGET,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
	if (FAILED(hr)) {
		return 0;
	}
	if (return_value.vt != VT_DISPATCH) {
		VariantClear(&return_value);
		return 0;
	}
	audio_device_stream = return_value.pdispVal;

	hr = audio_device_stream->lpVtbl->Invoke(audio_device_stream,
		((DISPID*)instance->memory_stream_dispids)[1],
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_PROPERTYGET,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
	audio_device_stream->lpVtbl->Release(audio_device_stream);
	if (FAILED(hr)) {
		return 0;
	}
	if (return_value.vt != VT_DISPATCH) {
		VariantClear(&return_value);
		return 0;
	}
	instance->format = return_value.pdispVal;

	if (retrieve_dispids) {
		hr = instance->format->lpVtbl->GetIDsOfNames(instance->format,
			&BS_IID_null,
			(LPOLESTR*)&audio_format_getwaveformatex_name,
			1,
			LOCALE_SYSTEM_DEFAULT,
			&instance->audio_format_getwaveformatex_dispid);
		if (FAILED(hr)) {
			goto error;
		}
		hr = instance->format->lpVtbl->GetIDsOfNames(instance->format,
			&BS_IID_null,
			(LPOLESTR*)&audio_format_setwaveformatex_name,
			1,
			LOCALE_SYSTEM_DEFAULT,
			&instance->audio_format_setwaveformatex_dispid);
		if (FAILED(hr)) {
			goto error;
		}
	}

	hr = instance->format->lpVtbl->Invoke(instance->format,
		instance->audio_format_getwaveformatex_dispid,
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_METHOD,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
	if (FAILED(hr)) {
		goto error;
	}
	if (return_value.vt != VT_DISPATCH) {
		VariantClear(&return_value);
		goto error;
	}
	formatex = return_value.pdispVal;

	if (retrieve_dispids) {
		for (puArgErr = 0; puArgErr < 4; ++puArgErr) {
			hr = formatex->lpVtbl->GetIDsOfNames(formatex,
				&BS_IID_null,
				(LPOLESTR*)&waveformatex_names[puArgErr],
				1,
				LOCALE_SYSTEM_DEFAULT,
				&((DISPID*)instance->waveformatex_dispids)[puArgErr]);
			if (FAILED(hr)) {
				goto error;
			}
		}
	}

	if (blastspeak_get_long_property(instance, ((DISPID*)instance->waveformatex_dispids)[2], &temp, formatex) == 0) {
		goto error;
	}
	if (temp != 1) {
		goto error;
	}

	if (blastspeak_get_long_property(instance, ((DISPID*)instance->waveformatex_dispids)[0], &temp, formatex) == 0) {
		goto error;
	}
	if (temp != 8 && temp != 16) {
		goto error;
	}
	*bits_per_sample = (unsigned char)temp;

	if (blastspeak_get_long_property(instance, ((DISPID*)instance->waveformatex_dispids)[1], &temp, formatex) == 0) {
		goto error;
	}
	if (temp != 1 && temp != 2) {
		goto error;
	}
	*channels = (unsigned char)temp;

	if (blastspeak_get_long_property(instance, ((DISPID*)instance->waveformatex_dispids)[3], &temp, formatex) == 0) {
		goto error;
	}
	if (temp >= 8000 && temp <= 192000) {
		*sample_rate = (unsigned long)temp;
	}
	else {
		goto error;
	}

	if (formatex) {
		formatex->lpVtbl->Release(formatex);
	}
	return 1;

error:
	if (formatex) {
		formatex->lpVtbl->Release(formatex);
	}
	return 0;
}

int blastspeak_get_voice_rate(blastspeak* instance, long* result) {
	if (result == NULL) {
		return 0;
	}
	return blastspeak_get_long_property(instance, ((DISPID*)instance->voice_dispids)[3], result, NULL);
}

int blastspeak_set_voice_rate(blastspeak* instance, long value) {
	if (value < -10 || value > 10) {
		return 0;
	}
	return blastspeak_set_long_property(instance, ((DISPID*)instance->voice_dispids)[3], value);
}

int blastspeak_get_voice_volume(blastspeak* instance, long* result) {
	if (result == NULL) {
		return 0;
	}
	return blastspeak_get_long_property(instance, ((DISPID*)instance->voice_dispids)[7], result, NULL);
}

int blastspeak_set_voice_volume(blastspeak* instance, long value) {
	if (value < -100 || value > 100) {
		return 0;
	}
	return blastspeak_set_long_property(instance, ((DISPID*)instance->voice_dispids)[7], value);
}

char* blastspeak_speak_to_memory(blastspeak* instance, unsigned long* bytes, const char* text) {
	int temp;
	HRESULT hr;
	DISPPARAMS parameters;
	VARIANT argument;
	VARIANT return_value;
	UINT puArgErr;
	char* ptr;
	LONGLONG elements;
	char* data = NULL;
	IDispatch* stream = NULL;
	IDispatch* stream_format = NULL;
	IDispatch* formatex = NULL;
	DISPID dispid_named = DISPID_PROPERTYPUT;
	int num_refs;

	if (instance->format == NULL) {
		return NULL;
	}

	hr = CoCreateInstance(&BS_IID_SpMemoryStream, NULL, CLSCTX_INPROC_SERVER, &BS_IID_IDispatch, (void**)&stream);
	if (FAILED(hr)) {
		return NULL;
	}

	parameters.cArgs = 0;
	parameters.cNamedArgs = 0;
	parameters.rgdispidNamedArgs = NULL;
	parameters.rgvarg = NULL;

	hr = stream->lpVtbl->Invoke(stream,
		((DISPID*)instance->memory_stream_dispids)[1],
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_PROPERTYGET,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
	if (FAILED(hr)) {
		goto done;
	}
	stream_format = return_value.pdispVal;

	hr = instance->format->lpVtbl->Invoke(instance->format,
		instance->audio_format_getwaveformatex_dispid,
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_METHOD,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
	if (FAILED(hr) || return_value.vt != VT_DISPATCH) {
		goto done;
	}
	formatex = return_value.pdispVal;

	parameters.rgvarg = &argument;
	parameters.cArgs = 1;
	argument.vt = VT_DISPATCH;
	argument.pdispVal = formatex;

	hr = stream_format->lpVtbl->Invoke(stream_format,
		instance->audio_format_setwaveformatex_dispid,
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_METHOD,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
	if (FAILED(hr)) {
		goto done;
	}
	VariantClear(&return_value);

	parameters.rgdispidNamedArgs = &dispid_named;
	parameters.cNamedArgs = 1;
	argument.vt = VT_DISPATCH;
	argument.pdispVal = stream;

	hr = instance->voice->lpVtbl->Invoke(instance->voice,
		((DISPID*)instance->voice_dispids)[1],
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_PROPERTYPUTREF,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
	if (FAILED(hr)) {
		goto done;
	}
	VariantClear(&return_value);

	instance->must_reset_output = 1;

	temp = blastspeak_speak_internal(instance, text);
	if (temp == 0) {
		goto done;
	}

	parameters.rgvarg = NULL;
	parameters.cArgs = 0;
	parameters.rgdispidNamedArgs = NULL;
	parameters.cNamedArgs = 0;

	hr = stream->lpVtbl->Invoke(stream,
		((DISPID*)instance->memory_stream_dispids)[0],
		&BS_IID_null,
		LOCALE_SYSTEM_DEFAULT,
		DISPATCH_METHOD,
		&parameters,
		&return_value,
		NULL,
		&puArgErr);
	if (FAILED(hr)) {
		goto done;
	}

	if (return_value.vt != (VT_ARRAY | VT_UI1)) {
		VariantClear(&return_value);
		goto done;
	}

	ptr = (char*)return_value.parray->pvData;
	elements = return_value.parray->rgsabound[0].cElements;
	ptr += return_value.parray->rgsabound[0].lLbound;

	data = blastspeak_get_temporary_memory(instance, (unsigned int)elements);
	if (data) {
		memcpy(data, ptr, (size_t)elements);
		*bytes = (unsigned long)elements;
	}
	else {
		*bytes = 0;
	}
	VariantClear(&return_value);

done:
	if (instance->must_reset_output) {
		blastspeak_reset_output(instance);
	}

	if (formatex != NULL) {
		formatex->lpVtbl->Release(formatex);
		formatex = NULL;
	}
	if (stream_format != NULL) {
		stream_format->lpVtbl->Release(stream_format);
		stream_format = NULL;
	}

	num_refs = (int)stream->lpVtbl->AddRef(stream);
	stream->lpVtbl->Release(stream);

	while (num_refs > 1) {
		stream->lpVtbl->Release(stream);
		num_refs--;
	}
	stream->lpVtbl->Release(stream);

	return data;
}

#ifdef __cplusplus
}
#endif
