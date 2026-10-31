#pragma once
#include "../context.c"
#include "../data_structures/reader.c"
#include "../utf8.c"
#include "json_base.c"
#include "json_state_machine.c"
#include <inttypes.h>
#include <stdint.h>

typedef struct {
  json_state_machine jsm;
  reader *reader;
} json_tokenizer;

void json_tokenizer_deinit(json_tokenizer *t, context *context) {
  jsm_deinit(&t->jsm, context);

  *t = (json_tokenizer){0};
}

/** Should be called on already-initialized tokenizer to reset its state to initial.
Exists to avoid freeing and reallocating memory for a tokenizer you want to reuse */
void json_tokenizer_reset(json_tokenizer *t) {
  jsm_reset(&t->jsm);
}

NODISCARD bool json_tokenizer_init(json_tokenizer *t, context *context, reader *reader) {
  *t = (json_tokenizer){0};

  t->reader = reader;

  if (!jsm_init(&t->jsm, context)) {
    json_tokenizer_deinit(t, context);
    return false;
  }

  json_tokenizer_reset(t);

  return true;
}

constexpr uint64 TENTH_OF_MAX_UINT64 = UINT64_MAX / 10;
INLINE bool _jtok_uint64_will_overflow(uint64 value, byte addition) {
  return value >= TENTH_OF_MAX_UINT64 - addition;
}

INLINE bool _jtok_is_hex(byte hex_char) {
  return (hex_char >= '0' && hex_char <= '9') || (hex_char >= 'a' && hex_char <= 'f') || (hex_char >= 'A' && hex_char <= 'F');
}

json_token _jtok_yield_singlebyte_token(json_tokenizer *t, json_token token) {
  if (!jsm_process_token(&t->jsm, token.kind)) {
    return INVALID_JSON_TOKEN;
  }
  reader_read_byte(t->reader); // it's a single byte token, which means the byte wasn't consumed yet, and only peeked at
  return token;
}

json_token _jtok_yield_multibyte_token(json_tokenizer *t, json_token token, reader_snapshot snapshot) {
  if (!jsm_process_token(&t->jsm, token.kind)) {
    reader_restore_snapshot(t->reader, snapshot);
    return INVALID_JSON_TOKEN;
  }
  return token;
}

json_token _jtok_parse_string_token(json_tokenizer *t) {
  // TODO: give up on peeking and create snapshot every time instead? for performance (and it would be better for consistency)
  byte first_byte = reader_peek_byte(t->reader);
  // TODO: switch here? for performance
  if (first_byte == '"') {
    return _jtok_yield_singlebyte_token(t, (json_token){.kind = JSON_TOKEN_STRING_DELIMITER});
  }

  if (first_byte == '\\') {
    if (reader_get_bytes_available(t->reader) < 2) {
      return INVALID_JSON_TOKEN;
    }

    reader_snapshot snapshot = reader_take_snapshot(t->reader);
    reader_read_byte(t->reader);
    byte next_char = reader_read_byte(t->reader);
    for (size_t i = 0; i < sizeof(_JTOK_ESCAPE_SEQ_CHARS); i++) {
      if (next_char == _JTOK_ESCAPE_SEQ_CHARS[i]) {
        return _jtok_yield_multibyte_token(t, (json_token){.kind = JSON_TOKEN_ESCAPE_SEQUENCE, .character = {.character = next_char}}, snapshot);
      }
    }

    if (next_char == 'u') {
      if (reader_get_bytes_available(t->reader) < JSON_HEX_CHARACTERS_IN_ESCAPED_UNICODE_CODEPOINT) {
        reader_restore_snapshot(t->reader, snapshot);
        return INVALID_JSON_TOKEN;
      }
      uint64 value = 0;
      for (int64 i = 0; i < JSON_HEX_CHARACTERS_IN_ESCAPED_UNICODE_CODEPOINT; i++) {
        next_char = reader_read_byte(t->reader);
        if (!_jtok_is_hex(next_char)) {
          reader_restore_snapshot(t->reader, snapshot);
          return INVALID_JSON_TOKEN;
        }
        value |= ((uint64)next_char) << (i << 3);
      }
      return _jtok_yield_multibyte_token(
          t, (json_token){.kind = JSON_TOKEN_ESCAPED_UNICODE_CHARCODE, .unicode_character = {.value = value, .length = JSON_HEX_CHARACTERS_IN_ESCAPED_UNICODE_CODEPOINT}}, snapshot);
    }

    reader_restore_snapshot(t->reader, snapshot);
    return INVALID_JSON_TOKEN;
  }

  // not a string terminator or escape character
  int64 utf8_length = utf8_get_sequence_length_by_first_byte(first_byte);
  if (utf8_length == 0 || reader_get_bytes_available(t->reader) < utf8_length) {
    return INVALID_JSON_TOKEN;
  }
  if (utf8_length == 1 && first_byte <= 0x1f) {
    // unescaped control characters are not valid in JSON
    return INVALID_JSON_TOKEN;
  }
  reader_snapshot snapshot = reader_take_snapshot(t->reader);
  uint64 value = first_byte;
  reader_read_byte(t->reader);
  for (int64 i = 1; i < utf8_length; i++) {
    byte next_byte = reader_read_byte(t->reader);
    if (!utf8_is_valid_continuation_byte(next_byte)) {
      reader_restore_snapshot(t->reader, snapshot);
      return INVALID_JSON_TOKEN;
    }
    value |= ((uint64)next_byte) << (i << 3);
  }
  // TODO: consider not showing all those characters (and escaped tokens) to JSM? we know they are allowed. (for performance)
  return _jtok_yield_multibyte_token(t, (json_token){.kind = JSON_TOKEN_UNICODE_CHARACTER, .unicode_character = {.value = value, .length = utf8_length}}, snapshot);
}

