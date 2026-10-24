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

void _json_compressor_drain_tokens(json_compressor *comp) {
  json_token *token;
  while (true) {
    token = json_tokenizer_consume(&comp->tokenizer);
    if (!token) {
      break;
    }
    ftable *table = _json_compression_base_get_current_ftable(&comp->base);
    acod_encoder_write(comp->encoder, table, token->kind);
    ftable_increment(table, token->kind);
    comp->base.current_state = json_tokenizer_get_current_state(&comp->tokenizer);
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
NODISCARD bool json_compressor_push_byte(json_compressor *comp, byte b) {
  if (!json_tokenizer_push(&comp->tokenizer, b)) {
    return false;
  }

  _json_compressor_drain_tokens(comp);
  return true;
}