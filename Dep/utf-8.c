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
#include "utf-8.h"

#include <immintrin.h>
#include <stdalign.h>
#include <stdint.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define BS_HOT __attribute__((hot))
#define BS_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define BS_LIKELY(x) __builtin_expect(!!(x), 1)
#else
#define BS_HOT
#define BS_UNLIKELY(x) (x)
#define BS_LIKELY(x) (x)
#endif

enum { BS_MAX_BOUND = 0xFFFFFFFFU };

alignas(16) const uint8_t BS_UTF8_Core_LUT[16] = {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 2, 2, 3, 4};

static inline uint32_t inline_utf8_converter_internal(const char* const BS_RESTRICT character, const uint8_t size) {
	const uint8_t* const BS_RESTRICT u_char = (const uint8_t*)character;
	if (size == 1)
		return (uint32_t)u_char[0];

	static const uint8_t masks[5] = {0x00, 0x7F, 0x1F, 0x0F, 0x07};
	uint32_t codepoint = (uint32_t)(masks[size & 3] & u_char[0]);

	switch (size) {
	case 4:
		if (BS_UNLIKELY((u_char[3] & 0xC0) != 0x80))
			return 0;
		codepoint = (codepoint << 6) | (u_char[3] & 0x3F);
		/* FALLTHROUGH */
	case 3:
		if (BS_UNLIKELY((u_char[2] & 0xC0) != 0x80))
			return 0;
		codepoint = (codepoint << 6) | (u_char[2] & 0x3F);
		/* FALLTHROUGH */
	case 2:
		if (BS_UNLIKELY((u_char[1] & 0xC0) != 0x80))
			return 0;
		codepoint = (codepoint << 6) | (u_char[1] & 0x3F);
		break;
	default:
		return 0;
	}

	if (BS_UNLIKELY((size == 2 && codepoint < 0x80) || (size == 3 && codepoint < 0x800) ||
			(size == 4 && codepoint < 0x10000))) {
		return 0;
	}

	return codepoint;
}

void utf8_init(utf8_iter* BS_RESTRICT const iter, const char* BS_RESTRICT const ptr) {
	*iter = (utf8_iter){.ptr = ptr,
		.codepoint = 0,
		.position = 0,
		.next = 0,
		.count = 0,
		.length = (ptr == NULL) ? 0 : BS_MAX_BOUND,
		.size = 0,
		.reserved_a = 0,
		.reserved_b = 0,
		.reserved_c = 0};
}

void utf8_initEx(utf8_iter* BS_RESTRICT const iter, const char* BS_RESTRICT const ptr, const uint32_t length) {
	*iter = (utf8_iter){.ptr = ptr,
		.codepoint = 0,
		.position = 0,
		.next = 0,
		.count = 0,
		.length = length,
		.size = 0,
		.reserved_a = 0,
		.reserved_b = 0,
		.reserved_c = 0};
}

uint8_t utf8_next(utf8_iter* BS_RESTRICT const iter) {
	const uint32_t c_next = iter->next;
	const uint32_t c_len = iter->length;

	if (BS_LIKELY(c_next < c_len)) {
		const uint8_t* const BS_RESTRICT current_char = (const uint8_t*)iter->ptr + c_next;
		if (current_char[0] == '\0') {
			iter->length = c_next;
			iter->position = c_next;
			return 0;
		}

		const uint8_t char_size = BS_UTF8_Core_LUT[current_char[0] >> 4];
		if (BS_UNLIKELY(char_size == 0))
			return 0;

		if (BS_UNLIKELY(c_len == BS_MAX_BOUND)) {
			for (uint8_t i = 1; i < char_size; ++i) {
				if (BS_UNLIKELY(current_char[i] == '\0')) {
					iter->length = c_next + i;
					iter->position = c_next + i;
					iter->next = c_next + i;
					return 0;
				}
			}
		}
		else if (BS_UNLIKELY((c_next + char_size) > c_len)) {
			return 0;
		}

		const uint32_t cp = inline_utf8_converter_internal((const char*)current_char, char_size);
		if (BS_UNLIKELY(cp == 0 || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF))
			return 0;

		iter->position = c_next;
		iter->size = char_size;
		iter->next = c_next + char_size;
		iter->codepoint = cp;
		iter->count++;
		return 1;
	}
	iter->position = c_next;
	return 0;
}

