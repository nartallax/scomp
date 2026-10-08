#pragma once
#include "../context.c"
#include "../data_structures/byte_fixed_stack.c"
#include "../data_structures/fixed_queue.c"
#include "../utf8.c"
#include "../writer.c"
#include "json_base.c"
#include "json_detokenizer.c"
#include <inttypes.h>

typedef struct {
  byte_fixed_stack state_stack;
  fixed_queue token_queue;
  json_number_token partial_number_token;
  json_unicode_token partial_unicode_token;
  // contains amount of bytes consumed by the token of fixed length
  byte value_progress;
} json_tokenizer;

void json_tokenizer_deinit(json_tokenizer *t, context *context) {
  bfstack_deinit(&t->state_stack, context);
  fqueue_deinit(&t->token_queue, context);
}

/** Should be called on already-initialized tokenizer to reset its state to initial.
Exists to avoid freeing and reallocating memory for a tokenizer you want to reuse */
void json_tokenizer_reset(json_tokenizer *t) {
  bfstack_reset(&t->state_stack);
  fqueue_reset(&t->token_queue);

  bfstack_push(&t->state_stack, JSON_STATE_ROOT);
  bfstack_push(&t->state_stack, JSON_STATE_START);
  t->partial_number_token = (json_number_token){0};
  t->partial_unicode_token = (json_unicode_token){0};
  t->value_progress = 0;
}

NODISCARD bool json_tokenizer_init(json_tokenizer *t, context *context) {
  *t = (json_tokenizer){0};

  if (!bfstack_init(&t->state_stack, context, JSON_MAX_STATE_STACK_LENGTH)) {
    json_tokenizer_deinit(t, context);
    return false;
  }

  if (!fqueue_init(&t->token_queue, context, sizeof(json_token), 2)) {
    json_tokenizer_deinit(t, context);
    return false;
  }

  json_tokenizer_reset(t);

  return true;
}

constexpr uint64_t TENTH_OF_MAX_UINT64 = UINT64_MAX / 10;
bool _jtok_uint64_will_overflow(uint64_t value, byte addition) {
  return value >= TENTH_OF_MAX_UINT64 - addition;
}

constexpr uint16_t TENTH_OFF_MAX_UINT16 = UINT16_MAX / 10;
bool _jtok_uint16_will_overflow(uint16_t value, byte addition) {
  return value >= TENTH_OFF_MAX_UINT16 - addition;
}

NODISCARD bool _jtok_push_state(json_tokenizer *t, json_state state) {
  if (bfstack_get_count(&t->state_stack) == bfstack_get_capacity(&t->state_stack)) {
    return false;
  }
  bfstack_push(&t->state_stack, state);
  return true;
}

void _jtok_push_simple_token(json_tokenizer *t, json_token_kind kind) {
  json_token *slot = fqueue_push(&t->token_queue);
  slot->kind = kind;
  t->value_progress = 0;
}

void _jtok_push_character_token(json_tokenizer *t, json_token_kind kind, byte character) {
  json_token *slot = fqueue_push(&t->token_queue);
  slot->kind = kind;
  slot->character.character = character;
  t->value_progress = 0;
}

void _jtok_push_unicode_token(json_tokenizer *t, json_token_kind kind) {
  json_token *slot = fqueue_push(&t->token_queue);
  slot->kind = kind;
  slot->unicode_character = t->partial_unicode_token;
  t->value_progress = 0;
}

void _jtok_push_number_token(json_tokenizer *t) {
  json_token *slot = fqueue_push(&t->token_queue);
  slot->kind = JSON_TOKEN_NUMBER;
  slot->number = t->partial_number_token;
  t->partial_number_token = (json_number_token){0};
  t->partial_unicode_token = (json_unicode_token){0};
  t->value_progress = 0;
}

NODISCARD bool _jtok_try_whitespace(json_tokenizer *t, byte b) {
  if (b == ' ' || b == '\n' || b == '\r' || b == '\t') {
    _jtok_push_character_token(t, JSON_TOKEN_WHITESPACE, b);
    return true;
  }
  return false;
}

NODISCARD bool _jtok_start_value(json_tokenizer *t, json_state state) {
  if (bfstack_peek(&t->state_stack) == JSON_STATE_VALUE) {
    bfstack_pop(&t->state_stack);
  }
  return _jtok_push_state(t, state);
}

