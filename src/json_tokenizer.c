#pragma once
#include "commons.c"
#include "context.c"
#include "limits.h"
#include "queue.c"
#include "stack.c"
#include "utf8.c"
#include <complex.h>
#include <inttypes.h>
#include <stdint.h>

// TODO: reduce this number, it doesn't need to be that long
#define JTOK_MAX_CHARS_LENGTH 64

/** Types of state in which JSON tokens may appear.
Different types of states may contain different types of tokens
("}" cannot appear in the middle of the array etc) */
typedef enum {
  JSON_STATE_ROOT = 1,            // expecting BOM or value
  JSON_STATE_VALUE,               // expecting some sort of value
  JSON_STATE_OBJECT,              // expecting key, or closing token
  JSON_STATE_OBJECT_KV_SEPARATOR, // expecting ":"
  JSON_STATE_ARRAY,               // expecting value, or closing token
  JSON_STATE_STRING,              // expecting characters
  JSON_STATE_OBJECT_KEY,          // like string, but hints about further state changes
  JSON_STATE_NUMBER,              // expecting numeric components
} json_state_type;

typedef enum {
  _JTOK_NUMBER_STATE_START = 1,
  _JTOK_NUMBER_STATE_FRACTION,
  _JTOK_NUMBER_STATE_EXPONENT
} _jtok_number_state;

typedef enum {
  // simple tokens
  JSON_TOKEN_OBJECT_OPEN = 1, // {
  JSON_TOKEN_OBJECT_CLOSE,    // }
  JSON_TOKEN_ARRAY_OPEN,      // [
  JSON_TOKEN_ARRAY_CLOSE,     // ]
  JSON_TOKEN_TRUE,            // true
  JSON_TOKEN_FALSE,           // false
  JSON_TOKEN_NULL,            // null
  JSON_TOKEN_COMMA,           // element separator
  JSON_TOKEN_QUOTES,          // string start or end
  JSON_TOKEN_COLON,           // : separator between key and value
  JSON_TOKEN_BOM,             // byte-order mark that may appear at the start of the JSON

  // character tokens
  JSON_TOKEN_WHITESPACE,        // ' ', '\n', '\r', '\t' between elements and commas
  JSON_TOKEN_ESCAPED_CHARACTER, // \n, \r, \\ and other single-character escape sequences

  // integer tokens
  JSON_TOKEN_CHARACTER,        // normal, non-escaped element of a string. one unicode codepoint; `int_token->value` contains sequence of utf-8 bytes merged into one uint64, not decoded codepoint
  JSON_TOKEN_ESCAPED_CHARCODE, // \u1234

  // number tokens
  JSON_TOKEN_NUMBER, // 12.345, 1e10. every number that is not 64-bit safe integer
} json_token_kind;

/** Tokens that store exactly 1 character of information with them */
typedef struct {
  byte character;
} json_character_token;

/** Tokens that store 64 bits of information with them */
typedef struct {
  uint64_t value;
  byte mod; // for numbers: '+', '-' or 0 (for "no sign"). for utf-8: byte length.
} json_integer_token;

/** Tokens that contains a number in JSON sense. */
typedef struct {
  // TODO: consider using smaller sizes for fraction/exponent, this is the largest struct in the union
  uint64_t integer_part;
  uint64_t fraction_part;
  uint64_t exponent_part;
  bool has_fraction_part;
  byte exponent_symbol; // 'e', 'E' or 0, meaning no exponent
  byte exponent_sign;   // '-', '+' or 0
  byte sign;            // '-', or 0
} json_number_token;

/** Any of the tokens described above. */
typedef struct {
  json_token_kind kind;

  union {
    json_character_token char_token;
    json_integer_token int_token;
    json_number_token number_token;
  };
} json_token;

typedef enum {
  _JTOK_PASS = 1,
  _JTOK_OK,
  _JTOK_ERROR
} _jtok_success_state;

