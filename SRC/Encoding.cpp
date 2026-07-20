#include "Encoding.h"

#include <array>
#include <climits>
#include <cstring>
#include <string>
#include <string_view>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <uchar.h>
#endif

namespace Sral {

bool UnicodeConvert(std::string_view input, std::wstring& output) {
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
		const int result = ::MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), input_size, stack_buffer.data(), size_needed);
		if (result > 0) {
			output.assign(stack_buffer.data(), static_cast<size_t>(result));
			return true;
		}
	}
	else {
		output.resize(static_cast<size_t>(size_needed));
		const int result =
			::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), input_size, output.data(), size_needed);
		if (result > 0) {
			return true;
		}
		output.clear();
	}
	return false;
#else
	std::mbstate_t state{};
	const char* ptr = input.data();
	const char* end = input.data() + input.size();

	constexpr size_t kStackBufferSize = 512;
	size_t converted_chars = 0;

	std::array<wchar_t, kStackBufferSize> stack_buffer;
	std::wstring heap_buffer;
	wchar_t* current_dest = stack_buffer.data();
	size_t current_capacity = kStackBufferSize;

	while (ptr < end) {
		char32_t c32 = 0;
		size_t rc = mbrtoc32(&c32, ptr, static_cast<size_t>(end - ptr), &state);

		if (rc == static_cast<size_t>(-1) || rc == static_cast<size_t>(-2)) [[unlikely]] {
			return false;
		}
		if (rc == 0) {
			rc = 1;
		}

		if (converted_chars >= current_capacity) {
			if (current_dest == stack_buffer.data()) {
				heap_buffer.assign(stack_buffer.data(), converted_chars);
			}
			heap_buffer.resize(heap_buffer.size() + kStackBufferSize);
			current_dest = heap_buffer.data();
			current_capacity = heap_buffer.size();
		}

		current_dest[converted_chars++] = static_cast<wchar_t>(c32);
		ptr += rc;
	}

	if (current_dest == stack_buffer.data()) {
		output.assign(stack_buffer.data(), converted_chars);
	}
	else {
		heap_buffer.resize(converted_chars);
		output = std::move(heap_buffer);
	}
	return true;
#endif
}

bool UnicodeConvert(std::wstring_view input, std::string& output) {
	output.clear();
	if (input.empty()) {
		return true;
	}

#if defined(_WIN32) || defined(_WIN64)
	if (input.size() > static_cast<size_t>(INT_MAX)) [[unlikely]] {
		return false;
	}

	const int input_size = static_cast<int>(input.size());
	const int size_needed =
		::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), input_size, nullptr, 0, nullptr, nullptr);
	if (size_needed <= 0) [[unlikely]] {
		return false;
	}

	constexpr int kStackBufferSize = 512;
	if (size_needed <= kStackBufferSize) {
		std::array<char, kStackBufferSize> stack_buffer;
		const int result = ::WideCharToMultiByte(CP_UTF8,
			WC_ERR_INVALID_CHARS,
			input.data(),
			input_size,
			stack_buffer.data(),
			size_needed,
			nullptr,
			nullptr);
		if (result > 0) {
			output.assign(stack_buffer.data(), static_cast<size_t>(result));
			return true;
		}
	}
	else {
		output.resize(static_cast<size_t>(size_needed));
		const int result = ::WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), input_size, output.data(), size_needed, nullptr, nullptr);
		if (result > 0) {
			return true;
		}
		output.clear();
	}
	return false;
#else
	std::mbstate_t state{};
	constexpr size_t kStackBufferSize = 1024;
	std::array<char, kStackBufferSize> stack_buffer;
	std::string heap_buffer;

	char* current_dest = stack_buffer.data();
	size_t bytes_written = 0;
	size_t current_capacity = kStackBufferSize;

	for (const wchar_t wch : input) {
		char32_t c32 = static_cast<char32_t>(wch);
		std::array<char, 4> bytes{};

		size_t rc = c32rtomb(bytes.data(), c32, &state);
		if (rc == static_cast<size_t>(-1)) [[unlikely]] {
			return false;
		}

		if (bytes_written + rc >= current_capacity) {
			if (current_dest == stack_buffer.data()) {
				heap_buffer.assign(stack_buffer.data(), bytes_written);
			}
			heap_buffer.resize(heap_buffer.size() + kStackBufferSize * 2);
			current_dest = heap_buffer.data();
			current_capacity = heap_buffer.size();
		}

		std::memcpy(current_dest + bytes_written, bytes.data(), rc);
		bytes_written += rc;
	}

	if (current_dest == stack_buffer.data()) {
		output.assign(stack_buffer.data(), bytes_written);
	}
	else {
		heap_buffer.resize(bytes_written);
		output = std::move(heap_buffer);
	}
	return true;
#endif
}

void XmlEncode(std::string& data) {
	if (data.empty()) {
		return;
	}

	size_t expansion_size = 0;
	for (const char c : data) {
		switch (c) {
		case '&':
			expansion_size += 4;
			break;
		case '<':
			expansion_size += 3;
			break;
		case '>':
			expansion_size += 3;
			break;
		case '"':
			expansion_size += 5;
			break;
		case '\'':
			expansion_size += 5;
			break;
		default:
			break;
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
		case '&':
			encoded.append("&amp;");
			break;
		case '<':
			encoded.append("&lt;");
			break;
		case '>':
			encoded.append("&gt;");
			break;
		case '"':
			encoded.append("&quot;");
			break;
		case '\'':
			encoded.append("&apos;");
			break;
		default:
			[[unlikely]] break;
		}

		source_view.remove_prefix(i + 1);
	}

	data = std::move(encoded);
}

} // namespace Sral
