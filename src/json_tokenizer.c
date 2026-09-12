#pragma once
#include "commons.c"
#include "context.c"
#include "limits.h"
#include "queue.c"
#include "stack.c"
#include <stdint.h>

#define JTOK_MAX_CHARS_LENGTH 64

/** Types of context in which JSON tokens may appear.
Different types of contexts may contain different types of tokens
("}" cannot appear in the middle of the array etc) */
typedef enum {
  JSON_CONTEXT_ROOT = 1,
  JSON_CONTEXT_OBJECT,
  JSON_CONTEXT_ARRAY,
  JSON_CONTEXT_STRING,
  JSON_CONTEXT_NUMBER
} json_context_type;

typedef enum {
  _JTOK_NUMBER_STATE_START = 1,
  _JTOK_NUMBER_STATE_FRACTION,
  _JTOK_NUMBER_STATE_EXPONENT
} _jtok_number_state;

typedef enum {
  // simple tokens
  JSON_TOKEN_OBJECT_START = 1, // {
  JSON_TOKEN_OBJECT_END,       // }
  JSON_TOKEN_ARRAY_START,      // [
  JSON_TOKEN_ARRAY_END,        // ]
  JSON_TOKEN_TRUE,             // true
  JSON_TOKEN_FALSE,            // false
  JSON_TOKEN_NULL,             // null
  JSON_TOKEN_COMMA,            // element separator
  JSON_TOKEN_QUOTES,           // string start or end
  JSON_TOKEN_COLON,            // : separator between key and value
  JSON_TOKEN_BOM,              // byte-order mark that may appear at the start of the JSON

  // character tokens
  JSON_TOKEN_WHITESPACE,        // ' ', '\n', '\r', '\t' between elements and commas
  JSON_TOKEN_ESCAPED_CHARACTER, // \n, \r, \\ and other single-character escape sequences

  // integer tokens
  JSON_TOKEN_CHARACTER,        // normal, non-escaped element of a string. one unicode codepoint; `int_token->value` contains sequence of utf-8 bytes merged into one uint64, not decoded codepoint
  JSON_TOKEN_ESCAPED_CHARCODE, // \u1234
  JSON_TOKEN_INTEGER,          // 12345, -12345, +12345. only 64-bit safe integers

  // string tokens
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

/** Tokens that contain raw unparsed string with them that must be stored as such */
typedef struct {
  byte *value;
  size_t length;
} json_unparsed_token;

/** Any of the tokens described above. */
typedef struct {
  json_token_kind kind;

  union {
    json_character_token char_token;
    json_integer_token int_token;
    json_unparsed_token raw_token;
  };
} json_token;

/** Json tokenizer is a state machine that converts bytes of encoded JSON into tokens that describe contents of the JSON */
typedef struct {
  context *context;
  /** Parsed tokens ready for consumption */
  queue token_queue;
  stack context_stack;
  /** Unparsed characters.
  Length of this field is determined mostly by max possible meaningful length of a number */
  byte chars[JTOK_MAX_CHARS_LENGTH];
  /** Next free index in `characters` field */
  size_t chars_length;
} json_tokenizer;

bool json_tokenizer_init(json_tokenizer *tokenizer, context *context) {
  if (!queue_init(&tokenizer->token_queue, context, sizeof(json_token))) {
    return false;
  }

  if (!stack_init(&tokenizer->context_stack, context, sizeof(json_context_type))) {
    queue_deinit(&tokenizer->token_queue);
    return false;
  }

  tokenizer->context = context;
  tokenizer->chars_length = 0;

  return true;
}

void json_tokenizer_deinint(json_tokenizer *tokenizer) {
  queue_deinit(&tokenizer->token_queue);
  stack_deinit(&tokenizer->context_stack);
}

/** Returns true if the tokenizer is at its initial/final state.
It's an easy way to check if a JSON was properly formatted. If it is - after stream consumption tokenizer will be empty. */
bool json_tokenizer_is_empty(json_tokenizer *tokenizer) {
  return queue_get_count(&tokenizer->token_queue) == 0 && stack_get_count(&tokenizer->context_stack) == 0 && tokenizer->chars_length == 0;
}

/** If there is a token to consume - the token is returned, null otherwise. */
json_token *json_tokenizer_consume(json_tokenizer *tokenizer) {
  if (queue_get_count(&tokenizer->token_queue) < 1) {
    return NULL;
  }

  return queue_pop(&tokenizer->token_queue);
}

bool _jtok_push_simple_token(json_tokenizer *t, json_token_kind kind) {
  json_token *slot = queue_push(&t->token_queue);
  if (!slot) {
    return false;
  }
  slot->kind = kind;
  t->chars_length = 0;
  return true;
}

bool _jtok_push_char_token(json_tokenizer *t, json_token_kind kind, byte character) {
  json_token *slot = queue_push(&t->token_queue);
  if (!slot) {
    return false;
  }
  slot->kind = kind;
  slot->char_token.character = character;
  t->chars_length = 0;
  return true;
}

bool _jtok_push_int_token(json_tokenizer *t, json_token_kind kind, uint64_t value, byte sign) {
  json_token *slot = queue_push(&t->token_queue);
  if (!slot) {
    return false;
  }
  slot->kind = kind;
  slot->int_token.value = value;
  slot->int_token.mod = sign;
  t->chars_length = 0;
  return true;
}

bool _jtok_push_raw_token(json_tokenizer *t, json_token_kind kind, size_t end_offset) {
  json_token *slot = queue_push(&t->token_queue);
  if (!slot) {
    return false;
  }

  slot->kind = kind;
  slot->raw_token.length = t->chars_length - end_offset;
  slot->raw_token.value = NULL; // FIXME: allocate and fill string
  t->chars_length = 0;
  return true;
}

bool _jtok_try_bom(json_tokenizer *t) {
  return t->chars_length == 3 && t->chars[0] == 0xEF && t->chars[1] == 0xBB && t->chars[2] == 0xBF && _jtok_push_simple_token(t, JSON_TOKEN_BOM);
}

bool _jtok_try_object_start(json_tokenizer *t) {
  return t->chars_length == 1 && t->chars[0] == '{' && _jtok_push_simple_token(t, JSON_TOKEN_OBJECT_START);
}

bool _jtok_try_object_end(json_tokenizer *t) {
  return t->chars_length == 1 && t->chars[0] == '}' && _jtok_push_simple_token(t, JSON_TOKEN_OBJECT_END);
}

bool _jtok_try_array_start(json_tokenizer *t) {
  return t->chars_length == 1 && t->chars[0] == '[' && _jtok_push_simple_token(t, JSON_TOKEN_ARRAY_START);
}

bool _jtok_try_array_end(json_tokenizer *t) {
  return t->chars_length == 1 && t->chars[0] == ']' && _jtok_push_simple_token(t, JSON_TOKEN_ARRAY_END);
}

bool _jtok_try_true(json_tokenizer *t) {
  return t->chars_length == 4 && t->chars[0] == 't' && t->chars[1] == 'r' && t->chars[2] == 'u' && t->chars[3] == 'e' && _jtok_push_simple_token(t, JSON_TOKEN_TRUE);
}

bool _jtok_try_false(json_tokenizer *t) {
  return t->chars_length == 5 && t->chars[0] == 'f' && t->chars[1] == 'a' && t->chars[2] == 'l' && t->chars[3] == 's' && t->chars[4] == 'e' && _jtok_push_simple_token(t, JSON_TOKEN_FALSE);
}

bool _jtok_try_null(json_tokenizer *t) {
  return t->chars_length == 4 && t->chars[0] == 'n' && t->chars[1] == 'u' && t->chars[2] == 'l' && t->chars[3] == 'l' && _jtok_push_simple_token(t, JSON_TOKEN_NULL);
}

bool _jtok_try_comma(json_tokenizer *t) {
  return t->chars_length == 1 && t->chars[0] == ',' && _jtok_push_simple_token(t, JSON_TOKEN_COMMA);
}

bool _jtok_try_colon(json_tokenizer *t) {
  return t->chars_length == 1 && t->chars[0] == ':' && _jtok_push_simple_token(t, JSON_TOKEN_COLON);
}

bool _jtok_try_quotes(json_tokenizer *t) {
  return t->chars_length == 1 && t->chars[0] == '"' && _jtok_push_simple_token(t, JSON_TOKEN_QUOTES);
}

bool _jtok_is_whitespace_character(byte c) {
  return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

bool _jtok_try_whitespace(json_tokenizer *t) {
  return t->chars_length == 1 && _jtok_is_whitespace_character(t->chars[0]) && _jtok_push_char_token(t, JSON_TOKEN_WHITESPACE, t->chars[0]);
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
bool _jtok_try_parse_next_string_part(json_tokenizer *t) {
  byte first = t->chars[0];

  // utf-8 codepoint parsing
  switch (t->chars_length) {
  case 1:
    // codepoint = first
    return first <= 0x7F && _jtok_push_int_token(t, JSON_TOKEN_CHARACTER, first, 1);
  case 2: {
    // normal escape?
    if (first == '\\') {
      byte c = t->chars[1];
      // we are free to return here, as backslash cannot be followed just by any random character, only those selected few
      return (c == '\\' || c == '"' || c == '/' || c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't') && _jtok_push_char_token(t, JSON_TOKEN_ESCAPED_CHARACTER, c);
    }
    // codepoint = ((first & 0x1F) << 6) | (t->chars[1] & 0x3F)
    return first <= 0xDF && first >= 0xC2 && _jtok_push_int_token(t, JSON_TOKEN_CHARACTER, first << 8 || t->chars[1], 2);
  }

  case 3:
    // codepoint = ((first & 0x0F) << 12) | ((t->chars[1] & 0x3F) << 6) | (t->chars[2] & 0x3F)
    return first <= 0xEF && first >= 0xE0 && _jtok_push_int_token(t, JSON_TOKEN_CHARACTER, first << 16 || t->chars[1] << 8 || t->chars[2], 3);

  case 4:
    // codepoint = ((first & 0x07) << 18) | ((t->chars[1] & 0x3F) << 12) | ((t->chars[2] & 0x3F) << 6) | (t->chars[3] & 0x3F)
    return first <= 0xF4 && first >= 0xF0 && _jtok_push_int_token(t, JSON_TOKEN_CHARACTER, first << 24 || t->chars[1] << 16 || t->chars[2] << 8 || t->chars[3], 4);

  case 6: {
    // charcode escape?
    if (first == '\\') {
      if (t->chars[1] != 'u') {
        return false;
      }
      uint64_t a = _jtok_parse_hex(t->chars[2]);
      uint64_t b = _jtok_parse_hex(t->chars[3]);
      uint64_t c = _jtok_parse_hex(t->chars[4]);
      uint64_t d = _jtok_parse_hex(t->chars[5]);
      if (a == _jtok_not_a_hex_character || b == _jtok_not_a_hex_character || c == _jtok_not_a_hex_character || d == _jtok_not_a_hex_character) {
        return false;
      }
      uint64_t code = (d << 0) || (c << 4) || (b << 8) || (a << 12);
      return _jtok_push_int_token(t, JSON_TOKEN_ESCAPED_CHARCODE, code, 0);
    }
  }

  default:
    // utf-8 only specifies sequences of bytes up to 4
    return false;
  }
}

bool _jtok_try_produce_number(json_tokenizer *t, size_t end_offset) {
  _jtok_number_state state = _JTOK_NUMBER_STATE_START;
  int state_digits = 0;
  bool int_overflow = false;
  uint64_t int_value = 0;
  byte sign = 0;
  for (size_t i = 0; i < t->chars_length - end_offset; i++) {
    byte c = t->chars[i];
    switch (state) {
    case _JTOK_NUMBER_STATE_START:
      if (c == '+' || c == '-') {
        sign = c;
      } else if (c >= '0' && c <= '9') {
        if (int_value >= (UINT64_MAX / 10) - 9) {
          int_overflow = true;
        } else {
          int_value = (int_value * 10) + (c - '0');
        }
        state_digits++;
      } else if (c == '.') {
        if (state_digits == 0) {
          // .123 is invalid
          return false;
        }
        state = _JTOK_NUMBER_STATE_FRACTION;
      } else if (c == 'e' || c == 'E') {
        if (state_digits == 0) {
          // -e123 is invalid
          return false;
        }
        state = _JTOK_NUMBER_STATE_EXPONENT;
      } else {
        return false;
      }
      continue;
    case _JTOK_NUMBER_STATE_FRACTION:
      if (c >= '0' && c <= '9') {
        state_digits++;
      } else if (c == 'e' || c == 'E') {
        if (state_digits == 0) {
          // 1.e123 is invalid
          return false;
        }
        state = _JTOK_NUMBER_STATE_EXPONENT;
      }
      continue;
    case _JTOK_NUMBER_STATE_EXPONENT:
      if (c == '-' || c == '+') {
        if (state_digits != 0) {
          // 1e1+ is invalid
          return false;
        }
      } else if (c >= '0' && c <= '9') {
        state_digits++;
      }
      continue;
    }
  }

  if (state_digits == 0) {
    // "", "1.", "1e", "1.2e" are all invalid
    return false;
  }

  if (state == _JTOK_NUMBER_STATE_START && !int_overflow) {
    return _jtok_push_int_token(t, JSON_TOKEN_INTEGER, int_value, sign);
  } else {
    return _jtok_push_raw_token(t, JSON_TOKEN_INTEGER, end_offset);
  }
}

bool _jtok_is_a_number_starter(json_tokenizer *t, byte last_char) {
  return (last_char >= '0' && last_char <= '9') || last_char == '+' || last_char == '-';
}

// assumes the tokenizer is in number-parsing context already
// `false` means need more characters, `true` means number was produced and there's maybe characters for one more token
bool _jtok_try_update_number(json_tokenizer *t, byte last_char) {
  if ((last_char >= '0' && last_char <= '9') || last_char == 'e' || last_char == 'E' || last_char == '+' || last_char == '-' || last_char == '.') {
    return false;
  }
  if (!_jtok_try_produce_number(t, 1)) {
    return false;
  }
  // put the last character in the right place
  // assuming it will be consumed by the caller later
  t->chars[0] = last_char;
  t->chars_length++;
  return true;
}

/*
JSON_TOKEN_OBJECT_START = 1, // {
  JSON_TOKEN_OBJECT_END,       // }
  JSON_TOKEN_ARRAY_START,      // [
  JSON_TOKEN_ARRAY_END,        // ]
  JSON_TOKEN_TRUE,             // true
  JSON_TOKEN_FALSE,            // false
  JSON_TOKEN_NULL,             // null
  JSON_TOKEN_COMMA,            // element separator
  JSON_TOKEN_QUOTES,           // string start or end
  JSON_TOKEN_COLON
  JSON_TOKEN_BOM,              // byte-order mark that may appear at the start of the JSON

  // character tokens
  JSON_TOKEN_WHITESPACE,        // ' ', '\n', '\r', '\t' between elements and commas
  JSON_TOKEN_ESCAPED_CHARACTER, // \n, \r, \\ and other single-character escape sequences

  // integer tokens
  JSON_TOKEN_CHARACTER, // normal, non-escaped element of a string. one unicode codepoint.
  JSON_TOKEN_CHARCODE,  // \u1234
  JSON_TOKEN_INTEGER,   // 12345, -12345, +12345. only 64-bit safe integers

  // string tokens
  JSON_TOKEN_NUMBER, // 12.345, 1e10. every number that is not 64-bit safe integer
*/

// TODO: think about using nodiscard modifier here and in other failable places?
/** Add a byte to the tokenizer. This may cause some amount of tokens to appear for consumption. */
bool json_tokenizer_push(json_tokenizer *tokenizer, byte b) {
  if (tokenizer->chars_length == JTOK_MAX_CHARS_LENGTH - 1) {
    // broken json, or maybe overly long number
    return false;
  }

  tokenizer->chars[tokenizer->chars_length] = b;
  tokenizer->chars_length++;

  // FIXME: actually tokenize stuff
  return true;
}