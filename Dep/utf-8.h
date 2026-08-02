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

#ifndef UTF8_ITER_H
#define UTF8_ITER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#include <cstdalign>
extern "C" {
#else
#include <stdalign.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define BS_RESTRICT __restrict__
#define BS_PURE __attribute__((pure))
#define BS_MUST_CHECK __attribute__((warn_unused_result))
#define BS_FORCE_INLINE __attribute__((always_inline)) static inline
#define BS_NONNULL_ALL __attribute__((nonnull))
#define BS_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#define BS_LEAF __attribute__((leaf))
#elif defined(_MSC_VER)
#define BS_RESTRICT __restrict
#define BS_PURE
#define BS_MUST_CHECK _Check_return_
#define BS_FORCE_INLINE __forceinline static inline
#define BS_NONNULL_ALL
#define BS_NONNULL(...)
#define BS_LEAF
#else
#define BS_RESTRICT restrict
#define BS_PURE
#define BS_MUST_CHECK
#define BS_FORCE_INLINE static inline
#define BS_NONNULL_ALL
#define BS_NONNULL(...)
#define BS_LEAF
#endif

/**
 * @struct utf8_iter
 * @brief Thread-safe UTF-8 string multi-byte iteration state tracker.
 * Explicitly aligned to a 16-byte boundary to guarantee a flawless 2-cycle
 * vector register dump instruction generation during initialization passes.
 */
typedef struct utf8_iter {
	const char* ptr;	// 8 Bytes (64-bit Target Native Alignment)
	uint32_t codepoint; // 4 Bytes
	uint32_t position;	// 4 Bytes
	uint32_t next;		// 4 Bytes
	uint32_t count;		// 4 Bytes
	uint32_t length;	// 4 Bytes
	uint8_t size;		// 1 Byte
	uint8_t reserved_a; // 1 Byte
	uint8_t reserved_b; // 1 Byte
	uint8_t reserved_c; // 1 Byte
} utf8_iter;

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(utf8_iter) == 32, "Structure memory layout must equal exactly 32 bytes.");
#endif

#define BS_UTF8_LUT_EXTRACT(x)                                                                                         \
	((uint8_t)("\x01\x01\x01\x01\x01\x01\x01\x01\x00\x00\x00\x00\x02\x02\x03\x04"[(x) & 0x0F]))

void utf8_init(utf8_iter* BS_RESTRICT const iter, const char* BS_RESTRICT const ptr) BS_NONNULL(1) BS_LEAF;
void utf8_initEx(utf8_iter* BS_RESTRICT const iter, const char* BS_RESTRICT const ptr, const uint32_t length)
	BS_NONNULL(1) BS_LEAF;

BS_MUST_CHECK uint8_t utf8_next(utf8_iter* BS_RESTRICT const iter) BS_NONNULL_ALL BS_LEAF;
BS_MUST_CHECK uint8_t utf8_previous(utf8_iter* BS_RESTRICT const iter) BS_NONNULL_ALL BS_LEAF;

BS_FORCE_INLINE const char* utf8_getchar(const utf8_iter* BS_RESTRICT const iter) BS_NONNULL_ALL {
	return (iter->size == 0) ? "" : (iter->ptr + iter->position);
}

BS_FORCE_INLINE uint8_t utf8_charsize(const char* BS_RESTRICT const character) BS_NONNULL_ALL {
	return BS_UTF8_LUT_EXTRACT((uint8_t)*character >> 4);
}

BS_PURE BS_MUST_CHECK uint32_t utf8_strlen(const char* BS_RESTRICT const string) BS_NONNULL_ALL BS_LEAF;
BS_PURE BS_MUST_CHECK uint32_t utf8_strnlen(
	const char* BS_RESTRICT const string, const uint32_t max_bytes) BS_NONNULL_ALL BS_LEAF;
BS_PURE BS_MUST_CHECK uint32_t utf8_to_unicode(const char* BS_RESTRICT const character) BS_NONNULL_ALL BS_LEAF;

#ifdef __cplusplus
uint8_t unicode_to_utf8(uint32_t codepoint, char* BS_RESTRICT out_buffer) BS_NONNULL_ALL BS_LEAF;
#else
uint8_t unicode_to_utf8(uint32_t codepoint, char out_buffer[static 5]) BS_NONNULL_ALL BS_LEAF;
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UTF8_ITER_H */