#ifndef ENGINE_H_
#define ENGINE_H_
#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <vector>

// cppcheck-suppress syntaxError
namespace Sral {

enum KeyboardFlags : int { HANDLE_NONE = 0, HANDLE_INTERRUPT = 2, HANDLE_PAUSE_RESUME = 4 };

#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201907L
    inline constexpr std::size_t destructive_alignment = std::hardware_destructive_interference_size;
#else
    inline constexpr std::size_t destructive_alignment = 64;
#endif

class alignas(destructive_alignment) Engine {
public:
	Engine() noexcept = default;
	virtual ~Engine() noexcept = default;

	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;
	
	Engine(Engine&&) noexcept = default;
	Engine& operator=(Engine&&) noexcept = default;

	[[nodiscard]] virtual bool Speak(const char* text, bool interrupt) = 0;
	[[nodiscard]] virtual bool SpeakSsml(const char* ssml, bool interrupt);

	virtual void* SpeakToMemory(
		const char* text, uint64_t* buffer_size, int* channels, int* sample_rate, int* bits_per_sample);

	[[nodiscard]] virtual bool Braille(const char* text);
	[[nodiscard]] virtual bool StopSpeech() = 0;
	[[nodiscard]] virtual bool PauseSpeech();
	[[nodiscard]] virtual bool ResumeSpeech();
	[[nodiscard]] virtual bool IsSpeaking() = 0;
	[[nodiscard]] virtual int GetNumber() = 0;
	[[nodiscard]] virtual int GetCategory() = 0;
	[[nodiscard]] virtual bool GetActive() = 0;
	[[nodiscard]] virtual int GetFeatures() = 0;
	[[nodiscard]] virtual int GetKeyFlags();
	[[nodiscard]] virtual bool SetParameter(int param, const void* value);
	[[nodiscard]] virtual bool GetParameter(int param, void* value);
	virtual bool Initialize() = 0;
	virtual bool Uninitialize() = 0;

	bool paused = false;

protected:
	std::vector<std::unique_ptr<char[]>> m_strings;

	[[nodiscard]] const char* AddString(const char* str) {
		if (!str) [[unlikely]] {
			return nullptr;
		}

		const size_t len = std::strlen(str) + 1;
		auto cString = std::make_unique<char[]>(len);
		std::memcpy(cString.get(), str, len);

		const char* const raw_ptr = cString.get();
		m_strings.push_back(std::move(cString));
		return raw_ptr;
	}

	void ReleaseAllStrings() noexcept { m_strings.clear(); }
};

} // namespace Sral

#endif // ENGINE_H_