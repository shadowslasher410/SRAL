#ifndef JAWS_H_
#define JAWS_H_
#pragma once

#include "../Dep/fsapi.h"
#include "../Include/SRAL.h"
#include "Engine.h"
#include <windows.h>
#include <comdef.h>

_COM_SMARTPTR_TYPEDEF(IJawsApi, __uuidof(IJawsApi));

namespace Sral {

class Jaws final : public Engine {
public:
	Jaws() noexcept = default;
	~Jaws() override = default;

	Jaws(const Jaws&) = delete;
	Jaws& operator=(const Jaws&) = delete;
	Jaws(Jaws&&) = delete;
	Jaws& operator=(Jaws&&) = delete;

	[[nodiscard]] bool Speak(const char* text, bool interrupt) override;
	[[nodiscard]] bool Braille(const char* text) override;
	[[nodiscard]] bool StopSpeech() override;
	[[nodiscard]] bool IsSpeaking() override;

	[[nodiscard]] int GetNumber() override { return SRAL_ENGINE_JAWS; }
	[[nodiscard]] int GetCategory() override { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	[[nodiscard]] bool GetActive() override;
	[[nodiscard]] int GetFeatures() override { return SRAL_SUPPORTS_SPEECH | SRAL_SUPPORTS_BRAILLE; }

	[[nodiscard]] bool Initialize() override;
	[[nodiscard]] bool Uninitialize() override;

private:
	IJawsApiPtr pJawsApi{ nullptr };
};

} // namespace Sral

#endif // JAWS_H_
