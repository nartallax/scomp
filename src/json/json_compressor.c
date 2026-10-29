#pragma once
#include "../arithmetic_coding/encoder.c"
#include "../context.c"
#include "../string_compression/string_compressor.c"
#include "../writer.c"
#include "json_compression_base.c"
#include "json_tokenizer.c"

constexpr int64 _JSON_COMPRESSOR_MAX_PARTIAL_TOKEN_LENGTH = 256;

typedef struct {
  json_compression_base base;
  acod_encoder *encoder;
  string_compressor *strcomp;
  json_tokenizer tokenizer;
  // this writer is used to receive values of partial tokens in case of invalid JSON
  writer partial_token_writer;
} json_compressor;

void json_compressor_deinit(json_compressor *comp, context *context) {
  json_tokenizer_deinit(&comp->tokenizer, context);
  _json_compression_base_deinit(&comp->base, context);
  writer_deinit(&comp->partial_token_writer, context);

  *comp = (json_compressor){0};
}

NODISCARD bool json_compressor_init(json_compressor *comp, context *context, acod_encoder *encoder, string_compressor *strcomp) {
  *comp = (json_compressor){0};

  comp->strcomp = strcomp;
  comp->encoder = encoder;

  if (!_json_compression_base_init(&comp->base, context)) {
    json_compressor_deinit(comp, context);
    return false;
  }

  if (!json_tokenizer_init(&comp->tokenizer, context)) {
    json_compressor_deinit(comp, context);
    return false;
  }

  if (!writer_init(&comp->partial_token_writer, context, _JSON_COMPRESSOR_MAX_PARTIAL_TOKEN_LENGTH, 1)) {
    json_compressor_deinit(comp, context);
    return false;
  }

  return true;
}

void _json_compressor_reset(json_compressor *comp) {
  _json_compression_base_reset(&comp->base);
  json_tokenizer_reset(&comp->tokenizer);
}

bool _json_compressor_write_string_token(json_compressor *comp, json_token *token) {
  switch (token->kind) {
  case JSON_TOKEN_UNICODE_CHARACTER: {
    uint64 value = token->unicode_character.value;
    // TODO: test for correct byte order
    for (int32 i = 0; i < token->unicode_character.length; i++) {
      strcomp_write(comp->strcomp, value & 0xff);
      value = value >> 8;
    }
    return false;
  }

  case JSON_TOKEN_ESCAPE_SEQUENCE: {
    strcomp_write(comp->strcomp, '\\');
    strcomp_write(comp->strcomp, token->character.character);
    return false;
  }

  case JSON_TOKEN_ESCAPED_UNICODE_CHARCODE: {
    strcomp_write(comp->strcomp, '\\');
    strcomp_write(comp->strcomp, 'u');
    uint64 value = token->unicode_character.value;
    for (int32 i = 0; i < token->unicode_character.length; i++) {
      strcomp_write(comp->strcomp, value & 0xff);
      value = value >> 8;
    }
    return false;
  }

  case JSON_TOKEN_STRING_DELIMITER: {
    strcomp_flush(comp->strcomp);
    return true;
  }

  default:
    assert(false && "Unreachable, there should be no other token kinds here");
  }
}

void _json_compressor_write_nonstring_token(json_compressor *comp, json_token *token) {
  ftable *table = _json_compression_base_get_current_ftable(&comp->base);
  acod_encoder_write(comp->encoder, table, token->kind);
  ftable_increment(table, token->kind);
  comp->base.current_state = json_tokenizer_get_current_state(&comp->tokenizer);

  switch (token->kind) {
  case JSON_TOKEN_WHITESPACE: {
    ftable *table = &comp->base.whitespaces;
    symbol s = _json_compression_get_symbol_by_whitespace_char(token->character.character);
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

void _json_compressor_drain_tokens(json_compressor *comp) {
  json_token *token;
  while (true) {
    token = json_tokenizer_consume(&comp->tokenizer);
    if (!token) {
      break;
    }

    if (json_is_string_state(comp->base.current_state)) {
      // when we are writing string parts - we should use string compression for every single part of it
      if (!_json_compressor_write_string_token(comp, token)) {
        continue;
      }
    }

    _json_compressor_write_nonstring_token(comp, token);
  }
}

/** This method must be called in case byte push returned false. It will write some bytes and return a buffer.
Buffer contains characters that tokenizer thought are okay, but then discovered they are not;
as in, `truI` - first three characters will be returned, and must be re-consumed somehow.
(last character in this example, I, was the byte pushed and rejected by byte push, so it won't be returned here) */
NODISCARD buffer json_compressor_abort(json_compressor *comp) {
  _json_compressor_drain_tokens(comp);

  int64 bytes_written = json_tokenizer_render_partial_token(&comp->tokenizer, &comp->partial_token_writer);
  assert(bytes_written < _JSON_COMPRESSOR_MAX_PARTIAL_TOKEN_LENGTH && "Unexpectedly long string representation of a partial token");

  buffer b = writer_peek_current_buffer(&comp->partial_token_writer);
  writer_reset_current_buffer(&comp->partial_token_writer);
  b.length = bytes_written;

  ftable *table = _json_compression_base_get_current_ftable(&comp->base);
  symbol eof = ftable_get_eof_symbol(table);
  acod_encoder_write(comp->encoder, table, eof);
  // we shouldn't increment here. JSON breaks like that should be rare, and should not be related to a value type

  return b;
}

/** Push a byte through the compressor.
Returns false if invalid JSON sequence is encountered. In this case, `b` is not consumed. */
NODISCARD bool json_compressor_write(json_compressor *comp, byte b) {
  if (!json_tokenizer_push(&comp->tokenizer, b)) {
    return false;
  }

  _json_compressor_drain_tokens(comp);
  return true;
}