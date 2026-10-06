#pragma once
#include "../context.c"
#include "../data_structures/byte_fixed_stack.c"
#include "../data_structures/fixed_queue.c"
#include "../utf8.c"
#include <inttypes.h>

// TODO: test if nesting below the limit actually works
/** Max length of state stack. Limits nesting.
Exists to prevent memory overflow in case when the input is infinite number of '[' */
constexpr size_t JSON_MAX_STATE_STACK_LENGTH__new = 512;

/** Max amount of bytes that may be assembled into one token.

It's that many mostly for various numbers;
non-number tokens will be 6 bytes at most (\u1234 being the second longest) */
constexpr size_t JSON_MAX_SINGLE_TOKEN_LENGTH__new = 128;

/** Max amount of leading zeroes a JSON number can have in its fractional part, or in its exponent.
It's possible to have in theory many more leading zeroes, especially in fractions, but it's just about never happens in real life. */
constexpr size_t JSON_MAX_LEADING_ZEROES__new = 32;

constexpr byte _JTOK_TRUE_BYTES[] = {'t', 'r', 'u', 'e'};
constexpr byte _JTOK_FALSE_BYTES[] = {'f', 'a', 'l', 's', 'e'};
constexpr byte _JTOK_NULL_BYTES[] = {'n', 'u', 'l', 'l'};

// TODO: consider making this single-byte long and check the performance
// this may require moving around parts of some structures, to jiggle alignment rules a bit
typedef enum {
  // simple tokens
  JSON_TOKEN_OBJECT_OPEN = 1, // {
  JSON_TOKEN_OBJECT_CLOSE,    // }
  JSON_TOKEN_ARRAY_OPEN,      // [
  JSON_TOKEN_ARRAY_CLOSE,     // ]
  JSON_TOKEN_TRUE,            // true
  JSON_TOKEN_FALSE,           // false
  JSON_TOKEN_NULL,            // null
  JSON_TOKEN_COMMA,           // element separator // TODO: rename to value_separator
  JSON_TOKEN_QUOTES,          // string start or end // TODO: rename to string_delimiter
  JSON_TOKEN_COLON,           // : separator between key and value // TODO: rename to key_value_separator
  JSON_TOKEN_BOM,             // byte-order mark that may appear at the start of the JSON

  // character tokens
  JSON_TOKEN_WHITESPACE,        // ' ', '\n', '\r', '\t' between elements and commas
  JSON_TOKEN_ESCAPED_CHARACTER, // \n, \r, \\ and other single-character escape sequences within a string

  // unicode tokens
  JSON_TOKEN_CHARACTER,        // normal, non-escaped element of a string. one unicode codepoint; `value` contains sequence of utf-8 bytes merged into one uint64, not decoded codepoint
  JSON_TOKEN_ESCAPED_CHARCODE, // \u1234

  // number tokens
  JSON_TOKEN_NUMBER, // 12.345, 1e10. every number that is not 64-bit safe integer
} json_token_kind__new;

typedef enum {
  JSON_STATE_ROOT = 0,            // if this state is ever reached - it means JSON reading is completed
  JSON_STATE_START,               // expecting BOM or value
  JSON_STATE_VALUE,               // expecting some sort of value
  JSON_STATE_OBJECT_START,        // expecting key or closing token
  JSON_STATE_OBJECT_CONTINUE,     // expecting comma or closing token
  JSON_STATE_OBJECT_KV_SEPARATOR, // expecting ":"
  JSON_STATE_OBJECT_KEY_START,    // expecting key
  JSON_STATE_ARRAY_START,         // expecting value or closing token
  JSON_STATE_ARRAY_CONTINUE,      // expecting comma or closing token
  JSON_STATE_STRING,              // expecting characters
  JSON_STATE_OBJECT_KEY,          // exactly like string, but hints about further state changes
  JSON_STATE_ESCAPED_CHARACTER,   // expecting n, r, or other escapeable character
  JSON_STATE_ESCAPED_CHARCODE,    // expecting four hexadecimals
  JSON_STATE_UNICODE_CHARACTER,   // expecting unicode continuation
  JSON_STATE_NUMBER,              // expecting numeric components
  JSON_STATE_TRUE,                // expecting letters of `true`
  JSON_STATE_FALSE,               // expecting letters of `false`
  JSON_STATE_NULL,                // expecting letters of `null`
  JSON_STATE_BOM                  // expecting bytes of utf-8 byte order mark
} json_state__new;

