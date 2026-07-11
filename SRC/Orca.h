#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string_view>
#include <cstdint>

#include "../Include/SRAL.h"
#include "Engine.h"

#if defined(__linux__) && !defined(__ANDROID__)
struct DBusConnection;
#endif

namespace Sral {

class Orca final : public Engine {
public:
	Orca() noexcept = default;
	~Orca() override = default;

	Orca(const Orca&) = delete;
	Orca& operator=(const Orca&) = delete;
	Orca(Orca&&) noexcept = delete;
	Orca& operator=(Orca&&) noexcept = delete;

	[[nodiscard]] bool Speak(std::string_view text, bool interrupt);
	[[nodiscard]] bool SpeakSsml(std::string_view ssml, bool interrupt);
	bool Braille(std::string_view text);

	[[nodiscard]] bool Speak(const char* text, bool interrupt) override final {
		return Speak(text ? std::string_view(text) : std::string_view(), interrupt);
	}

	[[nodiscard]] bool SpeakSsml(const char* ssml, bool interrupt) override final {
		return SpeakSsml(ssml ? std::string_view(ssml) : std::string_view(), interrupt);
	}

	bool Braille(const char* text) override final { 
		return Braille(text ? std::string_view(text) : std::string_view()); 
	}

	// cppcheck-suppress functionStatic
	[[nodiscard]] bool Speak(std::nullptr_t, bool) noexcept;

	// cppcheck-suppress functionStatic
	[[nodiscard]] bool SpeakSsml(std::nullptr_t, bool) noexcept;

	// cppcheck-suppress functionStatic
	bool Braille(std::nullptr_t) noexcept;

	[[nodiscard]] bool StopSpeech() override final;
	[[nodiscard]] bool IsSpeaking() override final;
	[[nodiscard]] bool PauseSpeech() override final { return false; }
	[[nodiscard]] bool ResumeSpeech() override final { return false; }

	[[nodiscard]] int GetNumber() noexcept override final { return 1 << 12; }
	[[nodiscard]] int GetCategory() noexcept override final { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	[[nodiscard]] int GetFeatures() noexcept override final { return SRAL_SUPPORTS_SPEECH; }
	[[nodiscard]] int GetKeyFlags() noexcept override final { return HANDLE_NONE; }

	[[nodiscard]] bool GetActive() noexcept override final;
	[[nodiscard]] bool Initialize() override final;
	bool Uninitialize() noexcept override final;

	[[nodiscard]] void* SpeakToMemory([[maybe_unused]] const char* text,
		[[maybe_unused]] uint64_t* buffer_size,
		[[maybe_unused]] int* channels,
		[[maybe_unused]] int* sample_rate,
		[[maybe_unused]] int* bits_per_sample) override final {
		
		if (buffer_size) { *buffer_size = 0; }
		if (channels)    { *channels = 0; }
		if (sample_rate)  { *sample_rate = 0; }
		if (bits_per_sample) { *bits_per_sample = 0; }
		return nullptr;
	}

	bool SetParameter([[maybe_unused]] int param, [[maybe_unused]] const void* value) override final { return false; }
	bool GetParameter([[maybe_unused]] int param, [[maybe_unused]] void* value) override final { return false; }

private:
	static std::atomic<bool> is_active;
	static std::mutex orca_mutex;

#if defined(__linux__) && !defined(__ANDROID__)
	static DBusConnection* _dbus_connection;
#else
	static void* _dbus_connection;
#endif
};

} // namespace Sral
