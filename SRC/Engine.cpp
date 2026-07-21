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
	(void)buffer_size;
	(void)channels;
	(void)sample_rate;
	(void)bits_per_sample;
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
	(void)param;
	(void)value;
	return false;
}

} // namespace Sral
