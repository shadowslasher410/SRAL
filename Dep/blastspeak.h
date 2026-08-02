#ifndef BLASTSPEAK_H
#define BLASTSPEAK_H

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
#define BS_NONNULL_ALL __attribute__((nonnull))
#define BS_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#define BS_LEAF __attribute__((leaf))
#define BS_STRUCT_ALIGN(x) __attribute__((aligned(x)))
#elif defined(_MSC_VER)
#define BS_RESTRICT __restrict
#define BS_PURE
#define BS_MUST_CHECK _Check_return_
#define BS_NONNULL_ALL
#define BS_NONNULL(...)
#define BS_LEAF
#define BS_STRUCT_ALIGN(x) __declspec(align(x))
#else
#define BS_RESTRICT restrict
#define BS_PURE
#define BS_MUST_CHECK
#define BS_NONNULL_ALL
#define BS_NONNULL(...)
#define BS_LEAF
#define BS_STRUCT_ALIGN(x)
#endif

#ifndef blastspeak_static_memory_length
#define blastspeak_static_memory_length 64
#endif

#ifndef blastspeak_max_languages_per_voice
#define blastspeak_max_languages_per_voice 4
#endif

struct IDispatch;
typedef struct IDispatch IDispatch;

#ifndef DISPID
typedef long DISPID;
#endif

typedef struct BS_STRUCT_ALIGN(16) blastspeak {
	IDispatch* voice;												 // 8 Bytes (64-bit native pointer alignment)
	IDispatch* format;												 // 8 Bytes
	IDispatch* voices;												 // 8 Bytes
	IDispatch* default_voice_token;									 // 8 Bytes
	IDispatch* current_voice_token;									 // 8 Bytes
	char* allocated_memory;											 // 8 Bytes
	uint32_t sample_rate;											 // 4 Bytes (Standard fixed 32-bit unsigned)
	uint32_t allocated_memory_length;								 // 4 Bytes
	uint32_t voice_count;											 // 4 Bytes
	int32_t voice_dispids[8];										 // 32 Bytes (Explicit 8 * 4-byte fields)
	int32_t waveformatex_dispids[4];								 // 16 Bytes (Explicit 4 * 4-byte fields)
	int32_t memory_stream_dispids[3];								 // 12 Bytes (Explicit 3 * 4-byte fields)
	int32_t voice_token_dispids[2];									 // 8 Bytes  (Explicit 2 * 4-byte fields)
	int32_t voice_collection_item_dispid;							 // 4 Bytes
	int32_t audio_format_getwaveformatex_dispid;					 // 4 Bytes
	int32_t audio_format_setwaveformatex_dispid;					 // 4 Bytes
	uint8_t bits_per_sample;										 // 1 Byte
	uint8_t channels;												 // 1 Byte
	uint8_t must_reset_output;										 // 1 Byte
	uint8_t com_is_owned;											 // 1 Byte
	alignas(16) char static_memory[blastspeak_static_memory_length]; // 64 Bytes
} blastspeak;

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(blastspeak) == 208, "Structure memory layout must equal exactly 208 bytes.");
#endif

int blastspeak_initialize(blastspeak* BS_RESTRICT const instance) BS_NONNULL(1) BS_LEAF;
void blastspeak_destroy(blastspeak* BS_RESTRICT const instance) BS_NONNULL(1) BS_LEAF;

int blastspeak_speak(blastspeak* BS_RESTRICT const instance, const char* BS_RESTRICT const text) BS_NONNULL_ALL BS_LEAF;
int blastspeak_set_voice(blastspeak* BS_RESTRICT const instance, const unsigned int voice_index) BS_NONNULL(1) BS_LEAF;

int blastspeak_get_voice_description(blastspeak* BS_RESTRICT const instance,
	const unsigned int voice_index,
	char* BS_RESTRICT const out_buffer,
	const size_t max_bytes) BS_NONNULL(1, 3) BS_LEAF;
int blastspeak_get_voice_attribute(blastspeak* BS_RESTRICT const instance,
	const unsigned int voice_index,
	const char* BS_RESTRICT const attribute,
	char* BS_RESTRICT const out_buffer,
	const size_t max_bytes) BS_NONNULL(1, 3, 4) BS_LEAF;
int blastspeak_get_voice_languages(blastspeak* BS_RESTRICT const instance,
	const unsigned int voice_index,
	char* BS_RESTRICT const out_buffer,
	const size_t max_bytes) BS_NONNULL(1, 3) BS_LEAF;

int blastspeak_get_voice_rate(
	blastspeak* BS_RESTRICT const instance, long* BS_RESTRICT const result) BS_NONNULL_ALL BS_LEAF;
int blastspeak_set_voice_rate(blastspeak* BS_RESTRICT const instance, const long value) BS_NONNULL(1) BS_LEAF;
int blastspeak_get_voice_volume(
	blastspeak* BS_RESTRICT const instance, long* BS_RESTRICT const result) BS_NONNULL_ALL BS_LEAF;
int blastspeak_set_voice_volume(blastspeak* BS_RESTRICT const instance, const long value) BS_NONNULL(1) BS_LEAF;

BS_MUST_CHECK char* blastspeak_speak_to_memory(blastspeak* BS_RESTRICT const instance,
	unsigned long* BS_RESTRICT const bytes,
	const char* BS_RESTRICT const text) BS_NONNULL_ALL BS_LEAF;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* BLASTSPEAK_H */