/** Tokens that store exactly 1 character of information with them */
typedef struct {
  byte character;
} json_character_token__new;

/** Tokens that are describing a unicode character, or a part of it, depeding on kind */
typedef struct {
  uint64_t value;
  byte length;
} json_unicode_token__new;

// TODO: make it be byte-long
/** Various booleans about JSON numbers packed into a bitmap */
typedef enum {
  JSON_NUMBER_HAS_FRACTION = 1 << 0,         // 1.1
  JSON_NUMBER_HAS_EXPONENT = 1 << 1,         // 1e1
  JSON_NUMBER_EXPONENT_UPPERCASE = 1 << 2,   // 1E1
  JSON_NUMBER_EXPONENT_IS_NEGATIVE = 1 << 3, // 1e-1
  JSON_NUMBER_EXPONENT_HAS_PLUS = 1 << 4,    // 1e+1
  JSON_NUMBER_IS_NEGATIVE = 1 << 5,          // -1
} json_number_flags__new;

/** Tokens that contains a number in JSON sense. */
typedef struct {
  uint64_t integer_part;
  uint64_t fraction_part;
  uint16_t exponent_part; // exponents over 65535 are way too large for anything practical anyway
  json_number_flags__new flags;
  byte fraction_leading_zeroes;
  byte exponent_leading_zeroes;
} json_number_token__new;

/** Any of the tokens described above. */
typedef struct {
  json_token_kind__new kind;

  union {
    json_character_token__new character;
    json_unicode_token__new unicode_character;
    json_number_token__new number;
  };
} json_token__new;

typedef struct {
  byte_fixed_stack state_stack;
  fixed_queue token_queue;
  json_number_token__new partial_number_token;
  json_unicode_token__new partial_unicode_token;
  // contains amount of bytes consumed by the token of fixed length
  byte value_progress;
} json_tokenizer__new;

void json_tokenizer_deinit__new(json_tokenizer__new *t, context *context) {
  bfstack_deinit(&t->state_stack, context);
  fqueue_deinit(&t->token_queue, context);
}

/** Should be called on already-initialized tokenizer to reset its state to initial.
Exists to avoid freeing and reallocating memory for a tokenizer you want to reuse */
void json_tokenizer_reset__new(json_tokenizer__new *t) {
  bfstack_reset(&t->state_stack);
  fqueue_reset(&t->token_queue);

  bfstack_push(&t->state_stack, JSON_STATE_ROOT);
  bfstack_push(&t->state_stack, JSON_STATE_START);
  t->partial_number_token = (json_number_token__new){0};
  t->partial_unicode_token = (json_unicode_token__new){0};
  t->value_progress = 0;
}

NODISCARD bool json_tokenizer_init__new(json_tokenizer__new *t, context *context) {
  *t = (json_tokenizer__new){0};

  if (!bfstack_init(&t->state_stack, context, JSON_MAX_STATE_STACK_LENGTH__new)) {
    json_tokenizer_deinit__new(t, context);
    return false;
  }

  if (!fqueue_init(&t->token_queue, context, sizeof(json_token__new), 2)) {
    json_tokenizer_deinit__new(t, context);
    return false;
  }

  json_tokenizer_reset__new(t);

  return true;
}

constexpr uint64_t TENTH_OF_MAX_UINT64__new = UINT64_MAX / 10;
bool _jtok_uint64_will_overflow__new(uint64_t value, byte addition) {
  return value >= TENTH_OF_MAX_UINT64__new - addition;
}

constexpr uint16_t TENTH_OFF_MAX_UINT16__new = UINT16_MAX / 10;
bool _jtok_uint16_will_overflow__new(uint16_t value, byte addition) {
  return value >= TENTH_OFF_MAX_UINT16__new - addition;
}

NODISCARD bool _jtok_push_state(json_tokenizer__new *t, json_state__new state) {
  if (bfstack_get_count(&t->state_stack) == bfstack_get_capacity(&t->state_stack)) {
    return false;
  }
  bfstack_push(&t->state_stack, state);
  return true;
}

