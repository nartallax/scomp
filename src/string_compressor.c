#pragma once
#include "arithmetic_coding.c"
#include "byte_stream_matcher.c"
#include "commons.c"
#include "context.c"
#include "ring_buffer.c"

const symbol _strcomp_backreference_symbol = 256;
const symbol _strcomp_bit_terminator_symbol = 2;
// TODO: tune this
const size_t _strcomp_min_backreference_length = 5;

typedef struct {
  context *context;
  acod_encoder *encoder;
  ftable *main_symbol_table;
  ftable *raw_bit_with_terminator_table;
  int32_t bytes_in_buffer;
  ring_buffer buffer;
  byte_stream_matcher bsm;
} string_compressor;

/** Don't forget to flush the compressor */
void strcomp_compressor_deinit(string_compressor *compressor) {
  bsm_deinit(&compressor->bsm);
  ring_buffer_deinit(&compressor->buffer);
  if (compressor->main_symbol_table != NULL) {
    ftable_delete(compressor->main_symbol_table);
  }
  if (compressor->raw_bit_with_terminator_table != NULL) {
    ftable_delete(compressor->raw_bit_with_terminator_table);
  }
}

NODISCARD bool strcomp_compressor_init(string_compressor *compressor, context *context, acod_encoder *encoder, ftable *table, size_t buffer_size) {
  compressor->encoder = encoder;
  compressor->main_symbol_table = table;
  compressor->context = context;

  if (!ring_buffer_init(&compressor->buffer, context, _BSM_MATCH_LENGTH_SHIFT)) {
    strcomp_compressor_deinit(compressor);
    return false;
  }

  if (!bsm_init(&compressor->bsm, context)) {
    strcomp_compressor_deinit(compressor);
    return false;
  }

  // TODO: do we really need eof here?
  // TODO: convert ftables to init/deinit
  compressor->main_symbol_table = ftable_new(context, 256, FTABLE_INIT_ONE | FTABLE_INCLUDE_EOF);
  if (!compressor->main_symbol_table) {
    strcomp_compressor_deinit(compressor);
    return false;
  }

  compressor->raw_bit_with_terminator_table = ftable_new(context, 3, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF);
  if (!compressor->raw_bit_with_terminator_table) {
    strcomp_compressor_deinit(compressor);
    return false;
  }

  return true;
}

NODISCARD bool _strcomp_write_uint(string_compressor *compressor, uint64_t value) {
  while (value > 0) {
    byte bit = value & 1;
    if (!acod_encoder_write(compressor->encoder, compressor->raw_bit_with_terminator_table, bit)) {
      return false;
    }
    // TODO: think about pre-initializing this table instead of learning on the actual stream
    // maybe it would be something like 8,8,1
    // using frequency tables like that should be very unefficient, unless we batch updates
    ftable_increment(compressor->raw_bit_with_terminator_table, bit);
    value = value >> 1;
  }
  if (!acod_encoder_write(compressor->encoder, compressor->raw_bit_with_terminator_table, _strcomp_bit_terminator_symbol)) {
    return false;
  }
  ftable_increment(compressor->raw_bit_with_terminator_table, _strcomp_bit_terminator_symbol);
  return true;
}

// TODO: think about better ways of writing backreferences. this is very suboptimal
// do it like deflate does it?
NODISCARD bool _strcomp_write_backreference(string_compressor *compressor, uint64_t offset, uint64_t length) {
  if (!acod_encoder_write(compressor->encoder, compressor->main_symbol_table, _strcomp_backreference_symbol)) {
    return false;
  }
  if (!_strcomp_write_uint(compressor, offset)) {
    return false;
  }
  if (!_strcomp_write_uint(compressor, length)) {
    return false;
  }
  return true;
}

NODISCARD bool _strcomp_compressor_flush_once(string_compressor *compressor) {
  int64_t buffer_start = (int64_t)ring_buffer_get_index(&compressor->buffer) - compressor->bytes_in_buffer;
  if (buffer_start < 0) {
    buffer_start += (int64_t)_BSM_MATCH_LENGTH_LIMIT;
  }
  // if we can't even properly take hash of the bytes - don't attempt to find anything
  bsm_match match = (compressor->bytes_in_buffer < _BSM_HASH_LENGTH_BYTES) ? empty_bsm_match : bsm_find_match(&compressor->bsm, &compressor->buffer, buffer_start);

  if (match.length >= _strcomp_min_backreference_length) {
    if (!_strcomp_write_backreference(compressor, match.offset, match.length)) {
      return false;
    }
    compressor->bytes_in_buffer -= match.length;
    assert(compressor->bytes_in_buffer >= 0);
  }

  byte first_byte = ring_buffer_get(&compressor->buffer, buffer_start);
  if (!acod_encoder_write(compressor->encoder, compressor->main_symbol_table, first_byte)) {
    return false;
  }
  compressor->bytes_in_buffer--;
  return true;
}

/** Put a byte in compressor's buffer.
This may result in some symbols being written out, or not. */
NODISCARD bool strcomp_compressor_write(string_compressor *compressor, byte b) {
  compressor->bytes_in_buffer++;
  ring_buffer_push(&compressor->buffer, b);
  bsm_push(&compressor->bsm, b);
  if (compressor->bytes_in_buffer == _BSM_MATCH_LENGTH_LIMIT - 1) {
    if (!_strcomp_compressor_flush_once(compressor)) {
      return false;
    }
  }

  return true;
}

/** Write out everything the compressor has in its buffer */
NODISCARD bool strcomp_compressor_flush(string_compressor *compressor) {
  while (compressor->bytes_in_buffer > 0) {
    if (!_strcomp_compressor_flush_once(compressor)) {
      return false;
    }
  }
  return true;
}