/** Json tokenizer is a state machine that converts bytes of encoded JSON into tokens that describe contents of the JSON */
typedef struct {
  context *context;
  /** Parsed tokens ready for consumption */
  queue token_queue;
  stack state_stack;
  /** Unparsed characters.
  Length of this field is determined mostly by max possible meaningful length of a number */
  byte chars[JTOK_MAX_CHARS_LENGTH];
  /** Next free index in `characters` field */
  size_t chars_length;
  json_token_kind last_nonws_read_token_kind;
} json_tokenizer;

void json_tokenizer_deinit(json_tokenizer *tokenizer) {
  queue_deinit(&tokenizer->token_queue);
  stack_deinit(&tokenizer->state_stack);
}

bool json_tokenizer_init(json_tokenizer *tokenizer, context *context) {
  if (!queue_init(&tokenizer->token_queue, context, sizeof(json_token))) {
    return false;
  }

  if (!stack_init(&tokenizer->state_stack, context, sizeof(json_state_type))) {
    queue_deinit(&tokenizer->token_queue);
    return false;
  }

  tokenizer->context = context;
  tokenizer->chars_length = 0;
  tokenizer->last_nonws_read_token_kind = JSON_TOKEN_WHITESPACE;

  // TODO: make sure that here (and in other places) overflow isn't possible
  // as in, a million '[' should be treated as invalid json instead of allocating million states
  json_state_type *slot = stack_push(&tokenizer->state_stack);
  // this push would never fail, as stack have already allocated memory for some values
  *slot = JSON_STATE_ROOT;

  return true;
}

/** Returns true if the tokenizer is at its initial/final state.
It's an easy way to check if a JSON was properly formatted. If it is - after stream consumption tokenizer will be empty. */
bool json_tokenizer_is_empty(json_tokenizer *tokenizer) {
  return tokenizer->chars_length == 0 && stack_get_count(&tokenizer->state_stack) == 1 && queue_get_count(&tokenizer->token_queue) == 0;
}

/** If there is a token to consume - the token is returned, null otherwise. */
json_token *json_tokenizer_consume(json_tokenizer *tokenizer) {
  if (queue_get_count(&tokenizer->token_queue) < 1) {
    return NULL;
  }

  return queue_pop(&tokenizer->token_queue);
}

_jtok_success_state _jtok_push_simple_token(json_tokenizer *t, json_token_kind kind) {
  json_token *slot = queue_push(&t->token_queue);
  if (!slot) {
    return _JTOK_ERROR;
  }
  slot->kind = kind;
  t->chars_length = 0;
  t->last_nonws_read_token_kind = kind;
  return _JTOK_OK;
}

_jtok_success_state _jtok_push_char_token(json_tokenizer *t, json_token_kind kind, byte character) {
  json_token *slot = queue_push(&t->token_queue);
  if (!slot) {
    return _JTOK_ERROR;
  }
  slot->kind = kind;
  slot->char_token.character = character;
  t->chars_length = 0;
  if (kind != JSON_TOKEN_WHITESPACE) {
    t->last_nonws_read_token_kind = kind;
  }
  return _JTOK_OK;
}

_jtok_success_state _jtok_push_int_token(json_tokenizer *t, json_token_kind kind, uint64_t value, byte sign) {
  json_token *slot = queue_push(&t->token_queue);
  if (!slot) {
    return _JTOK_ERROR;
  }
  slot->kind = kind;
  slot->int_token.value = value;
  slot->int_token.mod = sign;
  t->chars_length = 0;
  t->last_nonws_read_token_kind = kind;
  return _JTOK_OK;
}

_jtok_success_state _jtok_push_number_token(json_tokenizer *t, byte sign, uint64_t integer_part, bool has_fraction, uint64_t fraction_part, byte exponent_symbol, byte exponent_sign,
                                            uint64_t exponent_part) {
  json_token *slot = queue_push(&t->token_queue);
  if (!slot) {
    return _JTOK_ERROR;
  }

  slot->kind = JSON_TOKEN_NUMBER;
  slot->number_token.integer_part = integer_part;
  slot->number_token.fraction_part = fraction_part;
  slot->number_token.exponent_part = exponent_part;
  slot->number_token.has_fraction_part = has_fraction;
  slot->number_token.sign = sign;
  slot->number_token.exponent_symbol = exponent_symbol;
  slot->number_token.exponent_sign = exponent_sign;

  t->chars_length = 0;
  t->last_nonws_read_token_kind = JSON_TOKEN_NUMBER;
  return _JTOK_OK;
}