json_token _jtok_parse_fixed_token(json_tokenizer *t, json_token_kind kind, const byte *expected_bytes, int64 expected_byte_length) {
  if (reader_get_bytes_available(t->reader) < expected_byte_length) {
    return INVALID_JSON_TOKEN;
  }
  reader_snapshot snapshot = reader_take_snapshot(t->reader);
  reader_read_byte(t->reader); // expecting first byte to already be checked, otherwise we won't have the kind
  for (int i = 1; i < expected_byte_length; i++) {
    byte b = reader_read_byte(t->reader);
    if (b != expected_bytes[i]) {
      reader_restore_snapshot(t->reader, snapshot);
      return INVALID_JSON_TOKEN;
    }
  }

  return _jtok_yield_multibyte_token(t, (json_token){.kind = kind}, snapshot);
}

typedef struct {
  uint64 value;
  byte leading_zeroes;
} _jtok_uint_reading_result;

INLINE bool _jtok_uint_is_invalid(_jtok_uint_reading_result res) {
  return res.value == 0 && res.leading_zeroes == 0;
}

_jtok_uint_reading_result _jtok_read_uint(json_tokenizer *t) {
  byte leading_zeroes = 0;
  uint64 value = 0;
  while (reader_get_bytes_available(t->reader) > 0) {
    byte next_byte = reader_peek_byte(t->reader);
    if (next_byte < '0' || next_byte > '9') {
      break;
    }

    if (next_byte == '0' && value == 0) {
      leading_zeroes++;
      if (leading_zeroes >= JSON_MAX_NUMBER_LEADING_ZEROES) {
        return (_jtok_uint_reading_result){0};
      }
      reader_read_byte(t->reader); // skip '0'
      continue;
    }

    byte digit = next_byte - '0';
    if (_jtok_uint64_will_overflow(value, digit)) {
      return (_jtok_uint_reading_result){0};
    }
    value = (value * 10) + digit;
    reader_read_byte(t->reader); // skip the digit we just processed
  }

  return (_jtok_uint_reading_result){.leading_zeroes = leading_zeroes, .value = value};
}

json_token _jtok_parse_number(json_tokenizer *t) {
  reader_snapshot snapshot = reader_take_snapshot(t->reader);
  json_number_token number = {0};

  if (reader_peek_byte(t->reader) == '-') {
    if (reader_get_bytes_available(t->reader) < 2) { // lonely `-` is invalid
      reader_restore_snapshot(t->reader, snapshot);
      return INVALID_JSON_TOKEN;
    }
    number.flags |= JSON_NUMBER_IS_NEGATIVE;
    reader_read_byte(t->reader); // skip '-'
  }

  _jtok_uint_reading_result int_part = _jtok_read_uint(t);
  if (_jtok_uint_is_invalid(int_part) || int_part.leading_zeroes > 1 || (int_part.leading_zeroes > 0 && int_part.value > 0)) {
    reader_restore_snapshot(t->reader, snapshot);
    return INVALID_JSON_TOKEN;
  }
  if (int_part.leading_zeroes > 0) {
    number.flags |= JSON_NUMBER_INTEGER_PART_IS_ZERO;
  } else {
    number.integer_part = int_part.value;
  }

  // float part
  if (reader_get_bytes_available(t->reader) >= 1 && reader_peek_byte(t->reader) == '.') {
    if (reader_get_bytes_available(t->reader) < 2) { // `123.` is invalid
      reader_restore_snapshot(t->reader, snapshot);
      return INVALID_JSON_TOKEN;
    }
    number.flags |= JSON_NUMBER_HAS_FRACTION;
    reader_read_byte(t->reader); // skip '.'

    _jtok_uint_reading_result frac_part = _jtok_read_uint(t);
    if (_jtok_uint_is_invalid(frac_part)) { // `1.e` is invalid
      reader_restore_snapshot(t->reader, snapshot);
      return INVALID_JSON_TOKEN;
    }
    number.fraction_part = frac_part.value;
    number.fraction_leading_zeroes = frac_part.leading_zeroes;
  }

  if (reader_get_bytes_available(t->reader) >= 1) {
    byte next_byte = reader_peek_byte(t->reader);
    if (next_byte == 'e' || next_byte == 'E') {
      if (reader_get_bytes_available(t->reader) < 2) { // `1e` is invalid
        reader_restore_snapshot(t->reader, snapshot);
        return INVALID_JSON_TOKEN;
      }

      number.flags |= JSON_NUMBER_HAS_EXPONENT;
      if (next_byte == 'E') {
        number.flags |= JSON_NUMBER_EXPONENT_UPPERCASE;
      }
      reader_read_byte(t->reader); // skip 'e'

      next_byte = reader_peek_byte(t->reader);
      if (next_byte == '-' || next_byte == '+') {
        if (reader_get_bytes_available(t->reader) < 2) { // `1e-` is invalid
          reader_restore_snapshot(t->reader, snapshot);
          return INVALID_JSON_TOKEN;
        }
        number.flags |= next_byte == '-' ? JSON_NUMBER_EXPONENT_IS_NEGATIVE : JSON_NUMBER_EXPONENT_HAS_PLUS;
        reader_read_byte(t->reader); // skip '-'
      }

      _jtok_uint_reading_result expo_part = _jtok_read_uint(t);
      if (_jtok_uint_is_invalid(expo_part) || expo_part.value > UINT16_MAX) {
        reader_restore_snapshot(t->reader, snapshot);
        return INVALID_JSON_TOKEN;
      }

      number.exponent_part = expo_part.value;
      number.exponent_leading_zeroes = expo_part.leading_zeroes;
    }
  }

  return _jtok_yield_multibyte_token(t, (json_token){.kind = JSON_TOKEN_NUMBER, .number = number}, snapshot);
}

