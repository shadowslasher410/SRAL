#ifndef SRAL_CPP_HPP
#define SRAL_CPP_HPP
#pragma once

#include <SRAL.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Sral {

// -----------------------------------------------------------------------------
// Exception Class
// -----------------------------------------------------------------------------
class Exception final : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

inline void Check(bool result, const char* msg = "SRAL operation failed") {
	if (!result) [[unlikely]] {
		throw Exception(msg);
	}
}

inline std::string NullTerminate(std::string_view view) {
	return std::string(view);
}

// -----------------------------------------------------------------------------
// Data Structures
// -----------------------------------------------------------------------------
struct Voice final {
	int index{0};
	std::string name;
	std::string language;
	std::string gender;
	std::string vendor;

	explicit Voice(const SRAL_VoiceInfo& info)
		: index(info.index), name(info.name ? info.name : ""), language(info.language ? info.language : ""),
		  gender(info.gender ? info.gender : ""), vendor(info.vendor ? info.vendor : "") {}
};

struct AudioBuffer final {
	std::vector<uint8_t> data;
	int channels{0};
	int sample_rate{0};
	int bits_per_sample{0};

	[[nodiscard]] double GetDurationSeconds() const noexcept {
		if (sample_rate <= 0 || channels <= 0 || bits_per_sample <= 0) [[unlikely]] {
			return 0.0;
		}
		const size_t bytes_per_sample = static_cast<size_t>(bits_per_sample) / 8;
		if (bytes_per_sample == 0)
			return 0.0;

		return static_cast<double>(data.size()) / (static_cast<double>(sample_rate) * channels * bytes_per_sample);
	}
};
// -----------------------------------------------------------------------------
// Main Wrapper Class
// -----------------------------------------------------------------------------
class System final {
public:
	explicit System(int engines_exclude = 0) {
		if (!SRAL_Initialize(engines_exclude)) {
			throw Exception("Failed to initialize SRAL core engine matrix");
		}
	}

	~System() noexcept { SRAL_Uninitialize(); }
	System(const System&) = delete;
	System& operator=(const System&) = delete;
	System(System&&) noexcept = default;
	System& operator=(System&&) noexcept = default;

	// -------------------------------------------------------------------------
	// Core Speech Routing Actions
	// -------------------------------------------------------------------------
	void Speak(std::string_view text, bool interrupt = true) {
		Check(SRAL_Speak(NullTerminate(text).c_str(), interrupt), "Speak failed");
	}

	void SpeakSsml(std::string_view ssml, bool interrupt = true) {
		Check(SRAL_SpeakSsml(NullTerminate(ssml).c_str(), interrupt), "SpeakSSML failed");
	}

	void Braille(std::string_view text) { Check(SRAL_Braille(NullTerminate(text).c_str()), "Braille output failed"); }

	void Output(std::string_view text, bool interrupt = true) {
		Check(SRAL_Output(NullTerminate(text).c_str(), interrupt), "Output failed");
	}

	void Stop() noexcept { static_cast<void>(SRAL_StopSpeech()); }
	void Pause() noexcept { static_cast<void>(SRAL_PauseSpeech()); }
	void Resume() noexcept { static_cast<void>(SRAL_ResumeSpeech()); }

	[[nodiscard]] bool IsSpeaking() const noexcept { return SRAL_IsSpeaking(); }

	// -------------------------------------------------------------------------
	// Scheduled Deferred Queue Drivers
	// -------------------------------------------------------------------------
	#ifdef __ANDROID__
	void DelayOutput(int time, std::string_view text, bool interrupt = true) {
		Check(SRAL_DelayOutput(time, NullTerminate(text).c_str(), interrupt), "DelayOutput failed");
	}
	#endif
	// -------------------------------------------------------------------------
	// Audio Stream Capture Mechanics
	// -------------------------------------------------------------------------
	[[nodiscard]] AudioBuffer SpeakToMemory(std::string_view text) {
		uint64_t size = 0;
		int channels = 0, rate = 0, bits = 0;

		void* raw_pointer = SRAL_SpeakToMemory(NullTerminate(text).c_str(), &size, &channels, &rate, &bits);
		if (!raw_pointer) {
			throw Exception("SpeakToMemory operation returned a NULL buffer allocation");
		}

		AudioBuffer buffer;
		buffer.channels = channels;
		buffer.sample_rate = rate;
		buffer.bits_per_sample = bits;

		try {
			const uint8_t* byte_pointer = static_cast<const uint8_t*>(raw_pointer);
			buffer.data.assign(byte_pointer, byte_pointer + size);
		}
		catch (...) {
			SRAL_free(raw_pointer);
			throw;
		}

		SRAL_free(raw_pointer);
		return buffer;
	}

	// -------------------------------------------------------------------------
	// Target Configuration Property Maps
	// -------------------------------------------------------------------------
	template <typename T> void SetParameter(int engine_id, SRAL_EngineParams param, T value) {
		if constexpr (std::is_same_v<T, bool>) {
			int val = value ? 1 : 0;
			Check(SRAL_SetEngineParameter(engine_id, param, &val), "SetEngineParameter failed");
		}
		else {
			Check(SRAL_SetEngineParameter(engine_id, param, &value), "SetEngineParameter failed");
		}
	}

	template <typename T> void SetParameter(SRAL_EngineParams param, T value) {
		SetParameter(GetCurrentEngineId(), param, value);
	}

	template <typename T> [[nodiscard]] T GetParameter(int engine_id, SRAL_EngineParams param) const {
		T value{};
		Check(SRAL_GetEngineParameter(engine_id, param, &value), "GetEngineParameter failed");
		return value;
	}

