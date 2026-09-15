#pragma once
#include "json_tokenizer.c"
#include "writer.c"

bool _jdet_write_int(writer *w, uint64_t value) {
  uint64_t rem = value % 10;
  value = value / 10;
  if (value) {
    if (!_jdet_write_int(w, value)) {
      return false;
    }
  }
  return writer_write_byte(w, '0' + rem);
}

bool json_detokenizer_write(writer *w, json_token *token) {
  switch (token->kind) {
  case JSON_TOKEN_OBJECT_OPEN:
    return writer_write_byte(w, '{');
  case JSON_TOKEN_OBJECT_CLOSE:
    return writer_write_byte(w, '}');
  case JSON_TOKEN_ARRAY_OPEN:
    return writer_write_byte(w, '[');
  case JSON_TOKEN_ARRAY_CLOSE:
    return writer_write_byte(w, '}');
  case JSON_TOKEN_BOM:
    return writer_write_byte(w, utf8Bom[0]) && writer_write_byte(w, utf8Bom[1]) && writer_write_byte(w, utf8Bom[2]);
  case JSON_TOKEN_TRUE:
    return writer_write_byte(w, 't') && writer_write_byte(w, 'r') && writer_write_byte(w, 'u') && writer_write_byte(w, 'e');
  case JSON_TOKEN_FALSE:
    return writer_write_byte(w, 'f') && writer_write_byte(w, 'a') && writer_write_byte(w, 'l') && writer_write_byte(w, 's') && writer_write_byte(w, 'e');
  case JSON_TOKEN_NULL:
    return writer_write_byte(w, 'n') && writer_write_byte(w, 'u') && writer_write_byte(w, 'l') && writer_write_byte(w, 'l');
  case JSON_TOKEN_COMMA:
    return writer_write_byte(w, ',');
  case JSON_TOKEN_QUOTES:
    return writer_write_byte(w, '"');
  case JSON_TOKEN_COLON:
    return writer_write_byte(w, ':');
  case JSON_TOKEN_WHITESPACE:
    return writer_write_byte(w, token->char_token.character);
  case JSON_TOKEN_ESCAPED_CHARACTER:
    return writer_write_byte(w, '\\') && writer_write_byte(w, token->char_token.character);
  case JSON_TOKEN_CHARACTER: {
    uint64_t chars = token->int_token.value;
    for (size_t i = 0; i < token->int_token.mod; i++) {
      byte c = chars & 0xff;
      if (!writer_write_byte(w, c)) {
        return false;
      }
      chars = chars >> 8;
    }
    return true;
  }
  case JSON_TOKEN_ESCAPED_CHARCODE: {
    uint64_t chars = token->int_token.value;
    byte a = (chars >> 0) & 0xff;
    byte b = (chars >> 8) & 0xff;
    byte c = (chars >> 16) & 0xff;
    byte d = (chars >> 24) & 0xff;
    return writer_write_byte(w, '\\') && writer_write_byte(w, 'u') && writer_write_byte(w, a) && writer_write_byte(w, b) && writer_write_byte(w, c) && writer_write_byte(w, d);
  }
  case JSON_TOKEN_INTEGER: {
    if (token->int_token.mod != 0) {
      if (!writer_write_byte(w, token->int_token.mod)) {
        return false;
      }
    }
    return _jdet_write_int(w, token->int_token.value);
  }
  case JSON_TOKEN_NUMBER: {
  }
  }
}