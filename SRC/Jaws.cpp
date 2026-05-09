#ifdef _WIN32
#include "Encoding.h"
#include "Jaws.h"
#include <string>
namespace Sral {

	bool Jaws::Initialize() {
		HRESULT hr = CoCreateInstance(CLSID_JawsApi, NULL, CLSCTX_INPROC_SERVER, IID_IJawsApi, (void**)&pJawsApi);
		return (SUCCEEDED(hr) && pJawsApi != nullptr);
	}
	bool Jaws::Uninitialize() {
		if (pJawsApi) {
			pJawsApi->Release();
			pJawsApi = nullptr;
		}
		return true;
	}
	bool Jaws::GetActive() {
		return (!!FindWindowW(L"JFWUI2", nullptr)) && pJawsApi;
	}

	bool Jaws::Speak(const char* text, bool interrupt) {
		if (!GetActive())return false;
		if (interrupt)pJawsApi->StopSpeech();
		std::wstring str;
		UnicodeConvert(text, str);
		BSTR bstr = SysAllocString(str.c_str());
		if (!bstr) return false;
		VARIANT_BOOL result = VARIANT_FALSE;
		HRESULT hr = pJawsApi->SayString(bstr, interrupt ? VARIANT_TRUE : VARIANT_FALSE, &result);
		SysFreeString(bstr);
		return (SUCCEEDED(hr) && result == VARIANT_TRUE);
	}
	bool Jaws::Braille(const char* text) {
		if (!GetActive())return false;
		std::wstring wstr;
		UnicodeConvert(text, wstr);
		std::wstring::size_type i = wstr.find_first_of(L"\"");
		while (i != std::wstring::npos) {
			wstr[i] = L'\'';
			i = wstr.find_first_of(L"\"", i + 1);
		}
		BSTR bstr = SysAllocString(wstr.c_str());
		if (!bstr) return false;

		VARIANT_BOOL result = VARIANT_FALSE;
		HRESULT hr = pJawsApi->RunFunction(bstr, &result);
		SysFreeString(bstr);

		return (SUCCEEDED(hr) && result == VARIANT_TRUE);
	}

	bool Jaws::StopSpeech() {
		if (!GetActive())return false;
		return SUCCEEDED(pJawsApi->StopSpeech());
	}
}
#endif