json_token_kind _jtok_detect_nonstring_token_start(byte b) {
  switch (b) {
  case '{':
    return JSON_TOKEN_OBJECT_OPEN;
  case '}':
    return JSON_TOKEN_OBJECT_CLOSE;
  case '[':
    return JSON_TOKEN_ARRAY_OPEN;
  case ']':
    return JSON_TOKEN_ARRAY_CLOSE;
  case 't':
    return JSON_TOKEN_TRUE;
  case 'f':
    return JSON_TOKEN_FALSE;
  case 'n':
    return JSON_TOKEN_NULL;
  case ',':
    return JSON_TOKEN_VALUE_SEPARATOR;
  case ':':
    return JSON_TOKEN_KV_SEPARATOR;
  case ' ':
  case '\n':
  case '\r':
  case '\t':
    return JSON_TOKEN_WHITESPACE;
  case '"':
    return JSON_TOKEN_STRING_DELIMITER;
  default:
    if ((b >= '0' && b <= '9') || b == '-') {
      return JSON_TOKEN_NUMBER;
    } else if (b == UTF8_BOM[0]) {
      return JSON_TOKEN_BOM;
    } else {
      return 0;
    }
  }
}

// parse any token that is not a part of string
json_token _jtok_parse_nonstring_token(json_tokenizer *t) {
  byte b = reader_peek_byte(t->reader);
  json_token_kind kind = _jtok_detect_nonstring_token_start(b);
  switch (kind) {
  case JSON_TOKEN_INVALID:
    return INVALID_JSON_TOKEN;
  case JSON_TOKEN_TRUE:
    return _jtok_parse_fixed_token(t, kind, _JTOK_TRUE_BYTES, sizeof(_JTOK_TRUE_BYTES));
  case JSON_TOKEN_FALSE:
    return _jtok_parse_fixed_token(t, kind, _JTOK_FALSE_BYTES, sizeof(_JTOK_FALSE_BYTES));
  case JSON_TOKEN_NULL:
    return _jtok_parse_fixed_token(t, kind, _JTOK_NULL_BYTES, sizeof(_JTOK_NULL_BYTES));
  case JSON_TOKEN_BOM:
    return _jtok_parse_fixed_token(t, kind, UTF8_BOM, sizeof(UTF8_BOM));
  case JSON_TOKEN_NUMBER:
    return _jtok_parse_number(t);
  case JSON_TOKEN_WHITESPACE:
    return _jtok_yield_singlebyte_token(t, (json_token){.kind = JSON_TOKEN_WHITESPACE, .character = {.character = b}});
  default:
    return _jtok_yield_singlebyte_token(t, (json_token){.kind = kind, .character = {.character = b}});
  }
}

INLINE json_state json_tokenizer_get_current_state(json_tokenizer *t) {
  return jsm_get_current_state(&t->jsm);
}

/** Parse next token from the reader.
Returns token of kind JSON_TOKEN_INVALID if parsing is impossible;
in this case, no bytes are consumed from the reader. */
json_token json_tokenizer_parse_next(json_tokenizer *t) {
  assert(reader_get_bytes_available(t->reader) > 0 && "JSON tokenizer expects at least one byte in its reader");
  if (json_is_string_state(json_tokenizer_get_current_state(t))) {
    return _jtok_parse_string_token(t);
  } else {
    return _jtok_parse_nonstring_token(t);
  }
}

/** Returns true if the tokenizer have read one whole JSON completely.
In this state, tokenizer will only read whitespace tokens
(because we allow to have infinite number of whitespace tokens after the end of the actual value).
You can reset the tokenizer if you want to reuse it. */
INLINE bool json_tokenizer_is_done(json_tokenizer *t) {
  return json_tokenizer_get_current_state(t) == JSON_STATE_ROOT;
}