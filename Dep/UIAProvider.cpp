/*************************************************************************************************
 *
 * Description: Declaration of the Provider class.
 *
 *  Copyright (C) Microsoft Corporation.  All rights reserved.
 *
 * This source code is intended only as a supplement to Microsoft
 * Development Tools and/or on-line documentation.  See these other
 * materials for detailed information regarding Microsoft code samples.
 *
 * THIS CODE AND INFORMATION ARE PROVIDED AS IS WITHOUT WARRANTY OF ANY
 * KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 *************************************************************************************************/

#include "UIAProvider.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>

namespace Sral {

Provider::Provider(HWND hwnd) noexcept : m_refCount(1), m_controlHWnd(hwnd) { }

IFACEMETHODIMP_(ULONG) Provider::AddRef()
{
    return static_cast<ULONG>(::InterlockedIncrement(&m_refCount));
}

IFACEMETHODIMP_(ULONG) Provider::Release()
{
    const LONG val = ::InterlockedDecrement(&m_refCount);
    if (val == 0)
    {
        delete this;
        return 0;
    }
    return static_cast<ULONG>(val);
}

IFACEMETHODIMP Provider::QueryInterface(REFIID riid, void** ppInterface)
{
    if (!ppInterface)
    {
        return E_POINTER;
    }

    if (riid == __uuidof(IUnknown))
    {
        *ppInterface = static_cast<IRawElementProviderSimple*>(this);
    }
    else if (riid == __uuidof(IRawElementProviderSimple))
    {
        *ppInterface = static_cast<IRawElementProviderSimple*>(this);
    }
    else if (riid == __uuidof(IInvokeProvider))
    {
        *ppInterface = static_cast<IInvokeProvider*>(this);
    }
    else
    {
        *ppInterface = nullptr;
        return E_NOINTERFACE;
    }

    static_cast<IUnknown*>(*ppInterface)->AddRef();
    return S_OK;
}

IFACEMETHODIMP Provider::get_ProviderOptions(ProviderOptions* pRetVal)
{
    if (!pRetVal) 
    {
        return E_POINTER;
    }
    
    *pRetVal = ProviderOptions_ServerSideProvider;
    return S_OK;
}

IFACEMETHODIMP Provider::GetPatternProvider(PATTERNID patternId, IUnknown** pRetVal)
{
    if (!pRetVal) 
    {
        return E_POINTER;
    }
    
    *pRetVal = nullptr;

    if (patternId == UIA_InvokePatternId)
    {
        AddRef();
        *pRetVal = static_cast<IInvokeProvider*>(this);
    }
    
    return S_OK;
}

IFACEMETHODIMP Provider::GetPropertyValue(PROPERTYID propertyId, VARIANT* pRetVal)
{
    if (!pRetVal) [[unlikely]] 
    {
        return E_POINTER;
    }
    ::VariantInit(pRetVal);

    if (propertyId == UIA_ControlTypePropertyId)
    {
        V_VT(pRetVal) = VT_I4;
        V_I4(pRetVal) = UIA_ButtonControlTypeId;
    }
    else if (propertyId == UIA_NamePropertyId)
    {
        V_VT(pRetVal) = VT_BSTR;
        V_BSTR(pRetVal) = ::SysAllocString(L"ColorButton");

        if (!V_BSTR(pRetVal)) [[unlikely]]
        {
            V_VT(pRetVal) = VT_EMPTY;
            return E_OUTOFMEMORY;
        }
    }
    else
    {
        V_VT(pRetVal) = VT_EMPTY;
    }
    
    return S_OK;
}

IFACEMETHODIMP Provider::get_HostRawElementProvider(IRawElementProviderSimple** pRetVal)
{
    if (!pRetVal) 
    {
        return E_POINTER;
    }
    
    *pRetVal = nullptr;
    
    if (!m_controlHWnd)
    {
        return E_FAIL;
    }
    
    return ::UiaHostProviderFromHwnd(m_controlHWnd, pRetVal);
}

IFACEMETHODIMP Provider::Invoke()
{
    if (m_controlHWnd && ::IsWindow(m_controlHWnd))
    {
        ::PostMessageW(m_controlHWnd, WM_LBUTTONDOWN, 0, 0);
    }
    return S_OK;
}

}

#else

namespace Sral {

namespace Internal {
    [[maybe_unused]] inline void KeepProviderTranslationUnitAlive() noexcept {}
}

}

#endif