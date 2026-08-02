#ifndef SRAL_CPP_HPP
#define SRAL_CPP_HPP
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <iterator>

#include "SRAL.h"

namespace Sral {

class Exception final : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

[[noreturn]] inline void ThrowOpFailed(const char* msg) {
	throw Exception(msg);
}

inline void Check(bool result, const char* msg = "SRAL operation failed") {
	if (!result) [[unlikely]] {
		ThrowOpFailed(msg);
	}
}

struct Voice final {
	uint32_t index{0};
	std::string_view name{};
	std::string_view language{};
	std::string_view gender{};
	std::string_view vendor{};

private:
	std::unique_ptr<char[]> storage{nullptr};

public:
	explicit Voice(const SRAL_VoiceInfo& info) noexcept {
		index = static_cast<uint32_t>(info.index);

		const std::string_view sv_name = info.name ? info.name : "";
		const std::string_view sv_lang = info.language ? info.language : "";
		const std::string_view sv_gend = info.gender ? info.gender : "";
		const std::string_view sv_vend = info.vendor ? info.vendor : "";

		const size_t total_size = sv_name.size() + sv_lang.size() + sv_gend.size() + sv_vend.size() + 4;
		if (total_size == 4) return;

		storage = std::unique_ptr<char[]>(new (std::nothrow) char[total_size]);
		if SRAL_UNLIKELY (!storage) return;
		char* cursor = storage.get();

		auto copy_block = [&](std::string_view src, std::string_view& dest) noexcept {
			if (src.empty()) {
				dest = std::string_view("");
				return;
			}
			std::copy_n(src.data(), src.size(), cursor);
			dest = std::string_view(cursor, src.size());
			cursor += src.size();
			*cursor++ = '\0';
		};

		copy_block(sv_name, name);
		copy_block(sv_lang, language);
		copy_block(sv_gend, gender);
		copy_block(sv_vend, vendor);
	}

	~Voice() noexcept = default;
	Voice(const Voice&) = delete;
	Voice& operator=(const Voice&) = delete;

	Voice(Voice&& other) noexcept
		: index(other.index), name(other.name), language(other.language), gender(other.gender), vendor(other.vendor),
		  storage(std::move(other.storage)) {
		other.name = other.language = other.gender = other.vendor = {};
		other.index = 0;
	}

	Voice& operator=(Voice&& other) noexcept {
		if SRAL_LIKELY (this != &other) {
			index = other.index;
			name = other.name;
			language = other.language;
			gender = other.gender;
			vendor = other.vendor;
			storage = std::move(other.storage);

			other.name = other.language = other.gender = other.vendor = {};
			other.index = 0;
		}
		return *this;
	}
};

struct AudioBuffer final {
	sral::PCMBuffer buffer;

	[[nodiscard]] std::span<const uint8_t> GetData() const noexcept { return buffer.data; }
	[[nodiscard]] int GetChannels() const noexcept { return buffer.channels; }
	[[nodiscard]] int GetSampleRate() const noexcept { return buffer.sample_rate; }
	[[nodiscard]] int GetBitsPerSample() const noexcept { return buffer.bits_per_sample; }

	[[nodiscard]] double GetDurationSeconds() const noexcept {
		if (buffer.sample_rate <= 0 || buffer.channels <= 0 || buffer.bits_per_sample <= 0) [[unlikely]] {
			return 0.0;
		}
		const size_t bits = static_cast<size_t>(buffer.bits_per_sample);
		if SRAL_UNLIKELY ((bits & 7) != 0) {
			return 0.0;
		}
		const size_t bytes_per_sample = bits >> 3;
		return static_cast<double>(buffer.data.size()) /
			(static_cast<double>(buffer.sample_rate) * buffer.channels * bytes_per_sample);
	}
};

struct TransientVoice final {
	uint32_t index{0};
	std::string_view name{};
	std::string_view language{};
	std::string_view gender{};
	std::string_view vendor{};

	constexpr explicit TransientVoice(const SRAL_VoiceInfo& info) noexcept
		: index(static_cast<uint32_t>(info.index)), name(info.name ? info.name : ""),
		  language(info.language ? info.language : ""), gender(info.gender ? info.gender : ""),
		  vendor(info.vendor ? info.vendor : "") {}
};

class VoiceListView final {
private:
	sral::VoiceList raw_list;

public:
	constexpr explicit VoiceListView(sral::VoiceList&& list) noexcept : raw_list(std::move(list)) {}

	struct Iterator {
		using iterator_concept = std::input_iterator_tag;
		using iterator_category = std::input_iterator_tag;
		using value_type = TransientVoice;
		using difference_type = std::ptrdiff_t;
		using pointer = const TransientVoice*;
		using reference = TransientVoice;

		const SRAL_VoiceInfo* ptr{nullptr};

