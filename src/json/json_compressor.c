#pragma once
#include "../arithmetic_coding/encoder.c"
#include "../arithmetic_coding/uint_freq_tables.c"
#include "../arithmetic_coding/uint_writer.c"
#include "../context.c"
#include "../data_structures/fixed_queue.c"
#include "../math.c"
#include "../string_compression/string_compressor.c"
#include "json_compression_base.c"
#include "json_shapes_repository.c"
#include "json_tokenizer.c"
#include <limits.h>
#include <stdint.h>

typedef struct {
  json_compression_base base;
  acod_encoder *encoder;
  string_compressor *strcomp;
  json_tokenizer tokenizer;
  json_shapes_repository shapes;
  fixed_queue token_queue;
  byte *string_token_receiver;
  uint_freq_tables shape_id_tables;
  fixed_stack shaped_object_progress;
} json_compressor;

void json_compressor_deinit(json_compressor *comp) {
  json_tokenizer_deinit(&comp->tokenizer);
  _json_compression_base_deinit(&comp->base);
  jsr_deinit(&comp->shapes);
  uint_freq_tables_deinit(&comp->shape_id_tables);
  fstack_deinit(&comp->shaped_object_progress);

  *comp = (json_compressor){0};
}

void json_compressor_init(json_compressor *comp, context *context, reader *reader, acod_encoder *encoder, string_compressor *strcomp) {
  *comp = (json_compressor){0};

  comp->strcomp = strcomp;
  comp->encoder = encoder;
  comp->string_token_receiver = context_arena_allocate(context, JSON_MAX_STRING_TOKEN_SOURCE_LENGTH, sizeof(byte));

  _json_compression_base_init(&comp->base, context);
  json_tokenizer_init(&comp->tokenizer, context, reader);
  jsr_init(&comp->shapes, context);
  uint_freq_tables_init(&comp->shape_id_tables, context, JSON_SHAPES_TOTAL_POOL_SIZE);
  fqueue_init(&comp->token_queue, context, sizeof(json_token), _JCOMP_TOKEN_BUFFER_LENGTH_BIT_WIDTH);
  fstack_init(&comp->shaped_object_progress, context, sizeof(byte), JSON_MAX_STATE_STACK_LENGTH);
}

void _json_compressor_reset(json_compressor *comp) {
  _json_compression_base_reset(&comp->base);
  json_tokenizer_reset(&comp->tokenizer);
}

void _json_compressor_write_string_token(json_compressor *comp, json_token *token) {
  if (token->kind == JSON_TOKEN_STRING_DELIMITER) {
    strcomp_flush(comp->strcomp);
    return;
  }

  int64 length = json_decompose_string_token_to_bytes(token, comp->string_token_receiver);
  for (int64 i = 0; i < length; i++) {
    strcomp_write(comp->strcomp, comp->string_token_receiver[i]);
  }
}

constexpr uint64 _JCOMP_QUARTER_MAX_ID = UINT64_MAX / 4;
constexpr uint64 _JCOMP_HALF_MAX_ID = UINT64_MAX / 2;
constexpr uint64 _JCOMP_THREEQUARTER_MAX_ID = 3 * (UINT64_MAX / 4);

json_object_shape *_json_compressor_get_object_shape_if_usable(json_token *token) {
  if (token->shape_reference.shape == NULL) {
    return NULL;
  }
  json_object_shape *shape = token->shape_reference.shape;

  uint64 token_id = token->shape_reference.shape_id;
  uint64 shape_id = shape->id;
  bool is_overflowed = min_uint64(token_id, shape_id) < _JCOMP_QUARTER_MAX_ID && max_uint64(token_id, shape_id) > _JCOMP_THREEQUARTER_MAX_ID;
  if (is_overflowed) {
    *min_uint64_p(&token_id, &shape_id) += _JCOMP_HALF_MAX_ID;
    *max_uint64_p(&token_id, &shape_id) -= _JCOMP_HALF_MAX_ID;
  }
  if (token_id < shape_id) {
    // if shape ID is larger than token's - this is wrong shape and we must not use it
    // (it was replaced by some other shape in the pool)
    return NULL;
  }

  return shape;
}

NODISCARD bool _json_compressor_try_writing_object_shape_reference(json_compressor *comp, json_token *token) {
  json_object_shape *shape = _json_compressor_get_object_shape_if_usable(token);
  if (shape == NULL) {
    return false;
  }

  ftable *table = _json_compression_base_get_current_ftable(&comp->base);
  acod_encoder_write(comp->encoder, table, JSON_COMPRESSION_SHAPE_REFERENCE_SYMBOL);
  ftable_increment(table, JSON_COMPRESSION_SHAPE_REFERENCE_SYMBOL);
  int64 offset = jsr_get_shape_offset(&comp->shapes, shape);
  uint_writer_write(&comp->shape_id_tables, comp->encoder, offset);
  byte *slot = fstack_push(&comp->shaped_object_progress);
  *slot = 0;
  return true;
}

void _json_compressor_skip_ongoing_string(json_compressor *comp) {
  json_token *token;
  while (true) {
    token = fqueue_pop(&comp->token_queue);
    if (token->kind == JSON_TOKEN_STRING_DELIMITER) {
      return;
    }
  }
}

