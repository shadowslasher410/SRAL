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
#include <string.h>

static const uint8_t table_unicode[] = {0, 0, 0x1F, 0x0F, 0x07};
static const uint8_t table_utf8[] = {0, 0, 0xC0, 0xE0, 0xF0};

_Static_assert(sizeof(table_unicode) == 5, "Unicode table size error");
_Static_assert(sizeof(table_utf8) == 5, "UTF-8 table size error");

void utf8_init(utf8_iter* iter, const char* ptr) {
    if (iter) {
        iter->ptr       = ptr;
        iter->codepoint = 0;
        iter->size      = 0;
        iter->position  = 0;
        iter->next      = 0;
        iter->count     = 0;
        iter->length    = (ptr == NULL) ? 0 : (uint32_t)strlen(ptr);
    }
}

void utf8_initEx(utf8_iter* iter, const char* ptr, uint32_t length) {
    if (iter) {
        iter->ptr       = ptr;
        iter->codepoint = 0;
        iter->size      = 0;
        iter->position  = 0;
        iter->next      = 0;
        iter->count     = 0;
        iter->length    = length;
    }
}

uint8_t utf8_next(utf8_iter* iter) {
    if (iter == NULL || iter->ptr == NULL) return 0;

    if (iter->next < iter->length) {
        iter->position = iter->next;
        const char* pointer = iter->ptr + iter->next;
        iter->size = utf8_charsize(pointer);

        if (iter->size == 0 || iter->next + iter->size > iter->length) return 0;

        iter->next      = iter->next + iter->size;
        iter->codepoint = utf8_converter(pointer, iter->size);

        if (iter->codepoint == 0) return 0;

        iter->count++;
        return 1;
    } else {
        iter->position = iter->next;
        return 0;
    }
}

uint8_t utf8_previous(utf8_iter* iter) {
    if (iter == NULL || iter->ptr == NULL) return 0;

    if (iter->length != 0 && iter->position == 0 && iter->next == 0) {
        iter->position = iter->length;
        iter->count    = utf8_strnlen(iter->ptr, iter->length);
    }

    if (iter->position > 0) {
        iter->next = iter->position;
        iter->position--;

        if ((iter->ptr[iter->position] & 0x80) != 0) {
            iter->size = 1;
            while (iter->position > 0 && (iter->ptr[iter->position] & 0xC0) == 0x80 && iter->size < 4) {
                iter->position--;
                iter->size++;
            }
            if ((iter->ptr[iter->position] & 0xC0) == 0x80) return 0; 
        } else {
            iter->size = 1;
        }

        const char* pointer = iter->ptr + iter->position;
        iter->codepoint = utf8_converter(pointer, iter->size);

        if (iter->codepoint == 0) return 0;

        iter->count--;
        return 1;
    } else {
        iter->next = 0;
        return 0;
    }
}

const char* utf8_getchar(const utf8_iter* iter) {
    if (iter == NULL || iter->ptr == NULL || iter->size == 0) return "";
    return iter->ptr + iter->position;
}

uint32_t utf8_strlen(const char* string) {
    if (string == NULL) return 0;
    uint32_t length = 0, position = 0;
    while (string[position]) {
        uint8_t size = utf8_charsize(string + position);
        if (size == 0) break; 
        position += size;
        length++;
    }
    return length;
}

uint32_t utf8_strnlen(const char* string, uint32_t end) {
    if (string == NULL) return 0;
    uint32_t length = 0, position = 0;
    while (string[position] && position < end) {
        uint8_t size = utf8_charsize(string + position);
        if (size == 0 || position + size > end) break;
        position += size;
        length++;
    }
    return length;
}

uint32_t utf8_to_unicode(const char* character) {
    if (character == NULL || character[0] == 0) return 0;
    uint8_t size = utf8_charsize(character);
    if (size == 0) return 0;
    return utf8_converter(character, size);
}

#ifdef __cplusplus
uint8_t unicode_to_utf8(uint32_t codepoint, char* out_buffer)
#else
uint8_t unicode_to_utf8(uint32_t codepoint, char out_buffer[static 5])
#endif
{
    uint8_t size = unicode_charsize(codepoint);
    if (size == 0 || size > 4 || out_buffer == NULL) return 0; 
    
    uint32_t dynamicPattern = unicode_converter(codepoint, size);
    for (uint8_t i = 0; i < size; i++) {
        out_buffer[i] = (char)((dynamicPattern >> (8 * (size - 1 - i))) & 0xFF);
    }
    out_buffer[size] = '\0';
    return size;
}

uint8_t utf8_charsize(const char* character) {
    if (character == NULL || character[0] == 0) return 0;
    if ((character[0] & 0x80) == 0) return 1;
    if ((character[0] & 0xE0) == 0xC0) return 2;
    if ((character[0] & 0xF0) == 0xE0) return 3;
    if ((character[0] & 0xF8) == 0xF0) return 4;
    return 0;
}

uint32_t utf8_converter(const char* character, uint8_t size) {
    if (size == 0 || size > 4 || character == NULL || character[0] == 0) return 0;
    if (size == 1) return (uint32_t)((uint8_t)character[0]);

    uint32_t codepoint = (uint32_t)(table_unicode[size] & character[0]);
    for (uint8_t i = 1; i < size; i++) {
        if ((character[i] & 0xC0) != 0x80) return 0;
        codepoint = (codepoint << 6) | (uint32_t)(character[i] & 0x3F);
    }
    if ((codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint > 0x10FFFF) return 0;
    return codepoint;
}

uint8_t unicode_charsize(uint32_t codepoint) {
    if (codepoint == 0) return 0;
    if (codepoint < 0x80) return 1;
    if (codepoint < 0x800) return 2;
    if (codepoint < 0x10000) return 3;
    if (codepoint <= 0x10FFFF) return 4;
    return 0;
}

uint32_t unicode_converter(uint32_t codepoint, uint8_t size) {
    if (size == 0 || size > 4) return 0;
    if (size == 1) return codepoint & 0xFF;

    uint32_t packedValue = 0;
    uint32_t workingVal = codepoint;

    for (uint8_t i = (uint8_t)(size - 1); i > 0; i--) {
        uint32_t byteVal = 0x80 | (workingVal & 0x3F);
        packedValue |= (byteVal << (8 * (size - 1 - i)));
        workingVal >>= 6;
    }
    uint32_t leadByte = (uint32_t)(table_utf8[size] | workingVal);
    packedValue |= (leadByte << (8 * (size - 1)));
    return packedValue;
}
