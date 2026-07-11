#ifndef BLASTSPEAK_H
#define BLASTSPEAK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <oaidl.h>
#include <objbase.h>
#include <stddef.h>
#include <stdalign.h> 

#ifndef blastspeak_static_memory_length
#define blastspeak_static_memory_length 64 // in bytes
#endif

#ifndef blastspeak_max_languages_per_voice
#define blastspeak_max_languages_per_voice 4
#endif

typedef struct blastspeak
{
    IDispatch* voice;
    IDispatch* format;
    IDispatch* voices;
    IDispatch* default_voice_token;
    IDispatch* current_voice_token;
    char* allocated_memory;
    unsigned long sample_rate;
    unsigned int voice_count;
    unsigned int allocated_memory_length;
    unsigned int com_is_owned;
    DISPID voice_dispids[8];
    DISPID voice_collection_item_dispid;
    DISPID voice_token_dispids[2];
    DISPID memory_stream_dispids[3];
    DISPID audio_format_getwaveformatex_dispid;
    DISPID audio_format_setwaveformatex_dispid;
    DISPID waveformatex_dispids[4];
    unsigned char bits_per_sample;
    unsigned char channels;
    unsigned char must_reset_output;
    unsigned char reserved_padding;
    alignas(16) char static_memory[blastspeak_static_memory_length];
} blastspeak;

int blastspeak_initialize ( blastspeak* instance );
void blastspeak_destroy ( blastspeak* instance );
int blastspeak_speak ( blastspeak* instance, const char* text );
int blastspeak_set_voice ( blastspeak* instance, unsigned int voice_index );
int blastspeak_get_voice_description ( blastspeak* instance, unsigned int voice_index, char* out_buffer, size_t max_bytes );
int blastspeak_get_voice_attribute ( blastspeak* instance, unsigned int voice_index, const char* attribute, char* out_buffer, size_t max_bytes );
int blastspeak_get_voice_languages ( blastspeak* instance, unsigned int voice_index, char* out_buffer, size_t max_bytes );
int blastspeak_get_voice_rate ( blastspeak* instance, long* result );
int blastspeak_set_voice_rate ( blastspeak* instance, long value );
int blastspeak_get_voice_volume ( blastspeak* instance, long* result );
int blastspeak_set_voice_volume ( blastspeak* instance, long value );
char* blastspeak_speak_to_memory ( blastspeak* instance, unsigned long* bytes, const char* text );

#ifdef __cplusplus
}
#endif

#endif /* BLASTSPEAK_H */