// TODO: test what will happen if stack push fails on every value
// TODO: test what will happen if broken json is detected on every value
NODISCARD bool _jtok_try_value(json_tokenizer *t, byte b) {
  switch (b) {
  case '"':
    return _jtok_start_value(t, JSON_STATE_STRING);
  case '{':
    return _jtok_start_value(t, JSON_STATE_OBJECT_START);
  case '[':
    return _jtok_start_value(t, JSON_STATE_ARRAY_START);
  case _JTOK_NULL_BYTES[0]:
    t->value_progress = 1;
    return _jtok_start_value(t, JSON_STATE_NULL);
  case _JTOK_TRUE_BYTES[0]:
    t->value_progress = 1;
    return _jtok_start_value(t, JSON_STATE_TRUE);
  case _JTOK_FALSE_BYTES[0]:
    t->value_progress = 1;
    return _jtok_start_value(t, JSON_STATE_FALSE);
  case '-':
    t->partial_number_token.flags |= JSON_NUMBER_IS_NEGATIVE;
    return _jtok_start_value(t, JSON_STATE_NUMBER);
  default:
    if (b >= '0' && b <= '9') {
      t->value_progress = 1;
      t->partial_number_token.integer_part = b - '0';
      if (b == '0') {
        t->partial_number_token.flags |= JSON_NUMBER_IS_ZERO;
      }
      return _jtok_start_value(t, JSON_STATE_NUMBER);
    }
    return false;
  }
}

void _jtok_pop_state_after_reading_value(json_tokenizer *t) {
  bfstack_pop(&t->state_stack);
  json_state base_state = bfstack_peek(&t->state_stack);
  if (base_state == JSON_STATE_ARRAY_START) {
    bfstack_replace(&t->state_stack, JSON_STATE_ARRAY_CONTINUE);
  } else if (base_state == JSON_STATE_OBJECT_START) {
    bfstack_replace(&t->state_stack, JSON_STATE_OBJECT_CONTINUE);
  }
}

NODISCARD bool _jtok_advance_parsing_of_fixed_token(json_tokenizer *t, byte b, const byte *expected_bytes, size_t expected_bytes_length, json_token_kind kind) {
  if (b != expected_bytes[t->value_progress]) {
    return false;
  }
  t->value_progress++;
  if (t->value_progress >= expected_bytes_length) {
    _jtok_push_simple_token(t, kind);
    _jtok_pop_state_after_reading_value(t);
  }
  return true;
}

constexpr uint64_t _JTOK_NOT_A_HEX_CHARACTER = 0xff;
uint64_t _jtok_is_hex(byte hex_char) {
  return (hex_char >= '0' && hex_char <= '9') || (hex_char >= 'a' && hex_char <= 'f') || (hex_char >= 'A' && hex_char <= 'F');
}

/** Returns a token from token queue, or null if the queue is empty. */
json_token *json_tokenizer_consume(json_tokenizer *t) {
  if (fqueue_get_count(&t->token_queue) == 0) {
    return NULL;
  }
  return fqueue_pop(&t->token_queue);
}

/** Render a token that is being built by the tokenizer into the buffer, with the same bytes the token was built with.
State of the tokenizer is not changed by this.

Returns amount of meaningful bytes written.
It's possible that more bytes were written, but caller should only use meaningful bytes. */
size_t json_tokenizer_render_partial_token(json_tokenizer *t, writer *w) {
  json_state state = bfstack_peek(&t->state_stack);
  switch (state) {
  case JSON_STATE_BOM:
    json_detokenizer_write(w, &(json_token){.kind = JSON_TOKEN_BOM});
    return t->value_progress;
  case JSON_STATE_TRUE:
    json_detokenizer_write(w, &(json_token){.kind = JSON_TOKEN_TRUE});
    return t->value_progress;
  case JSON_STATE_FALSE:
    json_detokenizer_write(w, &(json_token){.kind = JSON_TOKEN_FALSE});
    return t->value_progress;
  case JSON_STATE_NULL:
    json_detokenizer_write(w, &(json_token){.kind = JSON_TOKEN_NULL});
    return t->value_progress;
  case JSON_STATE_ESCAPED_CHARACTER: {
    json_token token;
    token.kind = JSON_TOKEN_ESCAPED_CHARACTER;
    token.character.character = 'n';
    json_detokenizer_write(w, &token);
  }
    return 1;
  case JSON_STATE_ESCAPED_CHARCODE: {
    json_token token;
    token.kind = JSON_TOKEN_ESCAPED_CHARCODE;
    token.unicode_character = t->partial_unicode_token;
    json_detokenizer_write(w, &token);
  }
    return t->value_progress + 2;
  case JSON_STATE_UNICODE_CHARACTER: {
    json_token token;
    token.kind = JSON_TOKEN_CHARACTER;
    token.unicode_character = t->partial_unicode_token;
    json_detokenizer_write(w, &token);
  }
    return t->value_progress;
  case JSON_STATE_NUMBER: {
    json_token token;
    token.kind = JSON_TOKEN_NUMBER;
    token.number = t->partial_number_token;
    return json_detokenizer_write(w, &token);
  }
  case JSON_STATE_START:
  case JSON_STATE_ROOT:
  case JSON_STATE_OBJECT_KV_SEPARATOR:
  case JSON_STATE_STRING:
  case JSON_STATE_OBJECT_KEY:
  case JSON_STATE_OBJECT_KEY_START:
  case JSON_STATE_VALUE:
  case JSON_STATE_ARRAY_START:
  case JSON_STATE_ARRAY_CONTINUE:
  case JSON_STATE_OBJECT_START:
  case JSON_STATE_OBJECT_CONTINUE:
    // nothing to write, no pending token
    return 0;
  }
}