uint8_t utf8_previous(utf8_iter* BS_RESTRICT const iter) {
	uint32_t c_len = iter->length;
	if (BS_UNLIKELY(c_len == BS_MAX_BOUND)) {
		c_len = (uint32_t)strlen(iter->ptr);
		iter->length = c_len;
	}
	uint32_t c_pos = iter->position;
	uint32_t c_count = iter->count;
	const uint8_t* const BS_RESTRICT u_ptr = (const uint8_t*)iter->ptr;

	if (BS_UNLIKELY(c_len != 0 && c_pos == 0 && iter->next == 0)) {
		c_pos = c_len;
		c_count = 0;
	}

	if (BS_LIKELY(c_pos > 0)) {
		const uint32_t original_pos = c_pos;
		c_pos--;
		uint8_t byte_size = 1;
		if ((u_ptr[c_pos] & 0x80) != 0) {
			while (c_pos > 0 && (u_ptr[c_pos] & 0xC0) == 0x80 && byte_size < 4) {
				c_pos--;
				byte_size++;
			}
			if (BS_UNLIKELY((u_ptr[c_pos] & 0xC0) == 0x80))
				return 0;
		}

		const uint8_t expected_size = BS_UTF8_Core_LUT[u_ptr[c_pos] >> 4];
		if (BS_UNLIKELY(byte_size != expected_size))
			return 0;

		const uint32_t cp = inline_utf8_converter_internal((const char*)(u_ptr + c_pos), byte_size);
		if (BS_UNLIKELY(cp == 0 || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF))
			return 0;

		iter->next = original_pos;
		iter->position = c_pos;
		iter->size = byte_size;
		iter->codepoint = cp;
		iter->count = (c_count > 0) ? (c_count - 1) : 0;
		return 1;
	}
	iter->next = 0;
	return 0;
}

BS_HOT uint32_t utf8_strlen(const char* BS_RESTRICT const string) {
	size_t position = 0;
	uint32_t length = 0;

	while (1) {
		__m256i chunk = _mm256_loadu_si256((const __m256i*)(string + position));
		__m256i null_check = _mm256_cmpeq_epi8(chunk, _mm256_setzero_si256());
		uint32_t null_mask = (uint32_t)_mm256_movemask_epi8(null_check);
		if (BS_UNLIKELY(null_mask != 0))
			break;

		__m256i c_mask = _mm256_set1_epi8((char)0xC0);
		__m256i expected = _mm256_set1_epi8((char)0x80);
		__m256i is_continuation = _mm256_cmpeq_epi8(_mm256_and_si256(chunk, c_mask), expected);
		uint32_t continuation_mask = (uint32_t)_mm256_movemask_epi8(is_continuation);

		length += (32 - __popcnt(continuation_mask));
		position += 32;
	}

	while (string[position] != '\0') {
		uint8_t size = BS_UTF8_Core_LUT[(uint8_t)string[position] >> 4];
		if (BS_UNLIKELY(size == 0))
			break;
		position += size;
		length++;
	}
	return length;
}

BS_HOT uint32_t utf8_strnlen(const char* BS_RESTRICT const string, const uint32_t max_bytes) {
	size_t position = 0;
	uint32_t length = 0;

	while (position + 32 <= max_bytes) {
		__m256i chunk = _mm256_loadu_si256((const __m256i*)(string + position));
		__m256i null_check = _mm256_cmpeq_epi8(chunk, _mm256_setzero_si256());
		uint32_t null_mask = (uint32_t)_mm256_movemask_epi8(null_check);
		if (BS_UNLIKELY(null_mask != 0))
			break;

		__m256i c_mask = _mm256_set1_epi8((char)0xC0);
		__m256i expected = _mm256_set1_epi8((char)0x80);
		__m256i is_continuation = _mm256_cmpeq_epi8(_mm256_and_si256(chunk, c_mask), expected);
		uint32_t continuation_mask = (uint32_t)_mm256_movemask_epi8(is_continuation);

		length += (32 - __popcnt(continuation_mask));
		position += 32;
	}

	while (position < max_bytes && string[position] != '\0') {
		uint8_t size = BS_UTF8_Core_LUT[(uint8_t)string[position] >> 4];
		if (BS_UNLIKELY(size == 0 || (position + size) > max_bytes))
			break;
		position += size;
		length++;
	}
	return length;
}

uint32_t utf8_to_unicode(const char* BS_RESTRICT const character) {
	if (BS_UNLIKELY(!character || character[0] == '\0'))
		return 0;
	uint8_t size = BS_UTF8_Core_LUT[(uint8_t)character[0] >> 4];
	return (BS_UNLIKELY(size == 0)) ? 0 : inline_utf8_converter_internal(character, size);
}

#ifdef __cplusplus
uint8_t unicode_to_utf8(uint32_t codepoint, char* BS_RESTRICT out_buffer)
#else
uint8_t unicode_to_utf8(uint32_t codepoint, char out_buffer[static 5])
#endif
{
	const uint8_t char_size = inline_unicode_charsize(codepoint);
	if (BS_UNLIKELY(char_size == 0))
		return 0;

	uint32_t pattern = inline_unicode_converter(codepoint, char_size);
#if defined(__GNUC__) || defined(__clang__)
	pattern = __builtin_bswap32(pattern);
#elif defined(_MSC_VER)
	pattern = _byteswap_ulong(pattern);
#endif
	pattern >>= (32 - (char_size << 3));
	memcpy(out_buffer, &pattern, sizeof(uint32_t));
	out_buffer[char_size] = '\0';
	return char_size;
}

uint32_t utf8_converter(const char* BS_RESTRICT const character, uint8_t size) {
	return inline_utf8_converter_internal(character, size);
}

uint8_t unicode_charsize(uint32_t codepoint) {
	return inline_unicode_charsize(codepoint);
}

uint32_t unicode_converter(uint32_t codepoint, uint8_t size) {
	return inline_unicode_converter(codepoint, size);
}