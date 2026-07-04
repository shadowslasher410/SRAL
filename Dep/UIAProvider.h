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
#pragma once

#include <windows.h>
#include <uiautomation.h>
#include <uiautomationcore.h>

class Provider final : public IRawElementProviderSimple, public IInvokeProvider
{
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

    volatile LONG m_refCount{ 1 };
    HWND m_controlHWnd{ nullptr };
};
