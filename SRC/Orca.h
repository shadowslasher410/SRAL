#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>

#include "Engine.h"

#if defined(__linux__) && !defined(__ANDROID__)
struct DBusConnection;
#endif

namespace Sral {

class Orca final : public Engine {
public:
	Orca() noexcept = default;
	~Orca() noexcept final = default;

	Orca(const Orca&) = delete;
	Orca& operator=(const Orca&) = delete;
	Orca(Orca&&) noexcept = delete;
	Orca& operator=(Orca&&) noexcept = delete;

	[[nodiscard]] bool Speak(std::string_view text, bool interrupt);
	[[nodiscard]] bool SpeakSsml(std::string_view ssml, bool interrupt);
	bool Braille(std::string_view text);

	[[nodiscard]] bool Speak(const char* text, bool interrupt) final {
		return Speak(text ? std::string_view(text) : std::string_view(), interrupt);
	}

	[[nodiscard]] bool SpeakSsml(const char* ssml, bool interrupt) final {
		return SpeakSsml(ssml ? std::string_view(ssml) : std::string_view(), interrupt);
	}

	bool Braille(const char* text) final {
		return Braille(text ? std::string_view(text) : std::string_view());
	}

	[[nodiscard]] bool Speak(std::nullptr_t, bool) noexcept;
	[[nodiscard]] bool SpeakSsml(std::nullptr_t, bool) noexcept;
	bool Braille(std::nullptr_t) noexcept;

	[[nodiscard]] bool StopSpeech() final;
	[[nodiscard]] bool IsSpeaking() final;
	[[nodiscard]] bool PauseSpeech() final { return false; }
	[[nodiscard]] bool ResumeSpeech() final { return false; }

	[[nodiscard]] int GetNumber() noexcept final { return 1 << 12; }
	[[nodiscard]] int GetCategory() noexcept final { return SRAL_ENGINE_CATEGORY_SCREEN_READER; }
	[[nodiscard]] int GetFeatures() noexcept final { return SRAL_SUPPORTS_SPEECH; }
	[[nodiscard]] int GetKeyFlags() noexcept final { return HANDLE_NONE; }

	[[nodiscard]] bool GetActive() noexcept final;
	[[nodiscard]] bool Initialize() final;
	bool Uninitialize() noexcept final;

	[[nodiscard]] void* SpeakToMemory([[maybe_unused]] const char* text,
		[[maybe_unused]] uint64_t* buffer_size,
		[[maybe_unused]] int* channels,
		[[maybe_unused]] int* sample_rate,
		[[maybe_unused]] int* bits_per_sample) final {

		if (buffer_size) {
			*buffer_size = 0;
		}
		if (channels) {
			*channels = 0;
		}
		if (sample_rate) {
			*sample_rate = 0;
		}
		if (bits_per_sample) {
			*bits_per_sample = 0;
		}
		return nullptr;
	}

	bool SetParameter([[maybe_unused]] int param, [[maybe_unused]] const void* value) final { return false; }
	bool GetParameter([[maybe_unused]] int param, [[maybe_unused]] void* value) final { return false; }

private:
	static std::atomic<bool> is_active;
	static std::mutex orca_mutex;

#if defined(__linux__) && !defined(__ANDROID__)
	static DBusConnection* _dbus_connection;
#endif
};

} // namespace Sral