_jtok_success_state _jtok_try_bom(json_tokenizer *t) {
  if (t->chars_length != UTF8_BOM_LENGTH || !utf8_can_bytes_be_bom_start(t->chars, t->chars_length)) {
    return _JTOK_PASS;
  }
  return _jtok_push_simple_token(t, JSON_TOKEN_BOM);
}

_jtok_success_state _jtok_try_object_open(json_tokenizer *t) {
  if (t->chars_length == 1 && t->chars[0] == '{') {
    return _jtok_push_simple_token(t, JSON_TOKEN_OBJECT_OPEN);
  }
  return _JTOK_PASS;
}

_jtok_success_state _jtok_try_object_close(json_tokenizer *t) {
  if (t->chars_length == 1 && t->chars[0] == '}') {
    return _jtok_push_simple_token(t, JSON_TOKEN_OBJECT_CLOSE);
  }
  return _JTOK_PASS;
}

_jtok_success_state _jtok_try_array_open(json_tokenizer *t) {
  if (t->chars_length == 1 && t->chars[0] == '[') {
    return _jtok_push_simple_token(t, JSON_TOKEN_ARRAY_OPEN);
  }
  return _JTOK_PASS;
}

_jtok_success_state _jtok_try_array_close(json_tokenizer *t) {
  if (t->chars_length == 1 && t->chars[0] == ']') {
    return _jtok_push_simple_token(t, JSON_TOKEN_ARRAY_CLOSE);
  }
  return _JTOK_PASS;
}

_jtok_success_state _jtok_try_true(json_tokenizer *t) {
  if (t->chars_length == 4 && t->chars[0] == 't' && t->chars[1] == 'r' && t->chars[2] == 'u' && t->chars[3] == 'e') {
    return _jtok_push_simple_token(t, JSON_TOKEN_TRUE);
  }
  return _JTOK_PASS;
}

_jtok_success_state _jtok_try_false(json_tokenizer *t) {
  if (t->chars_length == 5 & t->chars[0] == 'f' && t->chars[1] == 'a' && t->chars[2] == 'l' && t->chars[3] == 's' && t->chars[4] == 'e') {
    return _jtok_push_simple_token(t, JSON_TOKEN_FALSE);
  }
  return _JTOK_PASS;
}

_jtok_success_state _jtok_try_null(json_tokenizer *t) {
  if (t->chars_length == 4 && t->chars[0] == 'n' && t->chars[1] == 'u' && t->chars[2] == 'l' && t->chars[3] == 'l') {
    return _jtok_push_simple_token(t, JSON_TOKEN_NULL);
  }
  return _JTOK_PASS;
}

_jtok_success_state _jtok_try_comma(json_tokenizer *t) {
  if (t->chars_length == 1 && t->chars[0] == ',') {
    return _jtok_push_simple_token(t, JSON_TOKEN_COMMA);
  }
  return _JTOK_PASS;
}

_jtok_success_state _jtok_try_colon(json_tokenizer *t) {
  if (t->chars_length == 1 && t->chars[0] == ':') {
    return _jtok_push_simple_token(t, JSON_TOKEN_COLON);
  }
  return _JTOK_PASS;
}

_jtok_success_state _jtok_try_quotes(json_tokenizer *t) {
  if (t->chars_length == 1 && t->chars[0] == '"') {
    return _jtok_push_simple_token(t, JSON_TOKEN_QUOTES);
  }
  return _JTOK_PASS;
}

_jtok_success_state _jtok_try_whitespace(json_tokenizer *t) {
  byte first = t->chars[0];
  if (t->chars_length == 1 && (first == ' ' || first == '\n' || first == '\r' || first == '\t')) {
    return _jtok_push_char_token(t, JSON_TOKEN_WHITESPACE, t->chars[0]);
  }
  return _JTOK_PASS;
}

