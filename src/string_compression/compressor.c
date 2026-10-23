#pragma once
#include "../arithmetic_coding/encoder.c"
#include "../arithmetic_coding/frequency_table.c"
#include "../commons.c"
#include "../context.c"
#include "../data_structures/ring_buffer.c"
#include "../math.c"
#include "string_compression_base.c"
#include <stdint.h>

typedef struct {
  // buffer that will be searched for sequences that can be replaced by a backreference
  ring_buffer input_buffer;
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
  ring_buffer_deinit(&compressor->input_buffer, context);
  _strcomp_deinit_base(&compressor->base, context);

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

  return true;
}

void _strcomp_write_uint(string_compressor *compressor, ftable *table, ftable *last_part_table, uint64_t value) {
  int bit_index = most_significant_bit_index(value);
  symbol length_symbols_symbol = (bit_index + (int)_STRCOMP_UINT_SYMBOL_BITS_COUNT) / _STRCOMP_UINT_SYMBOL_BITS_COUNT;
  acod_encoder_write(compressor->encoder, table, length_symbols_symbol);
  ftable_increment(table, length_symbols_symbol);

  for (symbol i = 0; i < length_symbols_symbol; i++) {
    byte part = value & _STRCOMP_UINT_SYMBOL_MASK;
    value = value >> _STRCOMP_UINT_SYMBOL_BITS_COUNT;
    bool is_last_part = i == (length_symbols_symbol - 1);
    if (is_last_part) {
      acod_encoder_write(compressor->encoder, last_part_table, part);
      ftable_increment(last_part_table, part);
    } else {
      acod_encoder_write(compressor->encoder, &compressor->base.uint_parts, part);
    }
  }
}

void _strcomp_write_backreference(string_compressor *compressor, uint64_t offset, uint64_t length) {
  acod_encoder_write(compressor->encoder, &compressor->base.main_symbol_table, _STRCOMP_BACKREFERENCE_SYMBOL);
  ftable_increment(&compressor->base.main_symbol_table, _STRCOMP_BACKREFERENCE_SYMBOL);
  // we could, in theory, subtract length from offset, to make the number smaller
  // but then we'll have to deal with byte_stream_matcher returning matches that pass the ring buffer boundary; matches with offset < length
  // we can forbid such matches, but then we'll have performance problems on repeated data that aligns with ring buffer size and perfectly loops
  // because we would have to reject those over and over again and write the data instead, which is slow and harms the compression
  // of course it won't happen in actual data, only in synthetic data; but the compressor should be resilient to such attacks
  // (and also, subtracting length from offset doesn't add much to compression quality. it's something about 0.05%)
  _strcomp_write_uint(compressor, &compressor->base.backref_offset_length, &compressor->base.backref_offset_last_part, offset);
  _strcomp_write_uint(compressor, &compressor->base.backref_length_length, &compressor->base.backref_length_last_part, length);
}

void _strcomp_compressor_flush_once(string_compressor *compressor) {
  int64_t buffer_start = (int64_t)ring_buffer_get_index(&compressor->input_buffer) - compressor->bytes_in_buffer;
  if (buffer_start < 0) {
    buffer_start += _BSM_MATCH_LENGTH_LIMIT;
  }
  // if we can't even properly take hash of the bytes - don't attempt to find anything
  bsm_match match = (compressor->bytes_in_buffer < _BSM_HASH_LENGTH_BYTES) ? BSM_MATCH_EMPTY : bsm_find_match(&compressor->bsm, &compressor->input_buffer, buffer_start);

  if (match.length >= _STRCOMP_MIN_BACKREFERENCE_LENGTH) {
    _strcomp_write_backreference(compressor, match.offset, match.length);

    // update bsm with values from backreference
    int64_t end = buffer_start + match.length;
    for (int64_t i = buffer_start; i < end; i++) {
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
void strcomp_write(string_compressor *compressor, byte b) {
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
  // printf("writing char: %zu\n", _STRCOMP_EOF_SYMBOL);
  acod_encoder_write(compressor->encoder, &compressor->base.main_symbol_table, _STRCOMP_EOF_SYMBOL);
  ftable_increment(&compressor->base.main_symbol_table, _STRCOMP_EOF_SYMBOL);
}