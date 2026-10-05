#pragma once
#include "../writer.c"
#include "json_tokenizer.c"

void _jdet_write_int(writer *w, uint64_t value) {
  uint64_t rem = value % 10;
  value = value / 10;
  if (value) {
    _jdet_write_int(w, value);
  }
  writer_write_byte(w, '0' + rem);
}

void json_detokenizer_write(writer *w, json_token *token) {
  switch (token->kind) {
  case JSON_TOKEN_OBJECT_OPEN:
    writer_write_byte(w, '{');
    return;
  case JSON_TOKEN_OBJECT_CLOSE:
    writer_write_byte(w, '}');
    return;
  case JSON_TOKEN_ARRAY_OPEN:
    writer_write_byte(w, '[');
    return;
  case JSON_TOKEN_ARRAY_CLOSE:
    writer_write_byte(w, ']');
    return;
  case JSON_TOKEN_BOM:
    writer_write_byte(w, UTF8_BOM[0]);
    writer_write_byte(w, UTF8_BOM[1]);
    writer_write_byte(w, UTF8_BOM[2]);
    return;
  case JSON_TOKEN_TRUE:
    writer_write_byte(w, 't');
    writer_write_byte(w, 'r');
    writer_write_byte(w, 'u');
    writer_write_byte(w, 'e');
    return;
  case JSON_TOKEN_FALSE:
    writer_write_byte(w, 'f');
    writer_write_byte(w, 'a');
    writer_write_byte(w, 'l');
    writer_write_byte(w, 's');
    writer_write_byte(w, 'e');
    return;
  case JSON_TOKEN_NULL:
    writer_write_byte(w, 'n');
    writer_write_byte(w, 'u');
    writer_write_byte(w, 'l');
    writer_write_byte(w, 'l');
    return;
  case JSON_TOKEN_COMMA:
    writer_write_byte(w, ',');
    return;
  case JSON_TOKEN_QUOTES:
    writer_write_byte(w, '"');
    return;
  case JSON_TOKEN_COLON:
    writer_write_byte(w, ':');
    return;
  case JSON_TOKEN_WHITESPACE:
    writer_write_byte(w, token->character.character);
    return;
  case JSON_TOKEN_ESCAPED_CHARACTER:
    writer_write_byte(w, '\\');
    writer_write_byte(w, token->character.character);
    return;
  case JSON_TOKEN_CHARACTER: {
    uint64_t chars = token->unicode_character.value;
    for (size_t i = 0; i < token->unicode_character.length; i++) {
      writer_write_byte(w, chars & 0xff);
      chars = chars >> 8;
    }
  }
    return;
  case JSON_TOKEN_ESCAPED_CHARCODE: {
    uint64_t chars = token->unicode_character.value;
    writer_write_byte(w, '\\');
    writer_write_byte(w, 'u');
    writer_write_byte(w, (chars >> 0) & 0xff);
    writer_write_byte(w, (chars >> 8) & 0xff);
    writer_write_byte(w, (chars >> 16) & 0xff);
    writer_write_byte(w, (chars >> 24) & 0xff);
  }
    return;
  default:
    // case JSON_TOKEN_NUMBER: as default for code coverage reasons
    // integer part
    if (token->number.flags & JSON_NUMBER_IS_NEGATIVE) {
      writer_write_byte(w, '-');
    }
    _jdet_write_int(w, token->number.integer_part);

    // fraction part
    if (token->number.flags & JSON_NUMBER_HAS_FRACTION) {
      writer_write_byte(w, '.');
      for (byte i = 0; i < token->number.fraction_leading_zeroes; i++) {
        writer_write_byte(w, '0');
      }
      if (token->number.fraction_part != 0) {
        _jdet_write_int(w, token->number.fraction_part);
      }
    }

    // exponent part
    if (token->number.flags & JSON_NUMBER_HAS_EXPONENT) {
      writer_write_byte(w, (token->number.flags & JSON_NUMBER_EXPONENT_UPPERCASE) ? 'E' : 'e');
      if (token->number.flags & (JSON_NUMBER_EXPONENT_HAS_PLUS | JSON_NUMBER_EXPONENT_IS_NEGATIVE)) {
        writer_write_byte(w, (token->number.flags & JSON_NUMBER_EXPONENT_HAS_PLUS) ? '+' : '-');
      }
      for (byte i = 0; i < token->number.exponent_leading_zeroes; i++) {
        writer_write_byte(w, '0');
      }
      if (token->number.exponent_part != 0) {
        _jdet_write_int(w, token->number.exponent_part);
      }
    }

    return;
  }
}