void _json_compressor_write_nonstring_token(json_compressor *comp, json_token *token) {
  if (token->kind == JSON_TOKEN_OBJECT_OPEN) {
    if (_json_compressor_try_writing_object_shape_reference(comp, token)) {
      return;
    } else {
      byte *slot = fstack_push(&comp->shaped_object_progress);
      *slot = JSON_SHAPE_UNKNOWN;
    }
  } else if (token->kind == JSON_TOKEN_OBJECT_CLOSE) {
    fstack_pop(&comp->shaped_object_progress);
  }
  if (token->kind == JSON_TOKEN_STRING_DELIMITER && (comp->base.current_state == JSON_STATE_OBJECT_START || comp->base.current_state == JSON_STATE_OBJECT_KEY_START)) {
    byte *progress = fstack_peek(&comp->shaped_object_progress);
    if (*progress != JSON_SHAPE_UNKNOWN) {
      (*progress)++;
      ftable *table = _json_compression_base_get_current_ftable(&comp->base);
      acod_encoder_write(comp->encoder, table, JSON_COMPRESSION_CONTINUE_SHAPE_SYMBOL);
      ftable_increment(table, JSON_COMPRESSION_CONTINUE_SHAPE_SYMBOL);
      _json_compressor_skip_ongoing_string(comp);
      return;
    }
  }

  ftable *table = _json_compression_base_get_current_ftable(&comp->base);
  acod_encoder_write(comp->encoder, table, token->kind);
  // printf("Writing token: %i (state: %i)\n", token->kind, comp->base.current_state);
  ftable_increment(table, token->kind);
  comp->base.current_state = json_tokenizer_get_current_state(&comp->tokenizer);

  switch (token->kind) {
  case JSON_TOKEN_WHITESPACE: {
    ftable *table = &comp->base.whitespaces;
    symbol s = _json_compression_get_symbol_by_whitespace_char(token->character.character);
    // printf("Writing whitespace symbol: %i\n", s);
    acod_encoder_write(comp->encoder, table, s);
    ftable_increment(table, s);
  } break;

  case JSON_TOKEN_NUMBER: {
    uint_writer_write(&comp->base.number_flag_tables, comp->encoder, token->number.flags);
    if (!(token->number.flags & JSON_NUMBER_INTEGER_PART_IS_ZERO)) {
      uint_writer_write(&comp->base.number_int_tables, comp->encoder, token->number.integer_part);
    }
    if (token->number.flags & JSON_NUMBER_HAS_FRACTION) {
      uint_writer_write(&comp->base.number_frac_zero_tables, comp->encoder, token->number.fraction_leading_zeroes);
      uint_writer_write(&comp->base.number_frac_tables, comp->encoder, token->number.fraction_part);
    }
    if (token->number.flags & JSON_NUMBER_HAS_EXPONENT) {
      uint_writer_write(&comp->base.number_expo_zero_tables, comp->encoder, token->number.exponent_leading_zeroes);
      uint_writer_write(&comp->base.number_expo_tables, comp->encoder, token->number.exponent_part);
    }
  } break;
  }
}

void _json_compressor_flush_once(json_compressor *comp) {
  json_token *token = fqueue_pop(&comp->token_queue);
  if (!json_is_string_state(comp->base.current_state)) {
    _json_compressor_write_nonstring_token(comp, token);
    return;
  }

  // when we are writing string parts - we should use string compression for every single part of it
  _json_compressor_write_string_token(comp, token);
  if (token->kind == JSON_TOKEN_STRING_DELIMITER) {
    _json_compressor_write_nonstring_token(comp, token);
  }
}

/** This method should be called when it's time to terminate the JSON. */
void json_compressor_finalize(json_compressor *comp) {
  while (fqueue_get_count(&comp->token_queue) > 0) {
    _json_compressor_flush_once(comp);
  }

  if (json_is_string_state(comp->base.current_state)) {
    strcomp_flush(comp->strcomp);
  }

  ftable *table = _json_compression_base_get_current_ftable(&comp->base);
  symbol eof = ftable_get_eof_symbol(table);
  // printf("Writing eof: %i (state: %i)\n", eof, comp->base.current_state);
  acod_encoder_write(comp->encoder, table, eof);
  // we shouldn't increment here. JSON breaks like that should be rare, and should not be related to a value type

  _json_compressor_reset(comp);
}

/** Process next JSON token.
Returns false if invalid JSON sequence is encountered. */
NODISCARD bool json_compressor_process_next_token(json_compressor *comp) {
  json_token token = json_tokenizer_parse_next(&comp->tokenizer);
  if (token.kind == JSON_TOKEN_INVALID) {
    return false;
  }

  if (fqueue_get_count(&comp->token_queue) == fqueue_get_capacity(&comp->token_queue)) {
    _json_compressor_flush_once(comp);
  }
  json_token *slot = fqueue_push(&comp->token_queue);
  *slot = token;
  jsr_process_token(&comp->shapes, slot, json_tokenizer_get_current_state(&comp->tokenizer));
  return true;
}