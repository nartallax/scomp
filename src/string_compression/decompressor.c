#pragma once
#include "../arithmetic_coding/decoder.c"
#include "../commons.c"
#include "../writer.c"
#include "string_compression_base.c"
#include <stdbool.h>

typedef enum {
  _STRDECOMP_STATE_CHARACTER = 0,
  _STRDECOMP_STATE_OFFSET = 1 << 0,
  _STRDECOMP_STATE_START = 1 << 1,
  _STRDECOMP_STATE_LENGTH = 1 << 2
} _strcomp_state;

typedef struct {
  // buffer that stores received characters and supplies sequence values for backreferences
  ring_buffer lookback_buffer;
  string_compression_base base;
  writer *writer;
  acod_decoder *decoder;
  _strcomp_state state;
  int64_t length;
  int64_t offset;
  byte uint_read_symbol_count;
  byte uint_total_symbol_count;
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
  decomp->state = _STRDECOMP_STATE_CHARACTER;
  decomp->length = 0;
  decomp->offset = 0;
  decomp->uint_read_symbol_count = 0;
  decomp->uint_total_symbol_count = 0;

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

// returns true in case of EOF
NODISCARD bool _strdecomp_read_character_symbol(string_decompressor *decomp) {
  ftable *table = &decomp->base.main_symbol_table;
  symbol s = acod_decoder_read(decomp->decoder, table);
  ftable_increment(table, s);
  if (s == _STRCOMP_BACKREFERENCE_SYMBOL) {
    decomp->state = _STRDECOMP_STATE_OFFSET | _STRDECOMP_STATE_START;
    return false;
  }

  if (s == _STRCOMP_EOF_SYMBOL) {
    return true;
  }

  writer_write_byte(decomp->writer, (byte)s);
  ring_buffer_push(&decomp->lookback_buffer, (byte)s);

  return false;
}

void _strdecomp_materialize_existing_backreference(string_decompressor *decomp) {
  int64_t start_index = (int64_t)ring_buffer_get_index(&decomp->lookback_buffer) - decomp->offset;
  if (start_index < 0) {
    start_index += (int64_t)ring_buffer_get_length(&decomp->lookback_buffer);
  }
  int64_t length = decomp->length;
  for (int64_t i = 0; i < length; i++) {
    byte b = ring_buffer_get(&decomp->lookback_buffer, start_index + i);
    writer_write_byte(decomp->writer, b);
    ring_buffer_push(&decomp->lookback_buffer, b);
  }
}

void _strdecomp_read_backreference_symbol(string_decompressor *decomp) {
  if (decomp->state & _STRDECOMP_STATE_START) {
    decomp->state = decomp->state & (~_STRDECOMP_STATE_START);
    ftable *length_table = decomp->state & _STRDECOMP_STATE_OFFSET ? &decomp->base.backref_offset_length : &decomp->base.backref_length_length;
    decomp->uint_total_symbol_count = acod_decoder_read(decomp->decoder, length_table);
    ftable_increment(length_table, decomp->uint_total_symbol_count);
    return;
  }

  if (decomp->uint_read_symbol_count < decomp->uint_total_symbol_count) {
    // reading a part of an offset or length uint
    bool is_last_part = decomp->uint_read_symbol_count == decomp->uint_total_symbol_count - 1;
    symbol part;
    if (is_last_part) {
      ftable *last_part_table = decomp->state & _STRDECOMP_STATE_OFFSET ? &decomp->base.backref_offset_last_part : &decomp->base.backref_length_last_part;
      part = acod_decoder_read(decomp->decoder, last_part_table);
      ftable_increment(last_part_table, part);
    } else {
      part = acod_decoder_read(decomp->decoder, &decomp->base.uint_parts);
    }

    int64_t *field = decomp->state & _STRDECOMP_STATE_OFFSET ? &decomp->offset : &decomp->length;
    *field |= (part << (decomp->uint_read_symbol_count * _STRCOMP_UINT_SYMBOL_BITS_COUNT));
    decomp->uint_read_symbol_count++;
    return;
  }

  // we have read through offset or length part of a backreference
  decomp->uint_total_symbol_count = 0;
  decomp->uint_read_symbol_count = 0;
  if (decomp->state & _STRDECOMP_STATE_OFFSET) {
    decomp->state = _STRDECOMP_STATE_LENGTH | _STRDECOMP_STATE_START;
  } else {
    _strdecomp_materialize_existing_backreference(decomp);
    decomp->length = 0;
    decomp->offset = 0;
    decomp->state = _STRDECOMP_STATE_CHARACTER;
  }
}

/** Returns true if pushing this bit resulted in reading EOF symbol */
NODISCARD bool strdecomp_push_bit(string_decompressor *decomp, byte bit) {
  acod_decoder_update(decomp->decoder, bit);

  while (acod_decoder_has_symbol(decomp->decoder)) {
    // reading a symbol
    if (decomp->state == _STRDECOMP_STATE_CHARACTER) {
      if (_strdecomp_read_character_symbol(decomp)) {
        return true;
      }
    } else {
      _strdecomp_read_backreference_symbol(decomp);
    }
  }

  return false;
}