const uint64_t _jtok_not_a_hex_character = 0xff;
uint64_t _jtok_parse_hex(byte hex_char) {
  if (hex_char >= '0' && hex_char <= '9') {
    return hex_char - '0';
  } else if (hex_char >= 'a' && hex_char <= 'f') {
    return (hex_char - 'a') + 10;
  } else if (hex_char >= 'A' && hex_char <= 'F') {
    return (hex_char - 'A') + 10;
  } else {
    return _jtok_not_a_hex_character;
  }
}

// this assumes that string-ending quotes have been processed already
_jtok_success_state _jtok_try_parse_next_string_part(json_tokenizer *t) {
  byte first = t->chars[0];
  if (t->chars_length == 2 && first == '\\') {
    // normal escape sequence
    byte c = t->chars[1];
    // we are free to return here, as backslash cannot be followed just by any random character, only those selected few
    if (c == '\\' || c == '"' || c == '/' || c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't') {
      return _jtok_push_char_token(t, JSON_TOKEN_ESCAPED_CHARACTER, c);
    }
    return _JTOK_PASS;
  } else if (t->chars_length == 6 && first == '\\' && t->chars[1] == 'u') {
    // utf-16 charcode escape sequence
    uint64_t a = _jtok_parse_hex(t->chars[2]);
    uint64_t b = _jtok_parse_hex(t->chars[3]);
    uint64_t c = _jtok_parse_hex(t->chars[4]);
    uint64_t d = _jtok_parse_hex(t->chars[5]);
    if (a == _jtok_not_a_hex_character || b == _jtok_not_a_hex_character || c == _jtok_not_a_hex_character || d == _jtok_not_a_hex_character) {
      return _JTOK_PASS;
    }
    // 4 hex bytes are stored like that to preserve case
    // as we must not lose any data at all during tokenization
    uint64_t code = (t->chars[0] << 0) | (t->chars[1] << 8) | (t->chars[2] << 16) | (t->chars[3] << 24);
    return _jtok_push_int_token(t, JSON_TOKEN_ESCAPED_CHARCODE, code, 0);
  }

  // trying to parse normal utf-8 byte sequence
  size_t codepoint_length = utf8_get_sequence_length_by_first_byte(t->chars[0]);
  if (t->chars_length != codepoint_length) {
    // this includes codepoint_length of 0
    return _JTOK_PASS;
  }

  if (!utf8_are_continuation_bytes_valid(t->chars, codepoint_length)) {
    return _JTOK_PASS;
  }

  uint64_t result = first;
  for (size_t i = 1; i < codepoint_length; i++) {
    // note that it's utf-8 bytes compressed into uint64_t, not a decoded codepoint
    result |= t->chars[i] << (8 * i);
  }
  return _jtok_push_int_token(t, JSON_TOKEN_CHARACTER, result, codepoint_length);
}

_jtok_success_state _jtok_push_state(json_tokenizer *t, json_state_type state) {
  // printf("push state: %i\n", state);
  json_state_type *slot = stack_push(&t->state_stack);
  if (!slot) {
    return _JTOK_ERROR;
  }
  *slot = state;
  return _JTOK_OK;
}

_jtok_success_state _jtok_pop_state(json_tokenizer *t) {
  json_state_type *old_state_slot = stack_pop(&t->state_stack);
  json_state_type old_state = *old_state_slot;
  json_state_type *base_state_slot = stack_peek(&t->state_stack);
  json_state_type base_state = *base_state_slot;
  // printf("pop state: %i -> %i\n", old_state, base_state);

  switch (base_state) {
  case JSON_STATE_OBJECT:
    if (old_state == JSON_STATE_OBJECT_KEY) {
      return _jtok_push_state(t, JSON_STATE_OBJECT_KV_SEPARATOR);
    }
    return _JTOK_OK;
  case JSON_STATE_OBJECT_KV_SEPARATOR:
    // this pops to JSON_STATE_OBJECT after reading a value
    return _jtok_pop_state(t);
  case JSON_STATE_VALUE:
    // after a composite value, like object, string, array or number, is finished reading - its state is popped
    // and Value state is exposed. but Value must not immediately follow another Value
    // therefore, we must pop this state to expose underlying state
    return _jtok_pop_state(t);
  default:
    return _JTOK_OK;
  }
}

