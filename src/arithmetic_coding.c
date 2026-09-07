#pragma once
#include "./commons.c"
#include "./frequency_table.c"
#include "./writer.c"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// size of arithmetic coding state, [2, 62]
// this is tweakable, but in my experiments it didn't ever improve the outcome
// extreme values (close to 2 or 62) can make compression worse, and/or degrade speed, and also may cause overflows
// midline value 32 works great for anything I tested with
const int ACOD_STATE_SIZE_BITS = 32;
const symbol_frequency ACOD_FULL_RANGE = 1L << ACOD_STATE_SIZE_BITS;
// non-zero
const symbol_frequency ACOD_HALF_RANGE = ACOD_FULL_RANGE >> 1;
// can be zero
const symbol_frequency ACOD_QUARTER_RANGE = ACOD_HALF_RANGE >> 1;
// at least 2
const symbol_frequency ACOD_MIN_RANGE = ACOD_QUARTER_RANGE + 2L;
const symbol_frequency ACOD_MAX_TOTAL_UNCAPPED = UINT64_MAX / ACOD_FULL_RANGE;
const symbol_frequency ACOD_MAX_TOTAL = ACOD_MAX_TOTAL_UNCAPPED > ACOD_MIN_RANGE ? ACOD_MIN_RANGE : ACOD_MAX_TOTAL_UNCAPPED;
const symbol_frequency ACOD_STATE_MASK = ACOD_FULL_RANGE - 1L;

const int ACOD_STAGE_SHIFT = 1;
const int ACOD_STAGE_UNDERFLOW = 2;
const int ACOD_STAGE_READY = 3;
const int ACOD_STAGE_PREPARATION = 4;

typedef struct {
  symbol_frequency high;
  symbol_frequency low;
  int stage;
} acod_state;

acod_state _acod_state_new() {
  acod_state state;
  state.low = 0;
  state.high = ACOD_STATE_MASK;
  state.stage = ACOD_STAGE_READY;
  return state;
}

bool _acod_is_shiftable(acod_state *state) {
  return ((state->low ^ state->high) & ACOD_HALF_RANGE) == 0;
}

bool _acod_is_underflowable(acod_state *state) {
  return (state->low & (~state->high) & ACOD_QUARTER_RANGE) != 0;
}

void _acod_try_progress_stage(acod_state *state) {
  while (true) {
    if (state->stage == ACOD_STAGE_READY) {
      state->stage = ACOD_STAGE_SHIFT;
    } else if (state->stage == ACOD_STAGE_SHIFT) {
      if (_acod_is_shiftable(state)) {
        return;
      }
      state->stage = ACOD_STAGE_UNDERFLOW;
    } else { // underflow
      if (_acod_is_underflowable(state)) {
        return;
      }
      state->stage = ACOD_STAGE_READY;
      return;
    }
  }
}

void _acod_perform_update_start(acod_state *state, ftable *frequencies, symbol symbol) {
  assert(state->stage == ACOD_STAGE_READY);

  symbol_frequency value_range = state->high - state->low + 1;
  symbol_frequency total = frequencies->total;
  symbol_frequency symbol_low = ftable_get_low(frequencies, symbol);
  symbol_frequency symbol_high = ftable_get_high(frequencies, symbol);

  // __uint128_t new_low_numerator = ((__uint128_t)symbol_low) * ((__uint128_t)value_range);
  // symbol_frequency new_low = state->low + (symbol_frequency)(new_low_numerator / ((__uint128_t)(total)));
  symbol_frequency new_low = state->low + ((symbol_low * value_range) / total);

  // __uint128_t new_high_numerator = ((__uint128_t)symbol_high) * ((__uint128_t)value_range);
  // symbol_frequency new_high = state->low + (symbol_frequency)(new_high_numerator / ((__uint128_t)(total))) - 1;
  symbol_frequency new_high = state->low + ((symbol_high * value_range) / total) - 1;

  state->low = new_low;
  state->high = new_high;
  assert(state->low < state->high);

  _acod_try_progress_stage(state);
}

