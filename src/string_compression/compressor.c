#pragma once
#include "../arithmetic_coding/encoder.c"
#include "../arithmetic_coding/frequency_table.c"
#include "../commons.c"
#include "../context.c"
#include "../data_structures/ring_buffer.c"
#include "base.c"

typedef struct {
  string_compression_base base;
  acod_encoder *encoder;
  // TODO: rethink data types everywhere. why can't everything be int32_t?
  int32_t bytes_in_buffer;
  byte_stream_matcher bsm;
} string_compressor;

/** Don't forget to flush the compressor before calling this */
// TODO: unite flush and deinit, like with acod
void strcomp_deinit(string_compressor *compressor, context *context) {
  bsm_deinit(&compressor->bsm, context);
  _strcomp_deinit_base(&compressor->base, context);
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

  return true;
}

void _strcomp_write_uint(string_compressor *compressor, uint64_t value) {
  while (value > 0) {
    byte bit = value & 1;
    // printf("writing bit: %i\n", bit);
    acod_encoder_write(compressor->encoder, &compressor->base.raw_bit_with_terminator_table, bit);
    // TODO: think about pre-initializing this table instead of learning on the actual stream
    // maybe it would be something like 8,8,1
    // using frequency tables like that should be very unefficient, unless we batch updates
    ftable_increment(&compressor->base.raw_bit_with_terminator_table, bit);
    value = value >> 1;
  }
  // printf("writing bit: %zu\n", _STRCOMP_BIT_TERMINATOR_SYMBOL);
  acod_encoder_write(compressor->encoder, &compressor->base.raw_bit_with_terminator_table, _STRCOMP_BIT_TERMINATOR_SYMBOL);
  ftable_increment(&compressor->base.raw_bit_with_terminator_table, _STRCOMP_BIT_TERMINATOR_SYMBOL);
}

// TODO: think about better ways of writing backreferences. this is very suboptimal
// do it like deflate does it?
void _strcomp_write_backreference(string_compressor *compressor, uint64_t offset, uint64_t length) {
  // TODO: writing a backreference does not populate lookback buffer
  // maybe it should? adjust decompressor too if yes
  // printf("writing char: %zu\n", _STRCOMP_BACKREFERENCE_SYMBOL);
  acod_encoder_write(compressor->encoder, &compressor->base.main_symbol_table, _STRCOMP_BACKREFERENCE_SYMBOL);
  ftable_increment(&compressor->base.main_symbol_table, _STRCOMP_BACKREFERENCE_SYMBOL);
  _strcomp_write_uint(compressor, offset);
  _strcomp_write_uint(compressor, length);
}

void _strcomp_compressor_flush_once(string_compressor *compressor) {
  int64_t buffer_start = (int64_t)ring_buffer_get_index(&compressor->base.buffer) - compressor->bytes_in_buffer;
  if (buffer_start < 0) {
    buffer_start += (int64_t)_BSM_MATCH_LENGTH_LIMIT;
  }
  // if we can't even properly take hash of the bytes - don't attempt to find anything
  bsm_match match = (compressor->bytes_in_buffer < _BSM_HASH_LENGTH_BYTES) ? BSM_MATCH_EMPTY : bsm_find_match(&compressor->bsm, &compressor->base.buffer, buffer_start);

  if (match.length >= _STRCOMP_MIN_BACKREFERENCE_LENGTH) {
    // TODO: subtract stuff to make values smaller
    _strcomp_write_backreference(compressor, match.offset, match.length);
    compressor->bytes_in_buffer -= match.length;
    assert(compressor->bytes_in_buffer >= 0);
  }

  byte first_byte = ring_buffer_get(&compressor->base.buffer, buffer_start);
  // printf("writing char: %i\n", first_byte);
  acod_encoder_write(compressor->encoder, &compressor->base.main_symbol_table, first_byte);
  ftable_increment(&compressor->base.main_symbol_table, first_byte);
  bsm_push(&compressor->bsm, first_byte);
  compressor->bytes_in_buffer--;
  assert(compressor->bytes_in_buffer >= 0);
}

/** Put a byte in compressor's buffer.
This may result in some symbols being written out, or not. */
void strcomp_write(string_compressor *compressor, byte b) {
  compressor->bytes_in_buffer++;
  ring_buffer_push(&compressor->base.buffer, b);
  if (compressor->bytes_in_buffer == _BSM_MATCH_LENGTH_LIMIT - 1) {
    _strcomp_compressor_flush_once(compressor);
  }
}

/** Write out everything the compressor has in its buffer, and then write EOF marker */
void strcomp_flush(string_compressor *compressor) {
  while (compressor->bytes_in_buffer > 0) {
    _strcomp_compressor_flush_once(compressor);
  }
  // printf("writing char: %zu\n", _STRCOMP_EOF_SYMBOL);
  acod_encoder_write(compressor->encoder, &compressor->base.main_symbol_table, _STRCOMP_EOF_SYMBOL);
  ftable_increment(&compressor->base.main_symbol_table, _STRCOMP_EOF_SYMBOL);
}