	template <typename T> [[nodiscard]] T GetParameter(SRAL_EngineParams param) const {
		return GetParameter<T>(GetCurrentEngineId(), param);
	}
	// -------------------------------------------------------------------------
	// Engine Properties & Discovery Maps
	// -------------------------------------------------------------------------
	[[nodiscard]] int GetCurrentEngineId() const noexcept { return SRAL_GetCurrentEngine(); }

	[[nodiscard]] std::string GetEngineName(int engine_id) const {
		const char* name = SRAL_GetEngineName(engine_id);
		return name ? std::string(name) : std::string("Unknown");
	}

	[[nodiscard]] int GetEngineFeatures(int engine_id = 0) const noexcept { return SRAL_GetEngineFeatures(engine_id); }

	[[nodiscard]] bool HasFeature(int engine_id, SRAL_SupportedFeatures feature) const noexcept {
		return (SRAL_GetEngineFeatures(engine_id) & feature) != 0;
	}

	[[nodiscard]] int GetAvailableEngines() const noexcept { return SRAL_GetAvailableEngines(); }
	[[nodiscard]] int GetActiveEngines() const noexcept { return SRAL_GetActiveEngines(); }

	// -------------------------------------------------------------------------
	// Voice Sub-System Extractors
	// -------------------------------------------------------------------------
	[[nodiscard]] std::vector<Voice> GetVoices(int engine_id) const {
		std::vector<Voice> result;
		int count = 0;

		if (!SRAL_GetEngineParameter(engine_id, SRAL_PARAM_VOICE_COUNT, &count) || count <= 0) {
			return result;
		}

		SRAL_VoiceInfo* raw_voice_array = nullptr;
		if (!SRAL_GetEngineParameter(engine_id, SRAL_PARAM_VOICE_PROPERTIES, &raw_voice_array) || !raw_voice_array) {
			return result;
		}

		result.reserve(static_cast<size_t>(count));
		for (int i = 0; i < count; ++i) {
			result.emplace_back(raw_voice_array[i]);
		}

		SRAL_free(raw_voice_array);
		return result;
	}

	[[nodiscard]] std::vector<Voice> GetVoices() const { return GetVoices(GetCurrentEngineId()); }

	// -------------------------------------------------------------------------
	// Global Keyboard Interceptions
	// -------------------------------------------------------------------------
	void RegisterKeyboardHooks() {
		Check(SRAL_RegisterKeyboardHooks(), "Failed to register global keyboard interception hooks");
	}

	void UnregisterKeyboardHooks() noexcept { SRAL_UnregisterKeyboardHooks(); }

	// -------------------------------------------------------------------------
	// Independent Engine Control Sub-Proxy Class
	// -------------------------------------------------------------------------
	class EngineProxy final {
	private:
		int id;
		System& sys;

	public:
		EngineProxy(int engine_id, System& system) : id(engine_id), sys(system) {}

		void Speak(std::string_view text, bool interrupt = true) const {
			Check(SRAL_SpeakEx(id, NullTerminate(text).c_str(), interrupt), "SpeakEx failed");
		}

		void SpeakSsml(std::string_view ssml, bool interrupt = true) const {
			Check(SRAL_SpeakSsmlEx(id, NullTerminate(ssml).c_str(), interrupt), "SpeakSsmlEx failed");
		}

		void Braille(std::string_view text) const {
			Check(SRAL_BrailleEx(id, NullTerminate(text).c_str()), "BrailleEx failed");
		}

		void Output(std::string_view text, bool interrupt = true) const {
			Check(SRAL_OutputEx(id, NullTerminate(text).c_str(), interrupt), "OutputEx failed");
		}

		#ifdef __ANDROID__
		void DelayOutput(int time, std::string_view text, bool interrupt = true) const {
			Check(SRAL_DelayOutputEx(id, time, NullTerminate(text).c_str(), interrupt), "DelayOutputEx failed");
		}
		#endif

		void Stop() const noexcept { static_cast<void>(SRAL_StopSpeechEx(id)); }
		void Pause() const noexcept { static_cast<void>(SRAL_PauseSpeechEx(id)); }
		void Resume() const noexcept { static_cast<void>(SRAL_ResumeSpeechEx(id)); }

		[[nodiscard]] bool IsSpeaking() const noexcept { return SRAL_IsSpeakingEx(id); }

		[[nodiscard]] AudioBuffer SpeakToMemory(std::string_view text) const {
			uint64_t size = 0;
			int channels = 0, rate = 0, bits = 0;

			void* raw_pointer = SRAL_SpeakToMemoryEx(id, NullTerminate(text).c_str(), &size, &channels, &rate, &bits);
			if (!raw_pointer) {
				throw Exception("SpeakToMemoryEx operation returned a NULL buffer allocation");
			}

			AudioBuffer buffer;
			buffer.channels = channels;
			buffer.sample_rate = rate;
			buffer.bits_per_sample = bits;

			try {
				const uint8_t* byte_pointer = static_cast<const uint8_t*>(raw_pointer);
				buffer.data.assign(byte_pointer, byte_pointer + size);
			}
			catch (...) {
				SRAL_free(raw_pointer);
				throw;
			}

			SRAL_free(raw_pointer);
			return buffer;
		}
	};

	[[nodiscard]] EngineProxy GetEngine(int engine_id) noexcept { return EngineProxy(engine_id, *this); }
};

} // namespace Sral

#endif // SRAL_CPP_HPP
