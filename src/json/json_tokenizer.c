#pragma once
#include "../context.c"
#include "../data_structures/fixed_queue.c"
#include "../utf8.c"
#include "../writer.c"
#include "json_base.c"
#include "json_detokenizer.c"
#include "json_state_machine.c"
#include <inttypes.h>

typedef struct {
  json_state_machine jsm;
  fixed_queue token_queue;
  json_number_token partial_number_token;
  json_unicode_token partial_unicode_token;
  // contains amount of bytes consumed by the token of fixed length
  byte value_progress;
} json_tokenizer;

void json_tokenizer_deinit(json_tokenizer *t, context *context) {
  jsm_deinit(&t->jsm, context);
  fqueue_deinit(&t->token_queue, context);

  *t = (json_tokenizer){0};
}

/** Should be called on already-initialized tokenizer to reset its state to initial.
Exists to avoid freeing and reallocating memory for a tokenizer you want to reuse */
void json_tokenizer_reset(json_tokenizer *t) {
  jsm_reset(&t->jsm);
  fqueue_reset(&t->token_queue);

  t->partial_number_token = (json_number_token){0};
  t->partial_unicode_token = (json_unicode_token){0};
  t->value_progress = 0;
}

NODISCARD bool json_tokenizer_init(json_tokenizer *t, context *context) {
  *t = (json_tokenizer){0};

  if (!jsm_init(&t->jsm, context)) {
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

constexpr uint64 TENTH_OF_MAX_UINT64 = UINT64_MAX / 10;
INLINE bool _jtok_uint64_will_overflow(uint64 value, byte addition) {
  return value >= TENTH_OF_MAX_UINT64 - addition;
}

constexpr uint16 TENTH_OFF_MAX_UINT16 = UINT16_MAX / 10;
INLINE bool _jtok_uint16_will_overflow(uint16 value, byte addition) {
  return value >= TENTH_OFF_MAX_UINT16 - addition;
}

INLINE void _jtok_push_simple_token(json_tokenizer *t, json_token_kind kind) {
  json_token *slot = fqueue_push(&t->token_queue);
  slot->kind = kind;
  t->value_progress = 0;
}

INLINE void _jtok_push_character_token(json_tokenizer *t, json_token_kind kind, byte character) {
  json_token *slot = fqueue_push(&t->token_queue);
  slot->kind = kind;
  slot->character.character = character;
  t->value_progress = 0;
}

INLINE void _jtok_push_unicode_token(json_tokenizer *t, json_token_kind kind) {
  json_token *slot = fqueue_push(&t->token_queue);
  slot->kind = kind;
  slot->unicode_character = t->partial_unicode_token;
  t->partial_unicode_token = (json_unicode_token){0};
  t->value_progress = 0;
}

INLINE void _jtok_push_number_token(json_tokenizer *t) {
  json_token *slot = fqueue_push(&t->token_queue);
  slot->kind = JSON_TOKEN_NUMBER;
  slot->number = t->partial_number_token;
  t->partial_number_token = (json_number_token){0};
  t->value_progress = 0;
}

NODISCARD INLINE bool _jtok_try_whitespace(json_tokenizer *t, byte b) {
  for (size_t i = 0; i < sizeof(_JTOK_WHITESPACE_CHARS); i++) {
    if (b == _JTOK_WHITESPACE_CHARS[i]) {
      _jtok_push_character_token(t, JSON_TOKEN_WHITESPACE, b);
      return true;
    }
  }
  return false;
}

NODISCARD INLINE bool _jtok_push_simple_token_with_state(json_tokenizer *t, json_token_kind kind) {
  if (!jsm_start_token(&t->jsm, kind)) {
    return false;
  }
  _jtok_push_simple_token(t, kind);
  return true;
}

NODISCARD INLINE bool _jtok_start_fixed_value_with_state(json_tokenizer *t, json_token_kind kind) {
  if (!jsm_start_token(&t->jsm, kind)) {
    return false;
  }
  t->value_progress = 1;
  return true;
}

// TODO: cringe. the rest of them are hardcoded
constexpr byte _JTOK_NULL_START = _JTOK_NULL_BYTES[0];
constexpr byte _JTOK_TRUE_START = _JTOK_TRUE_BYTES[0];
constexpr byte _JTOK_FALSE_START = _JTOK_FALSE_BYTES[0];

NODISCARD bool _jtok_try_value(json_tokenizer *t, byte b) {
  switch (b) {
  case '"':
    return _jtok_push_simple_token_with_state(t, JSON_TOKEN_STRING_DELIMITER);
  case '{':
    return _jtok_push_simple_token_with_state(t, JSON_TOKEN_OBJECT_OPEN);
  case '[':
    return _jtok_push_simple_token_with_state(t, JSON_TOKEN_ARRAY_OPEN);
  case _JTOK_NULL_START:
    return _jtok_start_fixed_value_with_state(t, JSON_TOKEN_NULL);
  case _JTOK_TRUE_START:
    return _jtok_start_fixed_value_with_state(t, JSON_TOKEN_TRUE);
  case _JTOK_FALSE_START:
    return _jtok_start_fixed_value_with_state(t, JSON_TOKEN_FALSE);
  case '-':
    if (!jsm_start_token(&t->jsm, JSON_TOKEN_NUMBER)) {
      return false;
    }
    t->partial_number_token.flags |= JSON_NUMBER_IS_NEGATIVE;
    return true;
  default:
    if (b >= '0' && b <= '9') {
      if (!jsm_start_token(&t->jsm, JSON_TOKEN_NUMBER)) {
        return false;
      }
      t->value_progress = 1;
      t->partial_number_token.integer_part = b - '0';
      if (b == '0') {
        t->partial_number_token.flags |= JSON_NUMBER_INTEGER_PART_IS_ZERO;
      }
      return true;
    }
    return false;
  }
}

NODISCARD INLINE bool _jtok_advance_parsing_of_fixed_token(json_tokenizer *t, byte b, const byte *expected_bytes, memsize expected_bytes_length, json_token_kind kind) {
  if (b != expected_bytes[t->value_progress]) {
    return false;
  }
  t->value_progress++;
  if (t->value_progress >= expected_bytes_length) {
    _jtok_push_simple_token(t, kind);
    jsm_end_token(&t->jsm, kind);
  }
  return true;
}

INLINE bool _jtok_is_hex(byte hex_char) {
  return (hex_char >= '0' && hex_char <= '9') || (hex_char >= 'a' && hex_char <= 'f') || (hex_char >= 'A' && hex_char <= 'F');
}

/** Returns a token from token queue, or null if the queue is empty. */
INLINE json_token *json_tokenizer_consume(json_tokenizer *t) {
  if (fqueue_get_count(&t->token_queue) == 0) {
    return NULL;
  }
  return fqueue_pop(&t->token_queue);
}

/** Render a token that is being built by the tokenizer into the buffer, with the same bytes the token was built with.
State of the tokenizer is not changed by this.

Returns amount of meaningful bytes written.
It's possible that more bytes were written, but caller should only use meaningful bytes. */
NODISCARD int64 json_tokenizer_render_partial_token(json_tokenizer *t, writer *w) {
  json_state state = jsm_get_current_state(&t->jsm);
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
    token.kind = JSON_TOKEN_ESCAPE_SEQUENCE;
    token.character.character = 'n';
    json_detokenizer_write(w, &token);
  }
    return 1;
  case JSON_STATE_ESCAPED_CHARCODE: {
    json_token token;
    token.kind = JSON_TOKEN_ESCAPED_UNICODE_CHARCODE;
    token.unicode_character = t->partial_unicode_token;
    json_detokenizer_write(w, &token);
  }
    return t->value_progress + 2;
  case JSON_STATE_UNICODE_CHARACTER: {
    json_token token;
    token.kind = JSON_TOKEN_UNICODE_CHARACTER;
    token.unicode_character = t->partial_unicode_token;
    json_detokenizer_write(w, &token);
  }
    return t->value_progress;
  case JSON_STATE_NUMBER:
    json_token token;
    token.kind = JSON_TOKEN_NUMBER;
    token.number = t->partial_number_token;
    return json_detokenizer_write(w, &token);
  default:
    // nothing to write, no pending token
    return 0;
  }
}

