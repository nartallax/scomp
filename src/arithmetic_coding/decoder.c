#pragma once
#include "../commons.c"
#include "base.c"
#include "constants.c"
#include "frequency_table.c"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  _acod_state state;
  // The current raw code bits being buffered, which is always in the range [low, high].
  symbol_frequency code;
  int base_bits_received;
} acod_decoder;

void acod_decoder_init(acod_decoder *decoder) {
  *decoder = (acod_decoder){0};

  _acod_state_init(&decoder->state);
  decoder->state.stage = ACOD_STAGE_PREPARATION;
  decoder->code = 0;
  decoder->base_bits_received = 0;
}

/** When a bit is known (received from some reader) - update internal state of the decoder with that bit.
This may result in symbols being extracted from internal state. After this call, make sure to call `acod_decoder_read()` repeatedly while `acod_decoder_has_symbol()`. */
void acod_decoder_update(acod_decoder *decoder, byte bit) {
  assert((bit == 0 || bit == 1) && "Bit must be 0 or 1");

  if (decoder->state.stage == ACOD_STAGE_PREPARATION) {
    decoder->code = (decoder->code << 1) | bit;
    decoder->base_bits_received++;
    if (decoder->base_bits_received >= ACOD_STATE_SIZE_BITS) {
      decoder->state.stage = ACOD_STAGE_READY;
    }
    return;
  }

  assert(decoder->state.stage != ACOD_STAGE_READY && "Decoder state was not properly drained by reads");

  if (decoder->state.stage == ACOD_STAGE_SHIFT) {
    decoder->code = ((decoder->code << 1) & ACOD_STATE_MASK) | bit;
    _acod_perform_shift(&decoder->state);
  } else { // underflow
    decoder->code = (decoder->code & ACOD_HALF_RANGE) | ((decoder->code << 1) & (ACOD_STATE_MASK >> 1)) | bit;
    _acod_perform_underflow(&decoder->state);
  }
}

bool acod_decoder_has_symbol(acod_decoder *decoder) {
  return decoder->state.stage == ACOD_STAGE_READY;
}

/** Returns a symbol from internal state.
Only makes sense to call when `acod_decoder_has_symbol(decoder) == true` */
symbol acod_decoder_read(acod_decoder *decoder, ftable *frequencies) {
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
  _acod_perform_update_start(&decoder->state, frequencies, symbol);
  return symbol;
}