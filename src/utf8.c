#pragma once
#include "commons.c"
#include <stdbool.h>

#define UTF8_BOM_LENGTH 3
const byte utf8_bom[UTF8_BOM_LENGTH] = {0xEF, 0xBB, 0xBF};

/** Determines if the sequence of bytes can be a utf-8 byte-order mark. If this returns true and length >= 3 - the sequence contains valid BOM. */
bool utf8_can_bytes_be_bom_start(const byte *bytes, size_t length) {
  return (length < 1 || bytes[0] == utf8_bom[0]) && (length < 2 || bytes[1] == utf8_bom[1]) && (length < 3 || bytes[2] == utf8_bom[2]);
}

/** Returns number 1-4 depending on expected length of the sequence, or 0 if passed byte is an invalid start of the sequence. */
size_t utf8_get_sequence_length_by_first_byte(byte b) {
  if (b <= 0x7F) { // 0b0xxxxxxx
    return 1;
  } else if (b <= 0xC1) { // 0b10xxxxxx
    // 0x80-0xC1 range are continuation bytes; they can only be second and later bytes.
    // i.e. bytes like 0b10xxxxxx cannot be the first byte
    return 0;
  } else if (b <= 0xDF) { // 0b110xxxxx
    return 2;
  } else if (b <= 0xEF) { // 0b1110xxxx
    return 3;
  } else if (b <= 0xF4) { // 0b11110xxx
    return 4;
  } else {
    // 0xF5-0xFF are invalid starter bytes
    // https://www.rfc-editor.org/info/rfc3629/#section-4
    // they were valid at some point, but got deprecated
    return 0;
  }
}

/** Continuation bytes = bytes after first one */
bool utf8_are_continuation_bytes_valid(const byte *bytes, size_t length) {
  for (size_t i = 1; i < length; i++) {
    if ((bytes[i] & 0b11000000) != 0b10000000) {
      return false;
    }
  }
  return true;
}

byte utf8_get_meaningful_bits_of_first_byte(byte b) {
  // see sequence length function definition to figure out how this work
  size_t len = utf8_get_sequence_length_by_first_byte(b);
  if (len == 1) {
    return b & 0x7F;
  }
  return b & (0x7F >> len);
}

byte utf8_get_meaningful_bits_of_continuation_byte(byte b) {
  return b & 0x3f;
}