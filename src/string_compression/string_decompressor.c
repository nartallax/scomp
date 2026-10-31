#pragma once
#include "../arithmetic_coding/decoder.c"
#include "../arithmetic_coding/uint_reader.c"
#include "../commons.c"
#include "../data_structures/ring_buffer.c"
#include "../data_structures/writer.c"
#include "byte_stream_matcher.c"
#include "string_compression_base.c"
#include <stdbool.h>

typedef struct {
  // buffer that stores received characters and supplies sequence values for backreferences
  ring_buffer lookback_buffer;
  string_compression_base base;
  writer *writer;
  acod_decoder *decoder;
} string_decompressor;

void strdecomp_deinit(string_decompressor *decomp, context *context) {
  ring_buffer_deinit(&decomp->lookback_buffer, context);
  _strcomp_deinit_base(&decomp->base, context);

  *decomp = (string_decompressor){0};
}

NODISCARD bool strdecomp_init(string_decompressor *decomp, context *context, writer *writer, acod_decoder *decoder) {
  *decomp = (string_decompressor){0};

  decomp->writer = writer;
  decomp->decoder = decoder;

  if (!_strcomp_init_base(&decomp->base, context)) {
    strdecomp_deinit(decomp, context);
    return false;
  }

  if (!ring_buffer_init(&decomp->lookback_buffer, context, _BSM_LENGTH_SHIFT)) {
    strdecomp_deinit(decomp, context);
    return false;
  }

  return true;
}

void _strdecomp_materialize_backreference(string_decompressor *decomp, int64 offset, int64 length) {
  int64 start_index = (int64)ring_buffer_get_index(&decomp->lookback_buffer) - (offset + length);
  if (start_index < 0) {
    start_index += (int64)ring_buffer_get_length(&decomp->lookback_buffer);
  }

  for (int64 i = 0; i < length; i++) {
    byte b = ring_buffer_get(&decomp->lookback_buffer, start_index + i);
    writer_write_byte(decomp->writer, b);
    ring_buffer_push(&decomp->lookback_buffer, b);
  }
}

void _strdecomp_read_backreference(string_decompressor *decomp) {
  int64 offset = (int64)uint_reader_read(&decomp->base.offset_tables, decomp->decoder);
  int64 length = (int64)uint_reader_read(&decomp->base.length_tables, decomp->decoder);

  _strdecomp_materialize_backreference(decomp, offset, length);
}

/** Consumes symbols from the decoder.
Returns on EOF (false), or when there are no more symbols (true). */
NODISCARD bool strdecomp_read(string_decompressor *decomp) {
  ftable *table = &decomp->base.main_symbol_table;
  symbol s = acod_decoder_read(decomp->decoder, table);
  ftable_increment(table, s);

  if (s == ftable_get_eof_symbol(&decomp->base.main_symbol_table)) {
    return false;
  }

  if (s == _STRCOMP_BACKREFERENCE_SYMBOL) {
    _strdecomp_read_backreference(decomp);
    return true;
  }

  // normal character
  writer_write_byte(decomp->writer, (byte)s);
  ring_buffer_push(&decomp->lookback_buffer, (byte)s);
  return true;
}