void _jtok_push_simple_token__new(json_tokenizer__new *t, json_token_kind__new kind) {
  json_token__new *slot = fqueue_push(&t->token_queue);
  slot->kind = kind;
  t->value_progress = 0;
}

void _jtok_push_character_token__new(json_tokenizer__new *t, json_token_kind__new kind, byte character) {
  json_token__new *slot = fqueue_push(&t->token_queue);
  slot->kind = kind;
  slot->character.character = character;
  t->value_progress = 0;
}

void _jtok_push_unicode_token__new(json_tokenizer__new *t, json_token_kind__new kind) {
  json_token__new *slot = fqueue_push(&t->token_queue);
  slot->kind = kind;
  slot->unicode_character = t->partial_unicode_token;
  t->value_progress = 0;
}

void _jtok_push_number_token__new(json_tokenizer__new *t) {
  json_token__new *slot = fqueue_push(&t->token_queue);
  slot->kind = JSON_TOKEN_NUMBER;
  slot->number = t->partial_number_token;
  t->partial_number_token = (json_number_token__new){0};
  t->partial_unicode_token = (json_unicode_token__new){0};
  t->value_progress = 0;
}

NODISCARD bool _jtok_try_whitespace__new(json_tokenizer__new *t, byte b) {
  if (b == ' ' || b == '\n' || b == '\r' || b == '\t') {
    _jtok_push_character_token__new(t, JSON_TOKEN_WHITESPACE, b);
    return true;
  }
  return false;
}

NODISCARD bool _jtok_start_value__new(json_tokenizer__new *t, json_state__new state) {
  if (bfstack_peek(&t->state_stack) == JSON_STATE_VALUE) {
    bfstack_pop(&t->state_stack);
  }
  return _jtok_push_state(t, state);
}

// TODO: test what will happen if stack push fails on every value
// TODO: test what will happen if broken json is detected on every value
NODISCARD bool _jtok_try_value__new(json_tokenizer__new *t, byte b) {
  switch (b) {
  case '"':
    return _jtok_start_value__new(t, JSON_STATE_STRING);
  case '{':
    return _jtok_start_value__new(t, JSON_STATE_OBJECT_START);
  case '[':
    return _jtok_start_value__new(t, JSON_STATE_ARRAY_START);
  case _JTOK_NULL_BYTES[0]:
    t->value_progress = 1;
    return _jtok_start_value__new(t, JSON_STATE_NULL);
  case _JTOK_TRUE_BYTES[0]:
    t->value_progress = 1;
    return _jtok_start_value__new(t, JSON_STATE_TRUE);
  case _JTOK_FALSE_BYTES[0]:
    t->value_progress = 1;
    return _jtok_start_value__new(t, JSON_STATE_FALSE);
  case '-':
    t->partial_number_token.flags |= JSON_NUMBER_IS_NEGATIVE;
    return _jtok_start_value__new(t, JSON_STATE_NUMBER);
  default:
    // TODO: think about not having leading zeroes in integers. `01` is invalid
    if (b >= '0' && b <= '9') {
      t->value_progress = 1;
      t->partial_number_token.integer_part = b - '0';
      return _jtok_start_value__new(t, JSON_STATE_NUMBER);
    }
    return false;
  }
}

void _jtok_pop_state_after_reading_value(json_tokenizer__new *t) {
  bfstack_pop(&t->state_stack);
  json_state__new base_state = bfstack_peek(&t->state_stack);
  if (base_state == JSON_STATE_ARRAY_START) {
    bfstack_replace(&t->state_stack, JSON_STATE_ARRAY_CONTINUE);
  } else if (base_state == JSON_STATE_OBJECT_START) {
    bfstack_replace(&t->state_stack, JSON_STATE_OBJECT_CONTINUE);
  }
}

NODISCARD bool _jtok_advance_parsing_of_fixed_token(json_tokenizer__new *t, byte b, const byte *expected_bytes, size_t expected_bytes_length, json_token_kind__new kind) {
  if (b != expected_bytes[t->value_progress]) {
    return false;
  }
  t->value_progress++;
  if (t->value_progress >= expected_bytes_length) {
    _jtok_push_simple_token__new(t, kind);
    _jtok_pop_state_after_reading_value(t);
  }
  return true;
}

