#pragma once
#include "../arithmetic_coding/decoder.c"
#include "../commons.c"
#include "../writer.c"
#include "base.c"
#include <stdbool.h>

typedef enum {
  _STRCOMP_STATE_SYMBOL,
  _STRCOMP_STATE_OFFSET,
  _STRCOMP_STATE_LENGTH
} _strcomp_state;

typedef enum {
  STRCOMP_OK = 1,
  STRCOMP_ERROR,
  STRCOMP_EOF
} strdecomp_push_result;

typedef struct {
  string_compression_base base;
  writer *writer;
  acod_decoder *decoder;
  _strcomp_state state;
  uint64_t length;
  uint64_t offset;
  byte shift;
} string_decompressor;

NODISCARD bool strdecomp_init(string_decompressor *decomp, context *context, writer *writer, acod_decoder *decoder) {
  *decomp = (string_decompressor){0};

  decomp->writer = writer;
  decomp->decoder = decoder;
  decomp->state = _STRCOMP_STATE_SYMBOL;
  decomp->length = 0;
  decomp->offset = 0;
  decomp->shift = 0;
  return _strcomp_init_base(&decomp->base, context);
}

void strdecomp_deinit(string_decompressor *decomp, context *context) {
  _strcomp_deinit_base(&decomp->base, context);
}

void _strdecomp_update_uint(string_decompressor *decomp, symbol bit) {
  uint64_t *field = decomp->state == _STRCOMP_STATE_OFFSET ? &decomp->offset : &decomp->length;
  *field |= (bit << decomp->shift);
  decomp->shift++;
}

bool _strdecomp_output_byte(string_decompressor *decomp, byte b) {
  if (!writer_write_byte(decomp->writer, b)) {
    return false;
  }
  ring_buffer_push(&decomp->base.buffer, b);
  return true;
}

strdecomp_push_result strdecomp_push_bit(string_decompressor *decomp, byte bit) {
  acod_decoder_update(decomp->decoder, bit);
  while (acod_decoder_has_symbol(decomp->decoder)) {

    // reading a symbol
    if (decomp->state == _STRCOMP_STATE_SYMBOL) {
      symbol s = acod_decoder_read(decomp->decoder, &decomp->base.main_symbol_table);
      if (s == _STRCOMP_BACKREFERENCE_SYMBOL) {
        decomp->state = _STRCOMP_STATE_OFFSET;
        continue;
      }

      if (s == _STRCOMP_EOF_SYMBOL) {
        return STRCOMP_EOF;
      }

      if (!_strdecomp_output_byte(decomp, (byte)s)) {
        return STRCOMP_ERROR;
      }

      continue;
    }

    // reading a backreference
    symbol s = acod_decoder_read(decomp->decoder, &decomp->base.raw_bit_with_terminator_table);
    if (s != _STRCOMP_BIT_TERMINATOR_SYMBOL) {
      _strdecomp_update_uint(decomp, s);
      continue;
    }

    if (decomp->state == _STRCOMP_STATE_OFFSET) {
      decomp->state = _STRCOMP_STATE_LENGTH;
      decomp->shift = 0;
      continue;
    }

    decomp->state = _STRCOMP_STATE_SYMBOL;
    int64_t start_index = (int64_t)ring_buffer_get_index(&decomp->base.buffer) - (int64_t)decomp->offset;
    if (start_index < 0) {
      start_index += ring_buffer_get_length(&decomp->base.buffer);
    }
    int64_t length = decomp->length;
    decomp->length = 0;
    decomp->offset = 0;
    decomp->shift = 0;
    for (int64_t i = 0; i < length; i++) {
      byte b = ring_buffer_get(&decomp->base.buffer, start_index + i);
      if (!_strdecomp_output_byte(decomp, (byte)s)) {
        return STRCOMP_ERROR;
      }
    }
  }

  return STRCOMP_OK;
}
