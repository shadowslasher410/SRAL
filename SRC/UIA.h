#pragma once

#include <windows.h>
#include <uiautomation.h>
#include <comdef.h>
#include "../Include/SRAL.h"
#include "Engine.h"

_COM_SMARTPTR_TYPEDEF(IUIAutomation, __uuidof(IUIAutomation));
_COM_SMARTPTR_TYPEDEF(IUIAutomationCondition, __uuidof(IUIAutomationCondition));
_COM_SMARTPTR_TYPEDEF(IUIAutomationElement, __uuidof(IUIAutomationElement));
_COM_SMARTPTR_TYPEDEF(IRawElementProviderSimple, __uuidof(IRawElementProviderSimple));

namespace Sral {

class Uia final : public Engine {
public:
    Uia() noexcept = default;
    ~Uia() override { (void)Uninitialize(); }

    Uia(const Uia&) = delete;
    Uia& operator=(const Uia&) = delete;
    Uia(Uia&&) noexcept = delete;
    Uia& operator=(Uia&&) noexcept = delete;

    [[nodiscard]] bool Speak(const char* text, bool interrupt) override;
    [[nodiscard]] bool StopSpeech() override;
    [[nodiscard]] bool IsSpeaking() override;
    [[nodiscard]] int GetNumber() override { return SRAL_ENGINE_UIA; }
    [[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER; }
    [[nodiscard]] bool GetActive() override;
    [[nodiscard]] int GetFeatures() override { return SRAL_SUPPORTS_SPEECH; }

    bool Initialize() override;
    bool Uninitialize() override;

private:
    IUIAutomationPtr             pAutomation{nullptr};
    IUIAutomationConditionPtr    pCondition{nullptr};
    IUIAutomationElementPtr      pElement{nullptr};
    _variant_t                   varName;
    IRawElementProviderSimplePtr pProvider{nullptr};
};

} // namespace Sral
