#include "UIA.h"
#include "Encoding.h"
#include "../Dep/UIAProvider.h"
#include <comdef.h>
#include <string>

namespace Sral {

bool Uia::Initialize() {
    IUIAutomationPtr pAutoInstance;
    HRESULT hr = pAutoInstance.CreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER);
    if (FAILED(hr)) {
        return false;
    }
    pAutomation = pAutoInstance;

    try {
        _variant_t varNameLocal(L"");
        IUIAutomationConditionPtr pCondInstance;
        
        hr = pAutomation->CreatePropertyConditionEx(
            UIA_NamePropertyId, 
            varNameLocal, 
            PropertyConditionFlags_None, 
            &pCondInstance
        );
        
        if (FAILED(hr)) {
            return false;
        }
        
        varName = varNameLocal;
        pCondition = pCondInstance;
    } catch (const _com_error&) {
        return false;
    }

    return true;
}

bool Uia::Uninitialize() {
    pProvider = nullptr;   
    pCondition = nullptr;
    pAutomation = nullptr;
    pElement = nullptr;
    varName.Clear();
    return true;
}

bool Uia::Speak(const char* text, bool interrupt) {
    if (!GetActive()) {
        return false;
    }

    const NotificationProcessing flags = interrupt 
        ? NotificationProcessing_ImportantMostRecent 
        : NotificationProcessing_ImportantAll;

    std::wstring str;
	if (!UnicodeConvert(text, str)) {
		return false;
	}

    pProvider = nullptr;
    pElement = nullptr;

    HWND foreground = GetForegroundWindow();
    if (!foreground) {
        return false;
    }

    Provider* pRawProvider = new (std::nothrow) Provider(foreground);
    if (!pRawProvider) {
        return false;
    }
    
    pProvider = pRawProvider;

    HRESULT hr = pAutomation->ElementFromHandle(foreground, &pElement);
    if (FAILED(hr)) {
        pProvider = nullptr;
        return false;
    }

    _bstr_t bstrText(str.c_str());
    _bstr_t bstrActivityId(L"");

    hr = UiaRaiseNotificationEvent(pProvider, NotificationKind_ActionCompleted, flags, bstrText, bstrActivityId);
    return SUCCEEDED(hr);
}

bool Uia::StopSpeech() {
    if (!GetActive()) {
        return false;
    }

    if (pProvider) {
        _bstr_t emptyText(L"");
        _bstr_t emptyActivityId(L"");
        HRESULT hr = UiaRaiseNotificationEvent(pProvider, NotificationKind_ActionCompleted, 
            NotificationProcessing_ImportantMostRecent, emptyText, emptyActivityId);
        return SUCCEEDED(hr);
	}
	return Speak("", true);
}

bool Uia::IsSpeaking() {
    return false;
}

bool Uia::GetActive() {
    if (!UiaClientsAreListening()) {
        return false;
    }

    BOOL screenReaderRunning = FALSE;
    BOOL result = SystemParametersInfo(SPI_GETSCREENREADER, 0, &screenReaderRunning, 0);
    return (result && screenReaderRunning);
}

} // namespace Sral
