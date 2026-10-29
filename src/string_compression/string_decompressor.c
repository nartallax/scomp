#pragma once
#include "../arithmetic_coding/decoder.c"
#include "../arithmetic_coding/uint_reader.c"
#include "../commons.c"
#include "../data_structures/ring_buffer.c"
#include "../writer.c"
#include "byte_stream_matcher.c"
#include "string_compression_base.c"
#include <stdbool.h>

typedef byte _strcomp_state;
constexpr _strcomp_state _STRDECOMP_STATE_CHARACTER = 0;
constexpr _strcomp_state _STRDECOMP_STATE_OFFSET = 1;
constexpr _strcomp_state _STRDECOMP_STATE_LENGTH = 2;

typedef struct {
  // buffer that stores received characters and supplies sequence values for backreferences
  ring_buffer lookback_buffer;
  uint_reader_state uint_reader;
  string_compression_base base;
  writer *writer;
  acod_decoder *decoder;
  _strcomp_state state;
  int64 length;
  int64 offset;
} string_decompressor;

void strdecomp_deinit(string_decompressor *decomp, context *context) {
  ring_buffer_deinit(&decomp->lookback_buffer, context);
  _strcomp_deinit_base(&decomp->base, context);
  uint_reader_reset(&decomp->uint_reader);

  *decomp = (string_decompressor){0};
}

NODISCARD bool strdecomp_init(string_decompressor *decomp, context *context, writer *writer, acod_decoder *decoder) {
  *decomp = (string_decompressor){0};

  decomp->writer = writer;
  decomp->decoder = decoder;
  decomp->state = _STRDECOMP_STATE_CHARACTER;
  decomp->length = 0;
  decomp->offset = 0;

  if (!_strcomp_init_base(&decomp->base, context)) {
    strdecomp_deinit(decomp, context);
    return false;
  }

  if (!ring_buffer_init(&decomp->lookback_buffer, context, _BSM_LENGTH_SHIFT)) {
    strdecomp_deinit(decomp, context);
    return false;
  }

  uint_reader_reset(&decomp->uint_reader);

  return true;
}

// returns true in case of EOF
NODISCARD bool _strdecomp_read_character_symbol(string_decompressor *decomp) {
  ftable *table = &decomp->base.main_symbol_table;
  symbol s = acod_decoder_read(decomp->decoder, table);
  ftable_increment(table, s);
  if (s == _STRCOMP_BACKREFERENCE_SYMBOL) {
    decomp->state = _STRDECOMP_STATE_OFFSET;
    return false;
  }

  if (s == ftable_get_eof_symbol(&decomp->base.main_symbol_table)) {
    return true;
  }

  writer_write_byte(decomp->writer, (byte)s);
  ring_buffer_push(&decomp->lookback_buffer, (byte)s);

  return false;
}

void _strdecomp_materialize_existing_backreference(string_decompressor *decomp) {
  int64 start_index = (int64)ring_buffer_get_index(&decomp->lookback_buffer) - (decomp->offset + decomp->length);
  if (start_index < 0) {
    start_index += (int64)ring_buffer_get_length(&decomp->lookback_buffer);
  }
  int64 length = decomp->length;

  for (int64 i = 0; i < length; i++) {
    byte b = ring_buffer_get(&decomp->lookback_buffer, start_index + i);
    writer_write_byte(decomp->writer, b);
    ring_buffer_push(&decomp->lookback_buffer, b);
  }
}

void _strdecomp_read_backreference_symbol(string_decompressor *decomp) {
  bool is_offset = decomp->state == _STRDECOMP_STATE_OFFSET;
  uint_reader_state *reader = &decomp->uint_reader;
  uint_freq_tables *tables = is_offset ? &decomp->base.offset_tables : &decomp->base.length_tables;
  if (uint_reader_read(reader, tables, decomp->decoder)) {
    if (is_offset) {
      decomp->offset = uint_reader_consume(reader);
      decomp->state = _STRDECOMP_STATE_LENGTH;
    } else {
      decomp->length = uint_reader_consume(reader);
      _strdecomp_materialize_existing_backreference(decomp);
      decomp->length = 0;
      decomp->offset = 0;
      decomp->state = _STRDECOMP_STATE_CHARACTER;
    }
  }
}

/** Consumes symbols from the decoder.
Returns on EOF (false), or when there are no more symbols (true). */
NODISCARD bool strdecomp_read(string_decompressor *decomp) {
  while (acod_decoder_has_symbol(decomp->decoder)) {
    // reading a symbol
    if (decomp->state == _STRDECOMP_STATE_CHARACTER) {
      if (_strdecomp_read_character_symbol(decomp)) {
        return false;
      }
    } else {
      _strdecomp_read_backreference_symbol(decomp);
    }
  }

  return true;
}