const uint64_t tenth_of_max_uint64 = UINT64_MAX / 10;
bool _jtok_uint64_will_overflow(uint64_t value, byte addition) {
  return value >= tenth_of_max_uint64 - addition;
}

_jtok_success_state _jtok_try_produce_number(json_tokenizer *t, size_t end_offset) {
  _jtok_number_state state = _JTOK_NUMBER_STATE_START;
  int state_digits = 0;
  uint64_t integer_part = 0;
  uint64_t fraction_part = 0;
  uint64_t exponent_part = 0;
  bool has_fraction_part = false;
  byte exponent_symbol = 0;
  byte exponent_sign = 0;
  byte sign = 0;
  for (size_t i = 0; i < t->chars_length - end_offset; i++) {
    byte c = t->chars[i];
    switch (state) {
    case _JTOK_NUMBER_STATE_START:
      if (c == '-') {
        sign = c;
      } else if (c >= '0' && c <= '9') {
        byte new_digit = c - '0';
        if (_jtok_uint64_will_overflow(integer_part, new_digit)) {
          return _JTOK_PASS;
        }
        integer_part = (integer_part * 10) + new_digit;
        state_digits++;
      } else if (c == '.') {
        if (state_digits == 0) {
          // .123 is invalid
          return _JTOK_PASS;
        }
        has_fraction_part = true;
        state = _JTOK_NUMBER_STATE_FRACTION;
        state_digits = 0;
      } else if (c == 'e' || c == 'E') {
        if (state_digits == 0) {
          // -e123 is invalid
          return _JTOK_PASS;
        }
        exponent_symbol = c;
        state = _JTOK_NUMBER_STATE_EXPONENT;
        state_digits = 0;
      } else {
        return _JTOK_PASS;
      }
      continue;
    case _JTOK_NUMBER_STATE_FRACTION:
      if (c >= '0' && c <= '9') {
        byte new_digit = c - '0';
        if (_jtok_uint64_will_overflow(fraction_part, new_digit)) {
          return _JTOK_PASS;
        }
        fraction_part = (fraction_part * 10) + new_digit;
        state_digits++;
      } else if (c == 'e' || c == 'E') {
        if (state_digits == 0) {
          // 1.e123 is invalid
          return _JTOK_PASS;
        }
        exponent_symbol = c;
        state = _JTOK_NUMBER_STATE_EXPONENT;
        state_digits = 0;
      } else {
        return _JTOK_PASS;
      }
      continue;
    case _JTOK_NUMBER_STATE_EXPONENT:
      if (c == '-' || c == '+') {
        if (state_digits != 0) {
          // 1e1+ is invalid
          return _JTOK_PASS;
        }
        exponent_sign = c;
      } else if (c >= '0' && c <= '9') {
        byte new_digit = c - '0';
        if (_jtok_uint64_will_overflow(exponent_part, new_digit)) {
          return _JTOK_PASS;
        }
        exponent_part = (exponent_part * 10) + new_digit;
        state_digits++;
      } else {
        return _JTOK_PASS;
      }
      continue;
    }
  }

  if (state_digits == 0) {
    // "", "1.", "1e", "1.2e" are all invalid numbers
    return _JTOK_PASS;
  }

  _jtok_success_state result = _jtok_push_number_token(t, sign, integer_part, has_fraction_part, fraction_part, exponent_symbol, exponent_sign, exponent_part);
  if (result != _JTOK_OK) {
    return result;
  }

  return _jtok_pop_state(t);
}

bool _jtok_is_a_number_starter(byte last_char) {
  return (last_char >= '0' && last_char <= '9') || last_char == '-';
}

// assumes the tokenizer is in number-parsing state already
_jtok_success_state _jtok_try_update_number(json_tokenizer *t) {
  byte last_char = t->chars[t->chars_length - 1];
  if ((last_char >= '0' && last_char <= '9') || last_char == 'e' || last_char == 'E' || last_char == '+' || last_char == '-' || last_char == '.') {
    return _JTOK_PASS; // pass has slightly different value with numbers
  }
  _jtok_success_state result = _jtok_try_produce_number(t, 1);
  if (result != _JTOK_OK) {
    return result;
  }
  // put the last character in the right place
  // assuming it will be consumed by the caller later
  t->chars[0] = last_char;
  t->chars_length++;
  return result;
}

