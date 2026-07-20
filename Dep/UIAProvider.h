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
#ifndef UIAPROVIDER_H_
#define UIAPROVIDER_H_
#pragma once

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

#include <uiautomation.h>
#include <uiautomationcore.h>
#define WINDOWS_UIA_CORE_SUPPORTED 1
#else
using HWND = void*;
using ULONG = unsigned long;
using REFIID = void*;
using PATTERNID = int;
using PROPERTYID = int;
using ProviderOptions = int;
using IUnknown = void;
using IRawElementProviderSimple = void;
using IInvokeProvider = void;
using HRESULT = int;

struct VARIANT {
	int vt;
	union {
		long lVal;
		const wchar_t* bstrVal;
	};
};

#define VT_EMPTY 0
#define VT_I4 3
#define VT_BSTR 8
#define S_OK 0
#define E_POINTER (-2147467261)
#define E_OUTOFMEMORY (-2147024882)

#define V_VT(X) ((X)->vt)
#define V_I4(X) ((X)->lVal)
#define V_BSTR(X) ((X)->bstrVal)

inline void VariantInit(VARIANT* p) noexcept {
	if (p)
		p->vt = VT_EMPTY;
}
inline const wchar_t* SysAllocString(const wchar_t* str) noexcept {
	return str;
}

#define IFACEMETHODIMP HRESULT
#define IFACEMETHODIMP_(type) type
#define override
#endif

#ifdef __cplusplus
namespace Sral {
#endif

#if defined(WINDOWS_UIA_CORE_SUPPORTED)
class Provider final : public IRawElementProviderSimple, public IInvokeProvider {
public:
	explicit Provider(HWND hwnd) noexcept;

	IFACEMETHODIMP_(ULONG) AddRef() override;
	IFACEMETHODIMP_(ULONG) Release() override;
	IFACEMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override;
	IFACEMETHODIMP get_ProviderOptions(ProviderOptions* pRetVal) override;
	IFACEMETHODIMP GetPatternProvider(PATTERNID patternId, IUnknown** pRetVal) override;
	IFACEMETHODIMP GetPropertyValue(PROPERTYID propertyId, VARIANT* pRetVal) override;
	IFACEMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple** pRetVal) override;
	IFACEMETHODIMP Invoke() override;

private:
	~Provider() = default;

	Provider(const Provider&) = delete;
	Provider& operator=(const Provider&) = delete;
	Provider(Provider&&) = delete;
	Provider& operator=(Provider&&) = delete;

	LONG m_refCount{1};
	HWND m_controlHWnd{nullptr};
};
#else
class Provider final {
public:
	explicit Provider(HWND hwnd) noexcept { (void)hwnd; }
	~Provider() = default;

	[[nodiscard]] unsigned long AddRef() noexcept { return 1; }
	[[nodiscard]] unsigned long Release() noexcept { return 0; }
};
#endif
#ifdef __cplusplus
}
#endif

#if !defined(_WIN32) && !defined(_WIN64)
#undef override
#endif

#endif
