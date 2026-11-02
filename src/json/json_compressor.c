#pragma once
#include "../arithmetic_coding/encoder.c"
#include "../context.c"
#include "../string_compression/string_compressor.c"
#include "json_compression_base.c"
#include "json_tokenizer.c"
#include <limits.h>

typedef struct {
  json_compression_base base;
  acod_encoder *encoder;
  string_compressor *strcomp;
  json_tokenizer tokenizer;
} json_compressor;

void json_compressor_deinit(json_compressor *comp, context *context) {
  json_tokenizer_deinit(&comp->tokenizer, context);
  _json_compression_base_deinit(&comp->base, context);

  *comp = (json_compressor){0};
}

NODISCARD bool json_compressor_init(json_compressor *comp, context *context, reader *reader, acod_encoder *encoder, string_compressor *strcomp) {
  *comp = (json_compressor){0};

  comp->strcomp = strcomp;
  comp->encoder = encoder;

  if (!_json_compression_base_init(&comp->base, context)) {
    json_compressor_deinit(comp, context);
    return false;
  }

  if (!json_tokenizer_init(&comp->tokenizer, context, reader)) {
    json_compressor_deinit(comp, context);
    return false;
  }

  return true;
}

void _json_compressor_reset(json_compressor *comp) {
  _json_compression_base_reset(&comp->base);
  json_tokenizer_reset(&comp->tokenizer);
}

void _json_compressor_write_string_token(json_compressor *comp, json_token *token) {
  switch (token->kind) {
  case JSON_TOKEN_UNICODE_CHARACTER: {
    uint64 value = token->unicode_character.value;
    for (int32 i = 0; i < token->unicode_character.length; i++) {
      strcomp_write(comp->strcomp, value & 0xff);
      value = value >> CHAR_BIT;
    }
  } break;

  case JSON_TOKEN_ESCAPE_SEQUENCE: {
    strcomp_write(comp->strcomp, '\\');
    strcomp_write(comp->strcomp, token->character.character);
  } break;

  case JSON_TOKEN_ESCAPED_UNICODE_CHARCODE: {
    strcomp_write(comp->strcomp, '\\');
    strcomp_write(comp->strcomp, 'u');
    uint64 value = token->unicode_character.value;
    for (int32 i = 0; i < token->unicode_character.length; i++) {
      strcomp_write(comp->strcomp, value & 0xff);
      value = value >> CHAR_BIT;
    }
  } break;

  default: { // case JSON_TOKEN_STRING_DELIMITER:
    strcomp_flush(comp->strcomp);
  } break;
  }
}

void _json_compressor_write_nonstring_token(json_compressor *comp, json_token *token) {
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

/** This method should be called when it's time to terminate the JSON. */
void json_compressor_finalize(json_compressor *comp) {
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

  if (!json_is_string_state(comp->base.current_state)) {
    _json_compressor_write_nonstring_token(comp, &token);
    return true;
  }

  // when we are writing string parts - we should use string compression for every single part of it
  _json_compressor_write_string_token(comp, &token);
  if (token.kind == JSON_TOKEN_STRING_DELIMITER) {
    _json_compressor_write_nonstring_token(comp, &token);
  }
  return true;
}