		constexpr TransientVoice operator*() const noexcept { return TransientVoice(*ptr); }
		constexpr Iterator& operator++() noexcept {
			++ptr;
			return *this;
		}
		constexpr Iterator operator++(int) noexcept {
			Iterator temp = *this;
			++ptr;
			return temp;
		}
		constexpr bool operator==(const Iterator& other) const noexcept { return ptr == other.ptr; }
	};

	[[nodiscard]] constexpr Iterator begin() const noexcept { return Iterator{raw_list.voices.data()}; }
	[[nodiscard]] constexpr Iterator end() const noexcept {
		const auto* data_ptr = raw_list.voices.data();
		if SRAL_UNLIKELY (!data_ptr) return Iterator{nullptr};
		return Iterator{data_ptr + raw_list.voices.size()};
	}
	[[nodiscard]] constexpr size_t size() const noexcept { return raw_list.voices.size(); }
	[[nodiscard]] constexpr bool empty() const noexcept { return raw_list.voices.empty(); }
};

class System final {
public:
	explicit System(uint32_t engines_exclude_mask = 0) {
		if (!sral::Initialize(engines_exclude_mask)) [[unlikely]] {
			ThrowOpFailed("Failed to initialize SRAL core engine matrix");
		}
	}

	~System() noexcept { ::SRAL_Uninitialize(); }

	System(const System&) = delete;
	System& operator=(const System&) = delete;
	System(System&&) noexcept = default;
	System& operator=(System&&) noexcept = default;

	void Speak(std::string_view text, bool interrupt = true) {
		Check(sral::Speak(text, interrupt), "Speak failed");
	}

	void SpeakSsml(std::string_view ssml, bool interrupt = true) {
		Check(sral::SpeakSsml(ssml, interrupt), "SpeakSSML failed");
	}

	void Braille(std::string_view text) { Check(sral::Braille(text), "Braille output failed"); }

	void Output(std::string_view text, bool interrupt = true) {
		Check(sral::Output(text, interrupt), "Output failed");
	}

	void Stop() noexcept { ::SRAL_StopSpeech(); }
	void Pause() noexcept { ::SRAL_PauseSpeech(); }
	void Resume() noexcept { ::SRAL_ResumeSpeech(); }

	[[nodiscard]] bool IsSpeaking() const noexcept { return ::SRAL_IsSpeaking(); }

	void DelayOutput(int time, std::string_view text, bool interrupt = true) {
		Check(sral::DelayOutput(time, text, interrupt), "DelayOutput failed");
	}

	[[nodiscard]] AudioBuffer SpeakToMemory(std::string_view text) {
		auto native_buffer = sral::SpeakToMemory(text);
		if (native_buffer.data.data() == nullptr) [[unlikely]] {
			ThrowOpFailed("SpeakToMemory operation returned a NULL buffer allocation");
		}
		AudioBuffer result;
		result.buffer = std::move(native_buffer);
		return result;
	}

	template <typename T>
	inline void SetParameter(SRAL_Engines engine_id, SRAL_EngineParams param, const T& value) noexcept {
		static_cast<void>(sral::SetEngineParameter(engine_id, param, value));
	}

	template <typename T> inline void SetParameter(SRAL_EngineParams param, const T& value) noexcept {
		SetParameter(GetCurrentEngineId(), param, value);
	}

	template <typename T>
	[[nodiscard]] inline T GetParameter(SRAL_Engines engine_id, SRAL_EngineParams param) const noexcept {
		T value{};
		static_cast<void>(sral::GetEngineParameter(engine_id, param, value));
		return value;
	}

	template <typename T> [[nodiscard]] inline T GetParameter(SRAL_EngineParams param) const noexcept {
		return GetParameter<T>(GetCurrentEngineId(), param);
	}

	[[nodiscard]] inline VoiceListView GetVoices(SRAL_Engines engine_id) const noexcept {
		return VoiceListView(sral::GetEngineVoiceList(engine_id));
	}

	[[nodiscard]] inline VoiceListView GetVoices() const noexcept {
		return GetVoices(GetCurrentEngineId());
	}

	[[nodiscard]] inline SRAL_Engines GetCurrentEngineId() const noexcept { return sral::GetCurrentEngine(); }

	[[nodiscard]] inline std::string_view GetEngineName(SRAL_Engines engine_id) const noexcept {
		return sral::GetEngineName(engine_id);
	}

	[[nodiscard]] inline uint32_t GetEngineFeatures(SRAL_Engines engine_id = SRAL_ENGINE_NONE) const noexcept {
		return sral::GetEngineFeatures(engine_id);
	}

	[[nodiscard]] inline bool HasFeature(SRAL_Engines engine_id, SRAL_SupportedFeatures feature) const noexcept {
		return (sral::GetEngineFeatures(engine_id) & feature) != 0;
	}
};

} // namespace Sral

#endif // SRAL_CPP_HPP
