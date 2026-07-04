#include "Engine.h"
#include <cstddef>
#include "../Include/SRAL.h"

namespace Sral {

bool Engine::SpeakSsml(const char* const ssml, const bool interrupt) {
	return Speak(ssml, interrupt);
}

void* Engine::SpeakToMemory([[maybe_unused]] const char* const text,
	[[maybe_unused]] uint64_t* const buffer_size,
	[[maybe_unused]] int* const channels,
	[[maybe_unused]] int* const sample_rate,
	[[maybe_unused]] int* const bits_per_sample) {
	return nullptr;
}

bool Engine::Braille([[maybe_unused]] const char* const text) {
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

bool Engine::SetParameter([[maybe_unused]] const int param, [[maybe_unused]] const void* const value) {
	return false;
}

bool Engine::GetParameter([[maybe_unused]] const int param, [[maybe_unused]] void* const value) {
	return false;
}

} // namespace Sral