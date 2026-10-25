#pragma once
#include "../arithmetic_coding/encoder.c"
#include "../arithmetic_coding/frequency_table.c"
#include "../arithmetic_coding/uint_writer.c"
#include "../commons.c"
#include "../context.c"
#include "../data_structures/ring_buffer.c"
#include "byte_stream_matcher.c"
#include "string_compression_base.c"
#include <stdint.h>

typedef struct {
  // buffer that will be searched for sequences that can be replaced by a backreference
  ring_buffer input_buffer;
  string_compression_base base;
  acod_encoder *encoder;
  int32 bytes_in_buffer;
  byte_stream_matcher bsm;
  uint_writer offset_writer;
  uint_writer length_writer;
} string_compressor;

/** Don't forget to flush the compressor before calling this */
// TODO: unite flush and deinit, like with acod
void strcomp_deinit(string_compressor *compressor, context *context) {
  bsm_deinit(&compressor->bsm, context);
  ring_buffer_deinit(&compressor->input_buffer, context);
  _strcomp_deinit_base(&compressor->base, context);
  uint_writer_deinit(&compressor->offset_writer, context);
  uint_writer_deinit(&compressor->length_writer, context);

  *compressor = (string_compressor){0};
}

NODISCARD bool strcomp_init(string_compressor *compressor, context *context, acod_encoder *encoder) {
  *compressor = (string_compressor){0};
  compressor->encoder = encoder;
  if (!_strcomp_init_base(&compressor->base, context)) {
    return false;
  }

  if (!bsm_init(&compressor->bsm, context)) {
    strcomp_deinit(compressor, context);
    return false;
  }

  if (!ring_buffer_init(&compressor->input_buffer, context, _BSM_MATCH_LENGTH_SHIFT)) {
    strcomp_deinit(compressor, context);
    return false;
  }

  if (!uint_writer_init(&compressor->offset_writer, context, encoder, _BSM_LENGTH)) {
    strcomp_deinit(compressor, context);
    return false;
  }

  if (!uint_writer_init(&compressor->length_writer, context, encoder, _BSM_MATCH_LENGTH_LIMIT)) {
    strcomp_deinit(compressor, context);
    return false;
  }

  return true;
}

INLINE void _strcomp_write_backreference(string_compressor *compressor, int64 offset, int64 length) {
  acod_encoder_write(compressor->encoder, &compressor->base.main_symbol_table, _STRCOMP_BACKREFERENCE_SYMBOL);
  ftable_increment(&compressor->base.main_symbol_table, _STRCOMP_BACKREFERENCE_SYMBOL);
  uint_writer_write(&compressor->offset_writer, offset - length);
  uint_writer_write(&compressor->length_writer, length);
}

void _strcomp_compressor_flush_once(string_compressor *compressor) {
  int64 buffer_start = (int64)ring_buffer_get_index(&compressor->input_buffer) - compressor->bytes_in_buffer;
  if (buffer_start < 0) {
    buffer_start += _BSM_MATCH_LENGTH_LIMIT;
  }
  // if we can't even properly take hash of the bytes - don't attempt to find anything
  bsm_match match = (compressor->bytes_in_buffer < _BSM_HASH_LENGTH_BYTES) ? BSM_MATCH_EMPTY : bsm_find_match(&compressor->bsm, &compressor->input_buffer, buffer_start);

  if (match.length >= _STRCOMP_MIN_BACKREFERENCE_LENGTH) {
    _strcomp_write_backreference(compressor, match.offset, match.length);

    // update bsm with values from backreference
    int64 end = buffer_start + match.length;
    for (int64 i = buffer_start; i < end; i++) {
      byte b = ring_buffer_get(&compressor->input_buffer, i);
      bsm_push(&compressor->bsm, b);
    }

    compressor->bytes_in_buffer -= match.length;
    assert(compressor->bytes_in_buffer >= 0);
    return;
  }

  byte first_byte = ring_buffer_get(&compressor->input_buffer, buffer_start);
  acod_encoder_write(compressor->encoder, &compressor->base.main_symbol_table, first_byte);
  ftable_increment(&compressor->base.main_symbol_table, first_byte);
  bsm_push(&compressor->bsm, first_byte);
  compressor->bytes_in_buffer--;
  assert(compressor->bytes_in_buffer >= 0);
}

/** Put a byte in compressor's buffer.
This may result in some symbols being written out, or not. */
INLINE void strcomp_write(string_compressor *compressor, byte b) {
  ring_buffer_push(&compressor->input_buffer, b);
  compressor->bytes_in_buffer++;
  if (compressor->bytes_in_buffer == _BSM_MATCH_LENGTH_LIMIT - 1) {
    _strcomp_compressor_flush_once(compressor);
  }
}

/** Write out everything the compressor has in its buffer, and then write EOF marker */
void strcomp_flush(string_compressor *compressor) {
  while (compressor->bytes_in_buffer > 0) {
    _strcomp_compressor_flush_once(compressor);
  }
  symbol eof = ftable_get_eof_symbol(&compressor->base.main_symbol_table);
  acod_encoder_write(compressor->encoder, &compressor->base.main_symbol_table, eof);
  ftable_increment(&compressor->base.main_symbol_table, eof);
}