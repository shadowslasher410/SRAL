#include "Engine.h"
#include <cstddef>
#include "SRAL.h"

namespace Sral {

bool Engine::SpeakSsml(const char* ssml, bool interrupt) {
	return Speak(ssml, interrupt);
}

void* Engine::SpeakToMemory(
	const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample) {
	(void)text;
	
	if (buffer_size != nullptr)     { *buffer_size = 0; }
	if (channels != nullptr)        { *channels = 0; }
	if (sample_rate != nullptr)     { *sample_rate = 0; }
	if (bits_per_sample != nullptr) { *bits_per_sample = 0; }
	return nullptr;
}

bool Engine::Braille(const char* text) {
	(void)text;
	return false;
}

bool Engine::PauseSpeech() {
	return false;
}

bool Engine::ResumeSpeech() {
	return false;
}

int Engine::GetKeyFlags() {
	return HANDLE_NONE;
}

bool Engine::SetParameter(int param, const void* value) {
	(void)param;
	(void)value;
	return false;
}

bool Engine::GetParameter(int param, void* value) {
	if (value == nullptr) [[unlikely]] {
		return false;
	}

	if (param == SRAL_PARAM_ENGINE_IS_PAUSED) {
		*static_cast<int*>(value) = paused ? 1 : 0;
		return true;
	}
	return false;
}

} // namespace Sral