#ifdef _WIN32
#include "Jaws.h"
#include "Encoding.h"
#include <comdef.h>
#include <string>
#include <string_view>

_COM_SMARTPTR_TYPEDEF(IJawsApi, __uuidof(IJawsApi));

namespace Sral {

[[nodiscard]] bool Jaws::Initialize() {
    IJawsApiPtr pJawsInstance;
    HRESULT hr = pJawsInstance.CreateInstance(CLSID_JawsApi, nullptr, CLSCTX_INPROC_SERVER);
    if (FAILED(hr)) {
        return false;
    }
    pJawsApi = pJawsInstance;
    return true;
}

[[nodiscard]] bool Jaws::Uninitialize() {
    pJawsApi = nullptr;
    return true;
}

[[nodiscard]] bool Jaws::GetActive() {
    return (FindWindowW(L"JFWUI2", nullptr) != nullptr) && (pJawsApi != nullptr);
}

[[nodiscard]] bool Jaws::Speak(const char* text, bool interrupt) {
    if (!GetActive()) {
        return false;
    }

    if (interrupt) {
        (void)pJawsApi->StopSpeech();
    }

    std::wstring str;
    if (!UnicodeConvert(text, str)) {
        return false;
    }
    
    _bstr_t bstrText(str.c_str());
    VARIANT_BOOL result = VARIANT_FALSE;
    const VARIANT_BOOL flush = interrupt ? VARIANT_TRUE : VARIANT_FALSE;

    const bool succeeded = SUCCEEDED(pJawsApi->SayString(bstrText, flush, &result));
    return (succeeded && result == VARIANT_TRUE);
}

[[nodiscard]] bool Jaws::Braille(const char* text) {
    if (!GetActive()) {
        return false;
    }

    std::wstring wstr;
    if (!UnicodeConvert(text, wstr)) {
        return false;
    }

    for (auto& ch : wstr) {
        if (ch == L'"') {
            ch = L'\'';
        }
    }

    wstr.insert(0, L"BrailleString(\"");
    wstr.append(L"\")");
    _bstr_t bstrCommand(wstr.c_str());
    VARIANT_BOOL result = VARIANT_FALSE;
    
    const bool succeeded = SUCCEEDED(pJawsApi->RunFunction(bstrCommand, &result));
    return (succeeded && result == VARIANT_TRUE);
}

[[nodiscard]] bool Jaws::StopSpeech() {
    if (!GetActive()) {
        return false;
    }
    return SUCCEEDED(pJawsApi->StopSpeech());
}

[[nodiscard]] bool Jaws::IsSpeaking() {
    return false;
}

} // namespace Sral
#endif