NODISCARD bool json_tokenizer_push(json_tokenizer *t, byte b);

NODISCARD bool _jtok_update_number(json_tokenizer *t, byte b) {
  if (b >= '0' && b <= '9') {
    byte digit = b - '0';
    if (t->partial_number_token.flags & JSON_NUMBER_HAS_EXPONENT) {
      if (t->value_progress == 0 && digit == 0) {
        t->partial_number_token.exponent_leading_zeroes++;
        if (t->partial_number_token.exponent_leading_zeroes == JSON_MAX_NUMBER_LEADING_ZEROES) {
          return false;
        }
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
        if (t->partial_number_token.fraction_leading_zeroes == JSON_MAX_NUMBER_LEADING_ZEROES) {
          return false;
        }
        return true;
      }
      if (_jtok_uint64_will_overflow(t->partial_number_token.fraction_part, digit)) {
        return false;
      }
      t->partial_number_token.fraction_part = (t->partial_number_token.fraction_part * 10) + digit;
      t->value_progress++;
      return true;
    } else {
      if (t->partial_number_token.flags & JSON_NUMBER_INTEGER_PART_IS_ZERO) {
        // `01` is invalid, `-01` is invalid
        return false;
      }
      if (_jtok_uint64_will_overflow(t->partial_number_token.integer_part, digit)) {
        return false;
      }
      if (digit == 0 && t->value_progress == 0) {
        t->partial_number_token.flags |= JSON_NUMBER_INTEGER_PART_IS_ZERO;
      } else {
        t->partial_number_token.integer_part = (t->partial_number_token.integer_part * 10) + digit;
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
      if (t->partial_number_token.flags & JSON_NUMBER_HAS_FRACTION) {
        if (t->partial_number_token.fraction_leading_zeroes == 0) {
          return false; // `1.e1` is invalid
        }
      } else {
        return false; // `-e1` is invalid
      }
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
    // it's not a part of a number. time to terminate the token
    if (t->value_progress == 0) {
      if (t->partial_number_token.flags & JSON_NUMBER_HAS_EXPONENT) {
        if (t->partial_number_token.exponent_leading_zeroes == 0) {
          return false; // `1e ` is invalid
        }
      } else if (t->partial_number_token.flags & JSON_NUMBER_HAS_FRACTION) {
        if (t->partial_number_token.fraction_leading_zeroes == 0) {
          return false; // `1. ` is invalid
        }
      } else {
        return false; // `- ` is invalid
      }
    }
    _jtok_push_number_token(t);
    jsm_end_token(&t->jsm, JSON_TOKEN_NUMBER);
    return json_tokenizer_push(t, b);
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
  json_state state = jsm_get_current_state(&t->jsm);
  switch (state) {
  case JSON_STATE_START:
    if (b == UTF8_BOM[0]) {
      return _jtok_start_fixed_value_with_state(t, JSON_TOKEN_BOM);
    } else {
      return _jtok_try_value(t, b) || _jtok_try_whitespace(t, b);
    }
  case JSON_STATE_VALUE:
    return _jtok_try_value(t, b) || _jtok_try_whitespace(t, b);
  case JSON_STATE_BOM:
    return _jtok_advance_parsing_of_fixed_token(t, b, UTF8_BOM, sizeof(UTF8_BOM), JSON_TOKEN_BOM);
  case JSON_STATE_TRUE:
    return _jtok_advance_parsing_of_fixed_token(t, b, _JTOK_TRUE_BYTES, sizeof(_JTOK_TRUE_BYTES), JSON_TOKEN_TRUE);
  case JSON_STATE_FALSE:
    return _jtok_advance_parsing_of_fixed_token(t, b, _JTOK_FALSE_BYTES, sizeof(_JTOK_FALSE_BYTES), JSON_TOKEN_FALSE);
  case JSON_STATE_NULL:
    return _jtok_advance_parsing_of_fixed_token(t, b, _JTOK_NULL_BYTES, sizeof(_JTOK_NULL_BYTES), JSON_TOKEN_NULL);
  case JSON_STATE_ARRAY_START:
    if (b == ']') {
      return _jtok_push_simple_token_with_state(t, JSON_TOKEN_ARRAY_CLOSE);
    }
    return _jtok_try_value(t, b) || _jtok_try_whitespace(t, b);
  case JSON_STATE_ARRAY_CONTINUE:
    if (b == ']') {
      return _jtok_push_simple_token_with_state(t, JSON_TOKEN_ARRAY_CLOSE);
    } else if (b == ',') {
      return _jtok_push_simple_token_with_state(t, JSON_TOKEN_VALUE_SEPARATOR);
    } else {
      return _jtok_try_whitespace(t, b);
    }
  case JSON_STATE_OBJECT_START:
    if (b == '}') {
      return _jtok_push_simple_token_with_state(t, JSON_TOKEN_OBJECT_CLOSE);
    } else if (b == '"') {
      return _jtok_push_simple_token_with_state(t, JSON_TOKEN_STRING_DELIMITER);
    } else {
      return _jtok_try_whitespace(t, b);
    }
  case JSON_STATE_OBJECT_CONTINUE:
    if (b == '}') {
      return _jtok_push_simple_token_with_state(t, JSON_TOKEN_OBJECT_CLOSE);
    } else if (b == ',') {
      return _jtok_push_simple_token_with_state(t, JSON_TOKEN_VALUE_SEPARATOR);
    } else {
      return _jtok_try_whitespace(t, b);
    }
  case JSON_STATE_OBJECT_KEY_START:
    if (b == '"') {
      return _jtok_push_simple_token_with_state(t, JSON_TOKEN_STRING_DELIMITER);
    } else {
      return _jtok_try_whitespace(t, b);
    }
  case JSON_STATE_OBJECT_KV_SEPARATOR:
    if (b == ':') {
      return _jtok_push_simple_token_with_state(t, JSON_TOKEN_KV_SEPARATOR);
    } else {
      return _jtok_try_whitespace(t, b);
    }
  case JSON_STATE_STRING:
  case JSON_STATE_OBJECT_KEY:
    if (b == '"') {
      return _jtok_push_simple_token_with_state(t, JSON_TOKEN_STRING_DELIMITER);
    } else if (b == '\\') {
      return jsm_start_token(&t->jsm, JSON_TOKEN_ESCAPE_SEQUENCE);
    } else {
      int64 utf8_length = utf8_get_sequence_length_by_first_byte(b);
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
        _jtok_push_unicode_token(t, JSON_TOKEN_UNICODE_CHARACTER);
        return true;
      }
      if (!jsm_start_token(&t->jsm, JSON_STATE_UNICODE_CHARACTER)) {
        t->partial_unicode_token = (json_unicode_token){0};
        return false;
      }
      t->value_progress = 1;
      return true;
    }
  case JSON_STATE_UNICODE_CHARACTER:
    if (!utf8_is_valid_continuation_byte(b)) {
      return false;
    }
    t->partial_unicode_token.value |= ((uint64)b) << (8 * t->value_progress);
    t->value_progress++;
    if (t->value_progress == t->partial_unicode_token.length) {
      _jtok_push_unicode_token(t, JSON_TOKEN_UNICODE_CHARACTER);
      jsm_end_token(&t->jsm, JSON_TOKEN_UNICODE_CHARACTER);
    }
    return true;
  case JSON_STATE_ESCAPED_CHARACTER:
    for (size_t i = 0; i < sizeof(_JTOK_ESCAPE_SEQ_CHARS); i++) {
      if (b == _JTOK_ESCAPE_SEQ_CHARS[i]) {
        _jtok_push_character_token(t, JSON_TOKEN_ESCAPE_SEQUENCE, b);
        jsm_end_token(&t->jsm, JSON_TOKEN_ESCAPE_SEQUENCE);
        return true;
      }
    }
    if (b == 'u') {
      if (!jsm_start_token(&t->jsm, JSON_TOKEN_ESCAPED_UNICODE_CHARCODE)) {
        return false;
      }
      t->partial_unicode_token.length = 4;
      return true;
    } else {
      return false;
    }
  case JSON_STATE_ESCAPED_CHARCODE:
    if (!_jtok_is_hex(b)) {
      return false;
    }
    // 4 hex bytes are stored like that to preserve case
    // as we must not lose any data at all during tokenization
    t->partial_unicode_token.value |= ((uint64)b) << (8 * t->value_progress);
    t->value_progress++;
    if (t->value_progress == 4) {
      jsm_end_token(&t->jsm, JSON_TOKEN_ESCAPED_UNICODE_CHARCODE);
      _jtok_push_unicode_token(t, JSON_TOKEN_ESCAPED_UNICODE_CHARCODE);
    }
    return true;
  case JSON_STATE_NUMBER:
    return _jtok_update_number(t, b);
  default: // JSON_STATE_ROOT as default for code coverage reasons
    return _jtok_try_whitespace(t, b);
  }
}

/** If there is some token in building and calling code reached the end of the stream -
this function can be called to flush the token.
returns false if token is in invalid state. */
NODISCARD bool json_tokenizer_flush(json_tokenizer *t) {
  // this is a hack, but it's better than replicating logic about properly closing every single token
  if (!json_tokenizer_push(t, ' ')) {
    return false;
  }
  int64 tokens_in_queue = fqueue_get_count(&t->token_queue);
  if (tokens_in_queue == 1) {
    fqueue_pop(&t->token_queue); // whitespace
  } else {
    assert(tokens_in_queue == 2 && "Json tokenizer queue was not properly consumed before flushing");
    json_token first_token = *(json_token *)fqueue_pop(&t->token_queue);
    fqueue_pop(&t->token_queue); // whitespace
    json_token *slot = fqueue_push(&t->token_queue);
    *slot = first_token;
  }

  return true;
}

INLINE json_state json_tokenizer_get_current_state(json_tokenizer *t) {
  return jsm_get_current_state(&t->jsm);
}

/** Returns true if the tokenizer have read one whole JSON completely.
In this state, tokenizer will only read whitespace tokens
(because we allow to have infinite number of whitespace tokens after the end of the actual value).
You can reset the tokenizer if you want to reuse it. */
INLINE bool json_tokenizer_is_done(json_tokenizer *t) {
  return json_tokenizer_get_current_state(t) == JSON_STATE_ROOT;
}