constexpr uint64_t _JTOK_NOT_A_HEX_CHARACTER__new = 0xff;
uint64_t _jtok_is_hex__new(byte hex_char) {
  return (hex_char >= '0' && hex_char <= '9') || (hex_char >= 'a' && hex_char <= 'f') || (hex_char >= 'A' && hex_char <= 'F');
}

/** Returns a token from token queue, or null if the queue is empty. */
json_token__new *json_tokenizer_consume__new(json_tokenizer__new *t) {
  if (fqueue_get_count(&t->token_queue) == 0) {
    return NULL;
  }
  return fqueue_pop(&t->token_queue);
}

/** Render a token that is being built by the tokenizer into the buffer. Zero-terminates the sequence. Returns amount of bytes written, not including zero.
Buffer must be at least JSON_MAX_SINGLE_TOKEN_LENGTH__new bytes long.
Buffer will contain the same bytes that were used to build the token. State of the tokenizer is not changed by this. */
size_t json_tokenizer_render_partial_token__new(json_tokenizer__new *t, byte *buffer) {
  // TODO: impl
  return 0;
}

/** Add a byte to the tokenizer.
Returns true if the byte was successfully used to build a token.
Each successfully used byte may produce from 0 to 2 tokens.
You should consume all of them before pushing more bytes.

Returns false if the byte yielded invalid state (as in, it was detected that input is not a valid JSON).
You should still consume existing tokens, if any;
then you should consume token-being-built using json_tokenizer_consume__new();
then you should still use that byte you passed to this function, as it was not used for anything.
After that you should deinit/reset the tokenizer. */
NODISCARD bool json_tokenizer_push__new(json_tokenizer__new *t, byte b) {
  json_state__new state = bfstack_peek(&t->state_stack);
  switch (state) {
  case JSON_STATE_ROOT:
    return _jtok_try_whitespace__new(t, b);
  case JSON_STATE_START:
    bfstack_pop(&t->state_stack);
    if (b == UTF8_BOM[0]) {
      t->value_progress = 1;
      return _jtok_push_state(t, JSON_STATE_BOM);
    } else {
      return _jtok_push_state(t, JSON_STATE_VALUE) && json_tokenizer_push__new(t, b);
    }
  case JSON_STATE_VALUE:
    return _jtok_try_value__new(t, b) || _jtok_try_whitespace__new(t, b);
  case JSON_STATE_BOM:
    if (!_jtok_advance_parsing_of_fixed_token(t, b, UTF8_BOM, sizeof(UTF8_BOM), JSON_TOKEN_BOM)) {
      return false;
    }
    // check if BOM is parsed - this will reset the state
    if (t->value_progress == 0) {
      return _jtok_push_state(t, JSON_STATE_VALUE);
    }
    return true;
  case JSON_STATE_TRUE:
    return _jtok_advance_parsing_of_fixed_token(t, b, _JTOK_TRUE_BYTES, sizeof(_JTOK_TRUE_BYTES), JSON_TOKEN_TRUE);
  case JSON_STATE_FALSE:
    return _jtok_advance_parsing_of_fixed_token(t, b, _JTOK_FALSE_BYTES, sizeof(_JTOK_FALSE_BYTES), JSON_TOKEN_FALSE);
  case JSON_STATE_NULL:
    return _jtok_advance_parsing_of_fixed_token(t, b, _JTOK_NULL_BYTES, sizeof(_JTOK_NULL_BYTES), JSON_TOKEN_NULL);
  case JSON_STATE_ARRAY_START:
    if (b == ']') {
      _jtok_push_simple_token__new(t, JSON_TOKEN_ARRAY_CLOSE);
      _jtok_pop_state_after_reading_value(t);
      return true;
    }
    return _jtok_try_whitespace__new(t, b) || _jtok_try_value__new(t, b);

  case JSON_STATE_ARRAY_CONTINUE:
    if (b == ']') {
      _jtok_push_simple_token__new(t, JSON_TOKEN_ARRAY_CLOSE);
      _jtok_pop_state_after_reading_value(t);
      return true;
    } else if (b == ',') {
      _jtok_push_simple_token__new(t, JSON_TOKEN_COMMA);
      return _jtok_push_state(t, JSON_STATE_VALUE);
    } else {
      return _jtok_try_whitespace__new(t, b);
    }
  case JSON_STATE_OBJECT_START:
    if (b == '}') {
      _jtok_push_simple_token__new(t, JSON_TOKEN_OBJECT_CLOSE);
      _jtok_pop_state_after_reading_value(t);
      return true;
    } else if (b == '"') {
      _jtok_push_simple_token__new(t, JSON_TOKEN_QUOTES);
      return _jtok_push_state(t, JSON_STATE_OBJECT_KEY);
    } else {
      return _jtok_try_whitespace__new(t, b);
    }
  case JSON_STATE_OBJECT_CONTINUE:
    if (b == '}') {
      _jtok_push_simple_token__new(t, JSON_TOKEN_OBJECT_CLOSE);
      _jtok_pop_state_after_reading_value(t);
      return true;
    } else if (b == ',') {
      _jtok_push_simple_token__new(t, JSON_TOKEN_COMMA);
      return _jtok_push_state(t, JSON_STATE_OBJECT_KEY_START);
    } else {
      return _jtok_try_whitespace__new(t, b);
    }
  case JSON_STATE_OBJECT_KEY_START:
    if (b == '"') {
      _jtok_push_simple_token__new(t, JSON_TOKEN_QUOTES);
      bfstack_replace(&t->state_stack, JSON_STATE_OBJECT_KEY);
      return true;
    } else {
      return _jtok_try_whitespace__new(t, b);
    }
    // TODO: test for an array with a lot of values, to see if I missed state management somewhere
    // use all different kinds of values
  case JSON_STATE_OBJECT_KV_SEPARATOR:
    if (b == ':') {
      _jtok_push_simple_token__new(t, JSON_TOKEN_COLON);
      bfstack_replace(&t->state_stack, JSON_STATE_VALUE);
      return true;
    } else {
      return _jtok_try_whitespace__new(t, b);
    }
  case JSON_STATE_STRING:
  case JSON_STATE_OBJECT_KEY:
    if (b == '"') {
      _jtok_push_simple_token__new(t, JSON_TOKEN_QUOTES);
      if (state == JSON_STATE_OBJECT_KEY) {
        bfstack_replace(&t->state_stack, JSON_STATE_OBJECT_KV_SEPARATOR);
      } else {
        _jtok_pop_state_after_reading_value(t);
      }
      return true;
    } else if (b == '\\') {
      _jtok_push_simple_token__new(t, JSON_TOKEN_ESCAPED_CHARACTER);
      return true;
    } else {
      size_t utf8_length = utf8_get_sequence_length_by_first_byte(b);
      if (utf8_length == 0) {
        return false;
      }
      t->partial_unicode_token.length = utf8_length;
      t->partial_unicode_token.value = b;
      if (utf8_length == 1) {
        if (b <= 0x1f) {
          // unescaped control characters are not valid in JSON
          return false;
        }
        _jtok_push_unicode_token__new(t, JSON_TOKEN_CHARACTER);
        return true;
      }
      t->value_progress = 1;
      return _jtok_push_state(t, JSON_STATE_UNICODE_CHARACTER);
    }
  case JSON_STATE_UNICODE_CHARACTER:
    if (!utf8_is_valid_continuation_byte(b)) {
      return false;
    }
    t->partial_unicode_token.value |= ((uint64_t)b) << (8 * t->value_progress);
    t->value_progress++;
    if (t->value_progress == t->partial_unicode_token.length) {
      _jtok_push_unicode_token__new(t, JSON_TOKEN_CHARACTER);
      bfstack_pop(&t->state_stack);
    }
    return true;
  case JSON_STATE_ESCAPED_CHARACTER:
    if (b == '\\' || b == '"' || b == '/' || b == 'b' || b == 'f' || b == 'n' || b == 'r' || b == 't') {
      _jtok_push_character_token__new(t, JSON_TOKEN_ESCAPED_CHARACTER, b);
      bfstack_pop(&t->state_stack);
      return true;
    } else if (b == 'u') {
      t->partial_unicode_token.length = 4;
      return _jtok_push_state(t, JSON_STATE_ESCAPED_CHARCODE);
    } else {
      return false;
    }
  case JSON_STATE_ESCAPED_CHARCODE:
    if (!_jtok_is_hex__new(b)) {
      return false;
    }
    // 4 hex bytes are stored like that to preserve case
    // as we must not lose any data at all during tokenization
    t->partial_unicode_token.value |= ((uint64_t)b) << (8 * t->value_progress);
    t->value_progress++;
    if (t->value_progress == 4) {
      bfstack_pop(&t->state_stack);
      _jtok_push_unicode_token__new(t, JSON_TOKEN_CHARACTER);
    }
    return true;
  case JSON_STATE_NUMBER:
    if (b >= '0' && b <= '9') {
      byte digit = b - '0';
      if (t->partial_number_token.flags & JSON_NUMBER_HAS_EXPONENT) {
        if (t->value_progress == 0 && digit == 0) {
          t->partial_number_token.exponent_leading_zeroes++;
          return true;
        }
        if (_jtok_uint16_will_overflow__new(t->partial_number_token.exponent_part, digit)) {
          return false;
        }
        t->partial_number_token.exponent_part = (t->partial_number_token.exponent_part * 10) + digit;
        t->value_progress++;
        return true;
      } else if (t->partial_number_token.flags & JSON_NUMBER_HAS_FRACTION) {
        if (t->value_progress == 0 && digit == 0) {
          t->partial_number_token.fraction_leading_zeroes++;
          return true;
        }
        if (_jtok_uint64_will_overflow__new(t->partial_number_token.fraction_part, digit)) {
          return false;
        }
        t->partial_number_token.fraction_part = (t->partial_number_token.fraction_part * 10) + digit;
        t->value_progress++;
        return true;
      } else {
        if (t->value_progress > 0 && t->partial_number_token.integer_part == 0) {
          // TODO: do we have those in tests? and others mentioned her
          // `01` is invalid, `-01` is invalid
          return false;
        }
        if (_jtok_uint64_will_overflow__new(t->partial_number_token.integer_part, digit)) {
          return false;
        }
        t->partial_number_token.fraction_part = (t->partial_number_token.fraction_part * 10) + digit;
        t->value_progress++;
        return true;
      }
    } else if (b == '.' && !(t->partial_number_token.flags & (JSON_NUMBER_HAS_EXPONENT | JSON_NUMBER_HAS_FRACTION))) {
      // `1.1.1` is invalid; `1e1.1` is invalid
      if (t->value_progress == 0) {
        // `-.1` is invalid
        return false;
      }
      t->partial_number_token.flags |= JSON_NUMBER_HAS_FRACTION;
      t->value_progress = 0;
      return true;
    } else if ((b == 'e' || b == 'E') && !(t->partial_number_token.flags & JSON_NUMBER_HAS_EXPONENT)) {
      // `1e1e1` is invalid
      if (t->value_progress == 0) {
        // `1.e1` is invalid; `-e1` is invalid
        return false;
      }
      t->partial_number_token.flags |= JSON_NUMBER_HAS_EXPONENT;
      if (b == 'E') {
        t->partial_number_token.flags |= JSON_NUMBER_EXPONENT_UPPERCASE;
      }
      t->value_progress = 0;
      return true;
    } else if ((b == '-' || b == '+') && t->partial_number_token.flags & JSON_NUMBER_HAS_EXPONENT &&
               !(t->partial_number_token.flags & (JSON_NUMBER_EXPONENT_IS_NEGATIVE | JSON_NUMBER_EXPONENT_HAS_PLUS)) && t->value_progress == 0) {
      // `1e-+1` is invalid; `1e--1` is invalid; `1-1` is invalid; `1.-1` is invalid; `1e1-1` is invalid
      t->partial_number_token.flags |= b == '-' ? JSON_NUMBER_EXPONENT_IS_NEGATIVE : JSON_NUMBER_EXPONENT_HAS_PLUS;
      return true;
    } else {
      // it's something else. time to terminate the token
      if (t->value_progress == 0) {
        // `-true` is invalid; `1.true` is invalid; `1etrue` is invalid
        return false;
      }
      _jtok_push_number_token__new(t);
      _jtok_pop_state_after_reading_value(t);
      return json_tokenizer_push__new(t, b);
    }
  }
}