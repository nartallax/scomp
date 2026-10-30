#pragma once
#include "../commons.c"
#include "../data_structures/reader.c"
#include "arithmetic_coding_base.c"
#include "constants.c"
#include "frequency_table.c"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  _acod_state state;
  // The current raw code bits being buffered, which is always in the range [low, high].
  symbol_frequency code;
  reader *reader;
  int32 base_bits_received;
  int32 trailing_zeroes_emitted;
} acod_decoder;

void acod_decoder_init(acod_decoder *decoder, reader *reader) {
  *decoder = (acod_decoder){0};

  _acod_state_init(&decoder->state);
  decoder->code = 0;
  decoder->base_bits_received = 0;
  decoder->reader = reader;
}

/** Returns a symbol from internal state.
Only makes sense to call when `acod_decoder_has_symbol(decoder) == true` */
symbol _acod_decoder_produce_symbol(acod_decoder *decoder, ftable *frequencies) {
  assert(decoder->state.low <= decoder->code);
  assert(decoder->code <= decoder->state.high);

  symbol_frequency total = frequencies->total;
  symbol_frequency value_range = decoder->state.high - decoder->state.low + 1;
  symbol_frequency offset = decoder->code - decoder->state.low;

  // this avoids overflow
  // __uint128_t numerator = ((__uint128_t)offset + 1) * total - 1;
  // symbol_frequency value = (symbol_frequency)(numerator / value_range);
  symbol_frequency value = (((offset + 1) * total) - 1) / value_range;

  // A kind of binary search. Find highest symbol such that freqs.getLow(symbol) <= value.
  symbol start = 0;
  symbol end = _ftable_get_symbol_limit(frequencies);
  while (end - start > 1) {
    symbol middle = (start + end) >> 1;
    if (ftable_get_low(frequencies, middle) > value) {
      end = middle;
    } else {
      start = middle;
    }
  }

  symbol symbol = start;
  _acod_shrink_the_range(&decoder->state, frequencies, symbol);
  return symbol;
}

INLINE byte _acod_decoder_read_bit(acod_decoder *decoder) {
  if (reader_get_bits_available(decoder->reader) > 0) {
    return reader_read_bit(decoder->reader);
  } else {
    if (decoder->trailing_zeroes_emitted > ACOD_STATE_SIZE_BITS) {
      reader_set_underflow_error(decoder->reader);
    }
    decoder->trailing_zeroes_emitted++;
    return 0;
  }
}

symbol acod_decoder_read(acod_decoder *decoder, ftable *frequencies) {
  if (decoder->base_bits_received < ACOD_STATE_SIZE_BITS) {
    do {
      decoder->code = (decoder->code << 1) | _acod_decoder_read_bit(decoder);
      decoder->base_bits_received++;
    } while (decoder->base_bits_received < ACOD_STATE_SIZE_BITS);
    return _acod_decoder_produce_symbol(decoder, frequencies);
  }

  while (_acod_is_shiftable(&decoder->state)) {
    decoder->code = ((decoder->code << 1) & ACOD_STATE_MASK) | _acod_decoder_read_bit(decoder);
    _acod_perform_shift(&decoder->state);
  }

  while (_acod_is_underflowable(&decoder->state)) {
    decoder->code = (decoder->code & ACOD_HALF_RANGE) | ((decoder->code << 1) & (ACOD_STATE_MASK >> 1)) | _acod_decoder_read_bit(decoder);
    _acod_perform_underflow(&decoder->state);
  }

  return _acod_decoder_produce_symbol(decoder, frequencies);
}