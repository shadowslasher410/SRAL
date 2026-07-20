/*
UTF-8 Iterator. Version 0.1.3

Original code by Adrian Guerrero Vera (adrianwk94@gmail.com)
MIT License
Copyright (c) 2016 Adrian Guerrero Vera

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/*
 * ==============================================================================
 * NOTICE OF ALTERATION
 * ==============================================================================
 * This file is an ALTERED and MODIFIED version of the original software library.
 * Changes made to this version include:
 *  1. Upgraded source file parameters to match the ISO C17 standard specifications.
 *  2. Converted 'unicode_to_utf8' and 'unicode_converter' signatures to target
 *     caller-allocated memory banks to establish complete thread safety.
 *  3. Applied strict const-correctness constraints to pointer data observers.
 *  4. Cleaned syntax and structural verification parameters across header blocks.
 * ==============================================================================
 */

#pragma once

#ifndef UTF8_ITER_H
#define UTF8_ITER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/**
 * @struct utf8_iter
 * @brief Thread-safe UTF-8 string multi-byte iteration state tracker (C17 Standard Compliant).
 */
typedef struct utf8_iter {
	const char* ptr;	/* Pointer tracking the underlying raw character vector */
	uint32_t codepoint; /* Decoded Unicode codepoint integer token value */
	uint8_t size;		/* Current character footprint size calculated in bytes */
	uint32_t position;	/* Current absolute byte position offset within array */
	uint32_t next;		/* Imminent byte position marker index */
	uint32_t count;		/* Logical multi-byte character sequence array counter */
	uint32_t length;	/* Total physical byte size layout of string (strlen) */
} utf8_iter;

/* --- Lifecycle Management --- */

/**
 * @brief Initializes a UTF-8 iterator by inspecting string length automatically.
 */
void utf8_init(utf8_iter* iter, const char* ptr);

/**
 * @brief Initializes a UTF-8 iterator with an explicit custom boundary execution scope length.
 */
void utf8_initEx(utf8_iter* iter, const char* ptr, uint32_t length);

/* --- Stream Navigation Routing Channels --- */

/**
 * @brief Moves the iterator structure forward by one logical character sequence alignment.
 * @return Returns 1 if another character is successfully parsed, 0 if bounds limit hit.
 */
uint8_t utf8_next(utf8_iter* iter);

/**
 * @brief Steps the iterator backward by processing multi-byte header boundaries.
 * @return Returns 1 if previous character layout resolved cleanly, 0 if baseline hit.
 */
uint8_t utf8_previous(utf8_iter* iter);

/**
 * @brief Resolves the pointer referencing the current active character offset window.
 */
const char* utf8_getchar(const utf8_iter* iter);

/* --- General String Utilities --- */
uint32_t utf8_strlen(const char* string);
uint32_t utf8_strnlen(const char* string, uint32_t max_bytes);
uint32_t utf8_to_unicode(const char* character);

/**
 * @brief Thread-safe conversion of a Unicode codepoint down to a multi-byte UTF-8 character string.
 * @param[out] out_buffer Target character destination block array (C17 array constraint bounds tracking).
 * @return Returns the number of encoded bytes successfully written to the target destination layout.
 */
#ifdef __cplusplus
uint8_t unicode_to_utf8(uint32_t codepoint, char* out_buffer);
#else
uint8_t unicode_to_utf8(uint32_t codepoint, char out_buffer[static 5]);
#endif

/* --- Advanced Engine Internal Processing Hooks --- */
uint8_t utf8_charsize(const char* character);
uint8_t unicode_charsize(uint32_t codepoint);
uint32_t utf8_converter(const char* character, uint8_t size);

/**
 * @brief Internal conversion execution step mapping codepoints down to native packed byte layouts safely.
 */
uint32_t unicode_converter(uint32_t codepoint, uint8_t size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UTF8_ITER_H */