void _acod_perform_shift(acod_state *state) {
  assert(_acod_is_shiftable(state));
  // While low and high have the same top bit value, shift them out
  state->low = ((state->low << 1) & ACOD_STATE_MASK);
  state->high = ((state->high << 1) & ACOD_STATE_MASK) | 1;
  assert(state->low < state->high);
  _acod_try_progress_stage(state);
}

void _acod_perform_underflow(acod_state *state) {
  assert(_acod_is_underflowable(state));
  // While low's top two bits are 01 and high's are 10, delete the second highest bit of both
  state->low = (state->low << 1) ^ ACOD_HALF_RANGE;
  state->high = ((state->high ^ ACOD_HALF_RANGE) << 1) | ACOD_HALF_RANGE | 1;
  assert(state->low < state->high);
  _acod_try_progress_stage(state);
}

typedef struct {
  acod_state state;
  // Number of saved underflow bits. This value can grow without bound, so a truly correct implementation would use a BigInteger.
  // (of course, it's very unlikely and can potentially happen only to very, very long sequences of input symbols)
  uint64_t underflows;
  writer *writer;
} acod_encoder;

acod_encoder *acod_encoder_new(writer *writer) {
  acod_encoder *encoder = malloc(sizeof(acod_encoder));
  encoder->state = _acod_state_new();
  encoder->underflows = 0;
  encoder->writer = writer;
  return encoder;
}

void _acod_encoder_finalize(acod_encoder *encoder) {
  // This makes the final interval unambiguous.
  encoder->underflows++;

  byte final_bit = encoder->state.low < ACOD_QUARTER_RANGE ? 0 : 1;
  writer_write_bit(encoder->writer, final_bit);

  while (encoder->underflows > 0) {
    writer_write_bit(encoder->writer, final_bit ^ 1);
    encoder->underflows--;
  }
}

/** Flush remaining state, and delete the encoder.
Must be called before deleting underlying writer. */
void acod_encoder_delete(acod_encoder *encoder) {
  _acod_encoder_finalize(encoder);
  free(encoder);
}

void acod_encoder_write(acod_encoder *encoder, ftable *frequencies, symbol symbol) {
  // TODO: consider moving those halvings outside of the encoder and decoder
  // encoder/decoder never modify frequency tables, and therefore should never trigger halvings
  // and also we need a test for this halving behavior
  ftable_halve_until_total_below_limit(frequencies, ACOD_MAX_TOTAL);
  _acod_perform_update_start(&encoder->state, frequencies, symbol);

  while (encoder->state.stage == ACOD_STAGE_SHIFT) {
    byte bit = encoder->state.low >> (ACOD_STATE_SIZE_BITS - 1);
    writer_write_bit(encoder->writer, bit);

    // Write out the saved underflow bits
    while (encoder->underflows > 0) {
      writer_write_bit(encoder->writer, bit ^ 1);
      encoder->underflows--;
    }

    _acod_perform_shift(&encoder->state);
  }

  while (encoder->state.stage == ACOD_STAGE_UNDERFLOW) {
    encoder->underflows++;
    _acod_perform_underflow(&encoder->state);
  }
}

typedef struct {
  acod_state state;
  // The current raw code bits being buffered, which is always in the range [low, high].
  symbol_frequency code;
  int base_bits_received;
} acod_decoder;

acod_decoder *acod_decoder_new() {
  acod_decoder *decoder = malloc(sizeof(acod_decoder));
  decoder->state = _acod_state_new();
  decoder->state.stage = ACOD_STAGE_PREPARATION;
  decoder->code = 0;
  decoder->base_bits_received = 0;
  return decoder;
}

void acod_decoder_delete(acod_decoder *decoder) {
  free(decoder);
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
  // TODO: as in encoder - move it out
  ftable_halve_until_total_below_limit(frequencies, ACOD_MAX_TOTAL);

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