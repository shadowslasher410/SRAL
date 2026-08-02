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

Provider::Provider(HWND const hwnd) noexcept : m_refCount(1), m_controlHWnd(hwnd) {}

IFACEMETHODIMP_(ULONG) Provider::AddRef() noexcept {
	return static_cast<ULONG>(::InterlockedIncrement(&m_refCount));
}

IFACEMETHODIMP_(ULONG) Provider::Release() noexcept {
	const LONG val = ::InterlockedDecrement(&m_refCount);
	if (val == 0) {
		delete this;
		return 0;
	}
	return static_cast<ULONG>(val);
}

IFACEMETHODIMP Provider::QueryInterface(REFIID riid, void** const ppInterface) noexcept {
	if (!ppInterface) [[unlikely]] {
		return E_POINTER;
	}

	if (riid == __uuidof(IUnknown) || riid == __uuidof(IRawElementProviderSimple)) {
		*ppInterface = static_cast<IRawElementProviderSimple*>(this);
	}
	else if (riid == __uuidof(IInvokeProvider)) {
		*ppInterface = static_cast<IInvokeProvider*>(this);
	}
	else {
		*ppInterface = nullptr;
		return E_NOINTERFACE;
	}

	static_cast<IUnknown*>(*ppInterface)->AddRef();
	return S_OK;
}

IFACEMETHODIMP Provider::get_ProviderOptions(ProviderOptions* const pRetVal) noexcept {
	if (!pRetVal) [[unlikely]] {
		return E_POINTER;
	}

	*pRetVal = ProviderOptions_ServerSideProvider;
	return S_OK;
}

IFACEMETHODIMP Provider::GetPatternProvider(PATTERNID const patternId, IUnknown** const pRetVal) noexcept {
	if (!pRetVal) [[unlikely]] {
		return E_POINTER;
	}

	*pRetVal = nullptr;

	if (patternId == UIA_InvokePatternId) {
		AddRef();
		*pRetVal = static_cast<IInvokeProvider*>(this);
	}

	return S_OK;
}

IFACEMETHODIMP Provider::GetPropertyValue(PROPERTYID const propertyId, VARIANT* const pRetVal) noexcept {
	if (!pRetVal) [[unlikely]] {
		return E_POINTER;
	}
	::VariantInit(pRetVal);

	switch (propertyId) {
	case UIA_ControlTypePropertyId: {
		V_VT(pRetVal) = VT_I4;
		V_I4(pRetVal) = UIA_ButtonControlTypeId;
		break;
	}
	case UIA_NamePropertyId: {
		V_VT(pRetVal) = VT_BSTR;
		V_BSTR(pRetVal) = ::SysAllocString(L"ColorButton");
		if (!V_BSTR(pRetVal)) [[unlikely]] {
			V_VT(pRetVal) = VT_EMPTY;
			return E_OUTOFMEMORY;
		}
		break;
	}
	default: {
		V_VT(pRetVal) = VT_EMPTY;
		break;
	}
	}

	return S_OK;
}

IFACEMETHODIMP Provider::get_HostRawElementProvider(IRawElementProviderSimple** const pRetVal) noexcept {
	if (!pRetVal) [[unlikely]] {
		return E_POINTER;
	}

	*pRetVal = nullptr;

	if (!m_controlHWnd) [[unlikely]] {
		return E_FAIL;
	}

	return ::UiaHostProviderFromHwnd(m_controlHWnd, pRetVal);
}

IFACEMETHODIMP Provider::Invoke() noexcept {
	const HWND targetWnd = m_controlHWnd;
	if (targetWnd && ::IsWindow(targetWnd)) {
		::PostMessageW(targetWnd, WM_LBUTTONDOWN, MK_LBUTTON, 0);
		::PostMessageW(targetWnd, WM_LBUTTONUP, 0, 0);
	}
	return S_OK;
}

} // namespace Sral
#endif