_jtok_success_state _jtok_try_start_string(json_tokenizer *t, json_state_type state) {
  _jtok_success_state result = _jtok_try_quotes(t);
  if (result == _JTOK_OK) {
    return _jtok_push_state(t, state);
  }
  return result;
}

_jtok_success_state _jtok_try_end_string(json_tokenizer *t) {
  _jtok_success_state result = _jtok_try_quotes(t);
  if (result == _JTOK_OK) {
    return _jtok_pop_state(t);
  }
  return result;
}

_jtok_success_state _jtok_try_start_object(json_tokenizer *t) {
  _jtok_success_state result = _jtok_try_object_open(t);
  if (result == _JTOK_OK) {
    return _jtok_push_state(t, JSON_STATE_OBJECT);
  }
  return result;
}

_jtok_success_state _jtok_try_end_object(json_tokenizer *t) {
  _jtok_success_state result = _jtok_try_object_close(t);
  if (result == _JTOK_OK) {
    return _jtok_pop_state(t);
  }
  return result;
}

_jtok_success_state _jtok_try_start_array(json_tokenizer *t) {
  _jtok_success_state result = _jtok_try_array_open(t);
  if (result == _JTOK_OK) {
    return _jtok_push_state(t, JSON_STATE_ARRAY);
  }
  return result;
}

_jtok_success_state _jtok_try_end_array(json_tokenizer *t) {
  _jtok_success_state result = _jtok_try_array_close(t);
  if (result == _JTOK_OK) {
    return _jtok_pop_state(t);
  }
  return result;
}

_jtok_success_state _jtok_try_start_number(json_tokenizer *t) {
  if (t->chars_length == 1 && _jtok_is_a_number_starter(t->chars[0])) {
    return _jtok_push_state(t, JSON_STATE_NUMBER);
  }
  return _JTOK_PASS;
}

_jtok_success_state _jtok_try_const_value(json_tokenizer *t) {
  // those values are simple and don't require a separate state to parse them
  // because of that, we need to manually pop Value state
  // (in case of composite values, Value state will be popped on popping state of that composite value)
  _jtok_success_state result = _jtok_try_true(t);
  if (result == _JTOK_PASS) {
    result = _jtok_try_false(t);
    if (result == _JTOK_PASS) {
      result = _jtok_try_null(t);
    }
  }
  if (result == _JTOK_OK) {
    return _jtok_pop_state(t);
  }
  return result;
}

_jtok_success_state _jtok_try_value(json_tokenizer *t) {
  _jtok_success_state result = _jtok_try_start_string(t, JSON_STATE_STRING);
  if (result == _JTOK_PASS) {
    result = _jtok_try_start_number(t);
    if (result == _JTOK_PASS) {
      result = _jtok_try_start_array(t);
      if (result == _JTOK_PASS) {
        result = _jtok_try_start_object(t);
        if (result == _JTOK_PASS) {
          result = _jtok_try_const_value(t);
        }
      }
    }
  }
  return result;
}