/** Add a byte to the tokenizer.
Returns true if the byte was successfully used to build a token.
Each successfully used byte may produce from 0 to 2 tokens.
You should consume all of them before pushing more bytes.

Returns false if the byte yielded invalid state (as in, it was detected that input is not a valid JSON).
You should still consume existing tokens, if any;
then you should consume token-being-built using json_tokenizer_consume();
then you should still use that byte you passed to this function, as it was not used for anything.
After that you should deinit/reset the tokenizer. */
NODISCARD bool json_tokenizer_push(json_tokenizer *t, byte b) {
  json_state state = bfstack_peek(&t->state_stack);
  switch (state) {
  case JSON_STATE_ROOT:
    return _jtok_try_whitespace(t, b);
  case JSON_STATE_START:
    bfstack_pop(&t->state_stack);
    if (b == UTF8_BOM[0]) {
      t->value_progress = 1;
      return _jtok_push_state(t, JSON_STATE_BOM);
    } else {
      return _jtok_push_state(t, JSON_STATE_VALUE) && json_tokenizer_push(t, b);
    }
  case JSON_STATE_VALUE:
    return _jtok_try_value(t, b) || _jtok_try_whitespace(t, b);
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
      _jtok_push_simple_token(t, JSON_TOKEN_ARRAY_CLOSE);
      _jtok_pop_state_after_reading_value(t);
      return true;
    }
    return _jtok_try_whitespace(t, b) || _jtok_try_value(t, b);

  case JSON_STATE_ARRAY_CONTINUE:
    if (b == ']') {
      _jtok_push_simple_token(t, JSON_TOKEN_ARRAY_CLOSE);
      _jtok_pop_state_after_reading_value(t);
      return true;
    } else if (b == ',') {
      _jtok_push_simple_token(t, JSON_TOKEN_COMMA);
      return _jtok_push_state(t, JSON_STATE_VALUE);
    } else {
      return _jtok_try_whitespace(t, b);
    }
  case JSON_STATE_OBJECT_START:
    if (b == '}') {
      _jtok_push_simple_token(t, JSON_TOKEN_OBJECT_CLOSE);
      _jtok_pop_state_after_reading_value(t);
      return true;
    } else if (b == '"') {
      _jtok_push_simple_token(t, JSON_TOKEN_QUOTES);
      return _jtok_push_state(t, JSON_STATE_OBJECT_KEY);
    } else {
      return _jtok_try_whitespace(t, b);
    }
  case JSON_STATE_OBJECT_CONTINUE:
    if (b == '}') {
      _jtok_push_simple_token(t, JSON_TOKEN_OBJECT_CLOSE);
      _jtok_pop_state_after_reading_value(t);
      return true;
    } else if (b == ',') {
      _jtok_push_simple_token(t, JSON_TOKEN_COMMA);
      return _jtok_push_state(t, JSON_STATE_OBJECT_KEY_START);
    } else {
      return _jtok_try_whitespace(t, b);
    }
  case JSON_STATE_OBJECT_KEY_START:
    if (b == '"') {
      _jtok_push_simple_token(t, JSON_TOKEN_QUOTES);
      bfstack_replace(&t->state_stack, JSON_STATE_OBJECT_KEY);
      return true;
    } else {
      return _jtok_try_whitespace(t, b);
    }
    // TODO: test for an array with a lot of values, to see if I missed state management somewhere
    // use all different kinds of values
  case JSON_STATE_OBJECT_KV_SEPARATOR:
    if (b == ':') {
      _jtok_push_simple_token(t, JSON_TOKEN_COLON);
      bfstack_replace(&t->state_stack, JSON_STATE_VALUE);
      return true;
    } else {
      return _jtok_try_whitespace(t, b);
    }
  case JSON_STATE_STRING:
  case JSON_STATE_OBJECT_KEY:
    if (b == '"') {
      _jtok_push_simple_token(t, JSON_TOKEN_QUOTES);
      if (state == JSON_STATE_OBJECT_KEY) {
        bfstack_replace(&t->state_stack, JSON_STATE_OBJECT_KV_SEPARATOR);
      } else {
        _jtok_pop_state_after_reading_value(t);
      }
      return true;
    } else if (b == '\\') {
      _jtok_push_simple_token(t, JSON_TOKEN_ESCAPED_CHARACTER);
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
        _jtok_push_unicode_token(t, JSON_TOKEN_CHARACTER);
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
      _jtok_push_unicode_token(t, JSON_TOKEN_CHARACTER);
      bfstack_pop(&t->state_stack);
    }
    return true;
  case JSON_STATE_ESCAPED_CHARACTER:
    if (b == '\\' || b == '"' || b == '/' || b == 'b' || b == 'f' || b == 'n' || b == 'r' || b == 't') {
      _jtok_push_character_token(t, JSON_TOKEN_ESCAPED_CHARACTER, b);
      bfstack_pop(&t->state_stack);
      return true;
    } else if (b == 'u') {
      t->partial_unicode_token.length = 4;
      return _jtok_push_state(t, JSON_STATE_ESCAPED_CHARCODE);
    } else {
      return false;
    }
  case JSON_STATE_ESCAPED_CHARCODE:
    if (!_jtok_is_hex(b)) {
      return false;
    }
    // 4 hex bytes are stored like that to preserve case
    // as we must not lose any data at all during tokenization
    t->partial_unicode_token.value |= ((uint64_t)b) << (8 * t->value_progress);
    t->value_progress++;
    if (t->value_progress == 4) {
      bfstack_pop(&t->state_stack);
      _jtok_push_unicode_token(t, JSON_TOKEN_CHARACTER);
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
        if (_jtok_uint16_will_overflow(t->partial_number_token.exponent_part, digit)) {
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
        if (_jtok_uint64_will_overflow(t->partial_number_token.fraction_part, digit)) {
          return false;
        }
        t->partial_number_token.fraction_part = (t->partial_number_token.fraction_part * 10) + digit;
        t->value_progress++;
        return true;
      } else {
        if (t->partial_number_token.flags & JSON_NUMBER_IS_ZERO) {
          // TODO: do we have those in tests? and others mentioned her
          // `01` is invalid, `-01` is invalid
          return false;
        }
        if (_jtok_uint64_will_overflow(t->partial_number_token.integer_part, digit)) {
          return false;
        }
        t->partial_number_token.fraction_part = (t->partial_number_token.fraction_part * 10) + digit;
        if (digit == 0 && t->value_progress == 0) {
          t->partial_number_token.flags |= JSON_NUMBER_IS_ZERO;
        }
        t->value_progress++;
        return true;
      }
    } else if (b == '.' && !(t->partial_number_token.flags & (JSON_NUMBER_HAS_EXPONENT | JSON_NUMBER_HAS_FRACTION))) {
      // `1.1.1` is invalid; `1..1` is invalid; `1e1.1` is invalid
      if (t->value_progress == 0) {
        // `-.1` is invalid
        return false;
      }
      t->partial_number_token.flags |= JSON_NUMBER_HAS_FRACTION;
      t->value_progress = 0;
      return true;
    } else if ((b == 'e' || b == 'E') && !(t->partial_number_token.flags & JSON_NUMBER_HAS_EXPONENT)) {
      // `1e1e1` is invalid, `1ee1` is invalid
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
      _jtok_push_number_token(t);
      _jtok_pop_state_after_reading_value(t);
      return json_tokenizer_push(t, b);
    }
  }
}

/** Returns true if the tokenizer have read one whole JSON completely.
In this state, tokenizer will only read whitespace tokens
(because we allow to have infinite number of whitespace tokens after the end of the actual value).
You can reset the tokenizer if you want to reuse it. */
bool json_tokenizer_is_done(json_tokenizer *t) {
  json_state state = bfstack_peek(&t->state_stack);
  return state == JSON_STATE_ROOT;
}