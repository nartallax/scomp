#pragma once
#include "../utf8.c"
#include "../writer.c"
#include "json_base.c"

size_t _jdet_write_int__new(writer *w, uint64_t value) {
  uint64_t rem = value % 10;
  value = value / 10;
  size_t result = 1;
  if (value) {
    result += _jdet_write_int__new(w, value);
  }
  writer_write_byte(w, '0' + rem);
  return result;
}

// TODO: tests for output length
size_t json_detokenizer_write__new(writer *w, json_token__new *token) {
  switch (token->kind) {
  case JSON_TOKEN_OBJECT_OPEN:
    writer_write_byte(w, '{');
    return 1;
  case JSON_TOKEN_OBJECT_CLOSE:
    writer_write_byte(w, '}');
    return 1;
  case JSON_TOKEN_ARRAY_OPEN:
    writer_write_byte(w, '[');
    return 1;
  case JSON_TOKEN_ARRAY_CLOSE:
    writer_write_byte(w, ']');
    return 1;
  case JSON_TOKEN_BOM:
    writer_write_byte(w, UTF8_BOM[0]);
    writer_write_byte(w, UTF8_BOM[1]);
    writer_write_byte(w, UTF8_BOM[2]);
    return sizeof(UTF8_BOM);
  case JSON_TOKEN_TRUE:
    writer_write_byte(w, _JTOK_TRUE_BYTES[0]);
    writer_write_byte(w, _JTOK_TRUE_BYTES[1]);
    writer_write_byte(w, _JTOK_TRUE_BYTES[2]);
    writer_write_byte(w, _JTOK_TRUE_BYTES[3]);
    return sizeof(_JTOK_TRUE_BYTES);
  case JSON_TOKEN_FALSE:
    writer_write_byte(w, _JTOK_FALSE_BYTES[0]);
    writer_write_byte(w, _JTOK_FALSE_BYTES[1]);
    writer_write_byte(w, _JTOK_FALSE_BYTES[2]);
    writer_write_byte(w, _JTOK_FALSE_BYTES[3]);
    writer_write_byte(w, _JTOK_FALSE_BYTES[4]);
    return sizeof(_JTOK_FALSE_BYTES);
  case JSON_TOKEN_NULL:
    writer_write_byte(w, _JTOK_NULL_BYTES[0]);
    writer_write_byte(w, _JTOK_NULL_BYTES[1]);
    writer_write_byte(w, _JTOK_NULL_BYTES[2]);
    writer_write_byte(w, _JTOK_NULL_BYTES[3]);
    return sizeof(_JTOK_NULL_BYTES);
  case JSON_TOKEN_COMMA:
    writer_write_byte(w, ',');
    return 1;
  case JSON_TOKEN_QUOTES:
    writer_write_byte(w, '"');
    return 1;
  case JSON_TOKEN_COLON:
    writer_write_byte(w, ':');
    return 1;
  case JSON_TOKEN_WHITESPACE:
    writer_write_byte(w, token->character.character);
    return 1;
  case JSON_TOKEN_ESCAPED_CHARACTER:
    writer_write_byte(w, '\\');
    writer_write_byte(w, token->character.character);
    return 2;
  case JSON_TOKEN_CHARACTER: {
    uint64_t chars = token->unicode_character.value;
    for (size_t i = 0; i < token->unicode_character.length; i++) {
      writer_write_byte(w, chars & 0xff);
      chars = chars >> 8;
    }
  }
    return token->unicode_character.length;
  case JSON_TOKEN_ESCAPED_CHARCODE: {
    uint64_t chars = token->unicode_character.value;
    writer_write_byte(w, '\\');
    writer_write_byte(w, 'u');
    writer_write_byte(w, (chars >> 0) & 0xff);
    writer_write_byte(w, (chars >> 8) & 0xff);
    writer_write_byte(w, (chars >> 16) & 0xff);
    writer_write_byte(w, (chars >> 24) & 0xff);
  }
    return 6;
  default:
    size_t result = 0;
    // case JSON_TOKEN_NUMBER: as default for code coverage reasons
    // integer part
    if (token->number.flags & JSON_NUMBER_IS_NEGATIVE) {
      writer_write_byte(w, '-');
      result++;
    }
    if (token->number.integer_part != 0 || (token->number.flags & JSON_NUMBER_IS_ZERO)) {
      result += _jdet_write_int__new(w, token->number.integer_part);
    }

    // fraction part
    if (token->number.flags & JSON_NUMBER_HAS_FRACTION) {
      writer_write_byte(w, '.');
      result++;
      for (byte i = 0; i < token->number.fraction_leading_zeroes; i++) {
        writer_write_byte(w, '0');
      }
      result += token->number.fraction_leading_zeroes;
      if (token->number.fraction_part != 0) {
        result += _jdet_write_int__new(w, token->number.fraction_part);
      }
    }

    // exponent part
    if (token->number.flags & JSON_NUMBER_HAS_EXPONENT) {
      writer_write_byte(w, (token->number.flags & JSON_NUMBER_EXPONENT_UPPERCASE) ? 'E' : 'e');
      result++;
      if (token->number.flags & (JSON_NUMBER_EXPONENT_HAS_PLUS | JSON_NUMBER_EXPONENT_IS_NEGATIVE)) {
        writer_write_byte(w, (token->number.flags & JSON_NUMBER_EXPONENT_HAS_PLUS) ? '+' : '-');
        result++;
      }
      for (byte i = 0; i < token->number.exponent_leading_zeroes; i++) {
        writer_write_byte(w, '0');
      }
      result += token->number.exponent_leading_zeroes;
      if (token->number.exponent_part != 0) {
        result += _jdet_write_int__new(w, token->number.exponent_part);
      }
    }

    return result;
  }
}