#ifndef ENCODING_H_
#define ENCODING_H_
#pragma once

#include <string>
#include <string_view>

namespace Sral {

/**
 * @brief Converts a UTF-8 string view into a UTF-16 wide string.
 * @param input The UTF-8 source text string view.
 * @param output Reference to target wide string destination container.
 * @return True if conversion succeeded, false if an encoding error or constraint violation occurred.
 */
[[nodiscard]] bool UnicodeConvert(std::string_view input, std::wstring& output);

/**
 * @brief Converts a UTF-16 wide string view into a UTF-8 string.
 * @param input The UTF-16 source text string view.
 * @param output The destination UTF-8 string container.
 * @return True if conversion succeeded, false if an encoding error or constraint violation occurred.
 */
[[nodiscard]] bool UnicodeConvert(std::wstring_view input, std::string& output);

/**
 * @brief Escapes reserved XML/HTML characters on an active string.
 * @details Optimizes allocations using safe single-pass pre-calculated expansion metrics.
 * @param data Target string reference to escape.
 */
void XmlEncode(std::string& data);

} // namespace Sral

#endif // ENCODING_H_
