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

#include <new>
#include <version>
#define WINDOWS_UIA_CORE_SUPPORTED 1
#else
#include <cstddef>
#include <cstdint>
#include <new>
#include <version>

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

inline void VariantInit(VARIANT* const p) noexcept {
	if (p) [[likely]]
		p->vt = VT_EMPTY;
}
inline const wchar_t* SysAllocString(const wchar_t* const str) noexcept {
	return str;
}

#define IFACEMETHODIMP HRESULT
#define IFACEMETHODIMP_(type) type
#endif

// Pure C++20 standard feature-testing macro verification for cache alignments
#if defined(__cpp_lib_hardware_interference_size) && !defined(__APPLE__)
using std::hardware_destructive_interference_size;
#else
#if defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64) || defined(TARGET_CPU_ARM64)
static constexpr size_t hardware_destructive_interference_size = 128;
#else
static constexpr size_t hardware_destructive_interference_size = 64;
#endif
#endif

namespace Sral {

#if defined(WINDOWS_UIA_CORE_SUPPORTED)
class alignas(hardware_destructive_interference_size) Provider final : public IRawElementProviderSimple,
																	   public IInvokeProvider {
public:
	explicit Provider(HWND const hwnd) noexcept;

	IFACEMETHODIMP_(ULONG) AddRef() noexcept override;
	IFACEMETHODIMP_(ULONG) Release() noexcept override;
	IFACEMETHODIMP QueryInterface(REFIID riid, void** const ppvObject) noexcept override;
	IFACEMETHODIMP get_ProviderOptions(ProviderOptions* const pRetVal) noexcept override;
	IFACEMETHODIMP GetPatternProvider(PATTERNID const patternId, IUnknown** const pRetVal) noexcept override;
	IFACEMETHODIMP GetPropertyValue(PROPERTYID const propertyId, VARIANT* const pRetVal) noexcept override;
	IFACEMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple** const pRetVal) noexcept override;
	IFACEMETHODIMP Invoke() noexcept override;

private:
	~Provider() noexcept = default;

	Provider(const Provider&) = delete;
	Provider& operator=(const Provider&) = delete;
	Provider(Provider&&) = delete;
	Provider& operator=(Provider&&) = delete;
	alignas(hardware_destructive_interference_size) LONG m_refCount{1};
	HWND m_controlHWnd{nullptr};
};
#else
class alignas(hardware_destructive_interference_size) Provider final {
public:
	explicit Provider(HWND const hwnd) noexcept { (void)hwnd; }
	~Provider() noexcept = default;

	[[nodiscard]] constexpr unsigned long AddRef() noexcept { return 1; }
	[[nodiscard]] constexpr unsigned long Release() noexcept { return 0; }
};
#endif

} // namespace Sral

#endif // UIAPROVIDER_H_
