#include "Encoding.h"
#include <algorithm>
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

bool UnicodeConvert(std::string_view input, std::wstring& output) noexcept {
    output.clear();
    if (input.empty()) [[unlikely]] {
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

#if defined(__cpp_lib_string_resize_and_overwrite)
    output.resize_and_overwrite(static_cast<size_t>(size_needed), [input, input_size, size_needed](wchar_t* buf, size_t) noexcept -> size_t {
        const int result = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), input_size, buf, size_needed);
        return (result > 0) ? static_cast<size_t>(result) : 0;
    });
    return !output.empty();
#else
    output.resize(static_cast<size_t>(size_needed));
    const int result = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), input_size, output.data(), size_needed);
    if (result > 0) [[likely]] {
        return true;
    }
    output.clear();
    return false;
#endif

#else
    std::mbstate_t state{};
    const char* ptr = input.data();
    const char* const end = input.data() + input.size();

    output.reserve(input.size());

    while (ptr < end) {
        char32_t c32 = 0;
        const size_t rc = mbrtoc32(&c32, ptr, static_cast<size_t>(end - ptr), &state);

        if (rc == static_cast<size_t>(-1) || rc == static_cast<size_t>(-2)) [[unlikely]] {
            output.clear();
            return false;
        }
        
        const size_t advance = (rc == 0) ? 1 : rc;
        output.push_back(static_cast<wchar_t>(c32));
        ptr += advance;
    }
    return true;
#endif
}

bool UnicodeConvert(std::wstring_view input, std::string& output) noexcept {
    output.clear();
    if (input.empty()) [[unlikely]] {
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

#if defined(__cpp_lib_string_resize_and_overwrite)
    output.resize_and_overwrite(static_cast<size_t>(size_needed), [input, input_size, size_needed](char* buf, size_t) noexcept -> size_t {
        const int result = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), input_size, buf, size_needed, nullptr, nullptr);
        return (result > 0) ? static_cast<size_t>(result) : 0;
    });
    return !output.empty();
#else
    output.resize(static_cast<size_t>(size_needed));
    const int result = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), input_size, output.data(), size_needed, nullptr, nullptr);
    if (result > 0) [[likely]] {
        return true;
    }
    output.clear();
    return false;
#endif

#else
    std::mbstate_t state{};
    output.reserve(input.size() * 4); 

    std::array<char, 4> bytes{};

    for (const wchar_t wch : input) {
        const char32_t c32 = static_cast<char32_t>(wch);
        const size_t rc = c32rtomb(bytes.data(), c32, &state);
        
        if (rc == static_cast<size_t>(-1)) [[unlikely]] {
            output.clear();
            return false;
        }

        output.append(bytes.data(), rc);
    }
    return true;
#endif
}

void XmlEncode(std::string& data) noexcept {
    if (data.empty()) [[unlikely]] {
        return;
    }

    size_t expansion_size = 0;
    for (const char c : data) {
        switch (c) {
        case '&':  expansion_size += 4; break;
        case '<':  expansion_size += 3; break;
        case '>':  expansion_size += 3; break;
        case '"':  expansion_size += 5; break;
        case '\'': expansion_size += 5; break;
        default: break;
        }
    }

    if (expansion_size == 0) {
        return; 
    }

    const size_t original_size = data.size();
    const size_t total_size = original_size + expansion_size;
    if (total_size < original_size) [[unlikely]] {
        return;
    }

    std::string encoded;
    encoded.reserve(total_size);

    for (size_t i = 0; i < original_size; ++i) {
        const char c = data[i];
        switch (c) {
        case '&':  encoded.append("&amp;"); break;
        case '<':  encoded.append("&lt;"); break;
        case '>':  encoded.append("&gt;"); break;
        case '"':  encoded.append("&quot;"); break;
        case '\'': encoded.append("&apos;"); break;
        default:   encoded.push_back(c); break;
        }
    }

    data = std::move(encoded);
}

} // namespace Sral