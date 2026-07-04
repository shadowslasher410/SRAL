#include "Encoding.h"

#include <string>
#include <string_view>
#include <array>
#include <climits>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

namespace Sral {

bool UnicodeConvert([[maybe_unused]] std::string_view input, [[maybe_unused]] std::wstring& output) {
	output.clear();
	if (input.empty()) {
		return true;
	}

#if defined(_WIN32) || defined(_WIN64)
	if (input.size() > static_cast<size_t>(INT_MAX)) [[unlikely]] {
		return false;
	}

	const int input_size = static_cast<int>(input.size());
	const int size_needed = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), input_size, nullptr, 0);
	if (size_needed <= 0) [[unlikely]] {
		return false;
	}

	constexpr int kStackBufferSize = 512;
	if (size_needed <= kStackBufferSize) {
		std::array<wchar_t, kStackBufferSize> stack_buffer;
		const int result = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), input_size, stack_buffer.data(), size_needed);
		if (result > 0) {
			output.assign(stack_buffer.data(), static_cast<size_t>(result));
			return true;
		}
	} else {
		output.resize(static_cast<size_t>(size_needed));
		const int result = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), input_size, output.data(), size_needed);
		if (result > 0) {
			return true;
		}
		output.clear();
	}
	return false;
#else
	return false;
#endif
}

bool UnicodeConvert([[maybe_unused]] std::wstring_view input, [[maybe_unused]] std::string& output) {
	output.clear();
	if (input.empty()) {
		return true;
	}

#if defined(_WIN32) || defined(_WIN64)
	if (input.size() > static_cast<size_t>(INT_MAX)) [[unlikely]] {
		return false;
	}

	const int input_size = static_cast<int>(input.size());
	
	const int size_needed = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), input_size, nullptr, 0, nullptr, nullptr);
	if (size_needed <= 0) [[unlikely]] {
		return false;
	}

	constexpr int kStackBufferSize = 512;
	if (size_needed <= kStackBufferSize) {
		std::array<char, kStackBufferSize> stack_buffer;
		const int result = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), input_size, stack_buffer.data(), size_needed, nullptr, nullptr);
		if (result > 0) {
			output.assign(stack_buffer.data(), static_cast<size_t>(result));
			return true;
		}
	} else {
		output.resize(static_cast<size_t>(size_needed));
		const int result = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), input_size, output.data(), size_needed, nullptr, nullptr);
		if (result > 0) {
			return true;
		}
		output.clear();
	}
	return false;
#else
	return false;
#endif
}

void XmlEncode(std::string& data) {
	if (data.empty()) {
		return;
	}

	size_t expansion_size = 0;
	for (const char c : data) {
		switch (c) {
		case '&':  expansion_size += 4; break; // "&amp;"  (+4 bytes)
		case '<':  expansion_size += 3; break; // "&lt;"   (+3 bytes)
		case '>':  expansion_size += 3; break; // "&gt;"   (+3 bytes)
		case '"':  expansion_size += 5; break; // "&quot;" (+5 bytes)
		case '\'': expansion_size += 5; break; // "&apos;" (+5 bytes)
		default: break;
		}
	}

	if (expansion_size == 0) {
		return;
	}

	const size_t total_size = data.size() + expansion_size;
	if (total_size < data.size()) [[unlikely]] {
		return;
	}

	std::string encoded;
	encoded.reserve(total_size);

	std::string_view source_view(data);
	constexpr std::string_view targets = "&<>'\"";

	while (!source_view.empty()) {
		const size_t i = source_view.find_first_of(targets);
		
		if (i == std::string_view::npos) {
			encoded.append(source_view);
			break;
		}

		if (i > 0) {
			encoded.append(source_view.substr(0, i));
		}

		switch (source_view[i]) {
		case '&':  encoded.append("&amp;");   break;
		case '<':  encoded.append("&lt;");    break;
		case '>':  encoded.append("&gt;");    break;
		case '"':  encoded.append("&quot;");  break;
		case '\'': encoded.append("&apos;");  break;
		default:   [[unlikely]] break;
		}

		source_view.remove_prefix(i + 1);
	}

	data = std::move(encoded);
}

} // namespace Sral