_jtok_success_state _jtok_try_tokenize(json_tokenizer *t) {
  json_state_type *state_slot = stack_peek(&t->state_stack);
  _jtok_success_state result = _JTOK_PASS;

  // printf("tokenize: %.*s (state = %i)\n", (int)t->chars_length, t->chars, *state_slot);

  switch (*state_slot) {
  case JSON_STATE_ROOT:
    // note that JSON_TOKEN_WHITESPACE is the default value for that field; it's impossible to have this situation otherwise
    // so this condition is "only proceed if we just red the BOM, or if this is very beginning of the stream"
    // this condition exists because two JSON values in a row are not a valid JSON
    if (t->last_nonws_read_token_kind != JSON_TOKEN_BOM && t->last_nonws_read_token_kind != JSON_TOKEN_WHITESPACE) {
      return _JTOK_PASS;
    }

    if (t->last_nonws_read_token_kind != JSON_TOKEN_BOM && utf8_can_bytes_be_bom_start(t->chars, t->chars_length)) {
      return _jtok_try_bom(t);
    }

    if (_jtok_push_state(t, JSON_STATE_VALUE) != _JTOK_OK) {
      return _JTOK_ERROR;
    }
    return _jtok_try_tokenize(t);

  case JSON_STATE_VALUE:
    result = _jtok_try_value(t);
    if (result == _JTOK_PASS) {
      result = _jtok_try_whitespace(t);
    }
    return result;

  case JSON_STATE_OBJECT:
    if (t->last_nonws_read_token_kind != JSON_TOKEN_OBJECT_OPEN && t->last_nonws_read_token_kind != JSON_TOKEN_COMMA) {
      result = _jtok_try_comma(t);
    }
    if (result == _JTOK_PASS) {
      result = _jtok_try_start_string(t, JSON_STATE_OBJECT_KEY);
      if (result == _JTOK_PASS) {
        result = _jtok_try_end_object(t);
        if (result == _JTOK_PASS) {
          result = _jtok_try_whitespace(t);
        }
      }
    }
    return result;

  case JSON_STATE_OBJECT_KV_SEPARATOR:
    if (t->last_nonws_read_token_kind != JSON_TOKEN_COLON) {
      result = _jtok_try_colon(t);
      if (result == _JTOK_OK) {
        return _jtok_push_state(t, JSON_STATE_VALUE);
      }
    }
    if (result == _JTOK_PASS) {
      result = _jtok_try_whitespace(t);
    }
    return result;

  case JSON_STATE_ARRAY:
    result = _jtok_try_whitespace(t);
    if (result == _JTOK_PASS) {
      result = _jtok_try_end_array(t);
    }
    if (result != _JTOK_PASS) {
      return result;
    }

    if (t->last_nonws_read_token_kind != JSON_TOKEN_ARRAY_OPEN && t->last_nonws_read_token_kind != JSON_TOKEN_COMMA) {
      result = _jtok_try_comma(t);
      if (result == _JTOK_OK) {
        return _jtok_push_state(t, JSON_STATE_VALUE);
      }
      return result;
    }

    // it's not comma, or whitespace, or array end, which means it can only be a value
    if (_jtok_push_state(t, JSON_STATE_VALUE) != _JTOK_OK) {
      return _JTOK_ERROR;
    }

    return _jtok_try_tokenize(t);

  case JSON_STATE_STRING:
  case JSON_STATE_OBJECT_KEY:
    result = _jtok_try_end_string(t);
    if (result == _JTOK_PASS) {
      result = _jtok_try_parse_next_string_part(t);
    }
    return result;

  case JSON_STATE_NUMBER:
    result = _jtok_try_update_number(t);
    if (result == _JTOK_OK) {
      result = _jtok_try_tokenize(t);
    }
    return result;
  }
}

// TODO: think about using nodiscard modifier here and in other failable places?
/** Add a byte to the tokenizer. This may cause some amount of tokens to appear for consumption.
Returns true if byte was consumed successfully. */
bool json_tokenizer_push(json_tokenizer *t, byte b) {
  if (t->chars_length == JTOK_MAX_CHARS_LENGTH - 1) {
    // broken json, or maybe overly long number
    return false;
  }

  t->chars[t->chars_length] = b;
  t->chars_length++;

  _jtok_success_state tokenize_result = _jtok_try_tokenize(t);
  // printf("tokenize result: %i\n", tokenize_result);
  bool result = tokenize_result != _JTOK_ERROR;

  // printf("after tokenize: %.*s\n", (int)t->chars_length, t->chars);

  return result;
}

/** Call this after you have no more bytes to push into the tokenizer.
This will attempt to consume all remaining buffer bytes, and may produce a number. */
bool json_tokenizer_finalize(json_tokenizer *t) {
  json_state_type *state_slot = stack_peek(&t->state_stack);
  // printf("finalize: %.*s (state = %i)\n", (int)t->chars_length, t->chars, *state_slot);
  _jtok_success_state result = _JTOK_PASS;
  if (*state_slot == JSON_STATE_NUMBER) {
    result = _jtok_try_produce_number(t, 0);
  }

  // printf("after finalize: %.*s\n", (int)t->chars_length, t->chars);

  return result != _JTOK_ERROR;
}