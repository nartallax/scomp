#pragma once
#include "../commons.c"
#include "constants.c"
#include "frequency_table.c"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  ACOD_STAGE_SHIFT = 1,
  ACOD_STAGE_UNDERFLOW,
  ACOD_STAGE_READY,
  ACOD_STAGE_PREPARATION
} _acod_stage;

typedef struct {
  symbol_frequency high;
  symbol_frequency low;
  _acod_stage stage;
} _acod_state;

void _acod_state_init(_acod_state *state) {
  *state = (_acod_state){0};

  state->low = 0;
  state->high = ACOD_STATE_MASK;
  state->stage = ACOD_STAGE_READY;
}

bool _acod_is_shiftable(_acod_state *state) {
  return ((state->low ^ state->high) & ACOD_HALF_RANGE) == 0;
}

bool _acod_is_underflowable(_acod_state *state) {
  return (state->low & (~state->high) & ACOD_QUARTER_RANGE) != 0;
}

void _acod_try_progress_stage(_acod_state *state) {
  while (true) {
    switch (state->stage) {
    case ACOD_STAGE_READY:
      state->stage = ACOD_STAGE_SHIFT;
      break;
    case ACOD_STAGE_SHIFT:
      if (_acod_is_shiftable(state)) {
        return;
      }
      state->stage = ACOD_STAGE_UNDERFLOW;
      break;
    default: // underflow
      if (_acod_is_underflowable(state)) {
        return;
      }
      state->stage = ACOD_STAGE_READY;
      return;
    }
  }
}

void _acod_perform_update_start(_acod_state *state, ftable *frequencies, symbol symbol) {
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

void _acod_perform_shift(_acod_state *state) {
  assert(_acod_is_shiftable(state));
  // While low and high have the same top bit value, shift them out
  state->low = ((state->low << 1) & ACOD_STATE_MASK);
  state->high = ((state->high << 1) & ACOD_STATE_MASK) | 1;
  assert(state->low < state->high);
  _acod_try_progress_stage(state);
}

void _acod_perform_underflow(_acod_state *state) {
  assert(_acod_is_underflowable(state));
  // While low's top two bits are 01 and high's are 10, delete the second highest bit of both
  state->low = (state->low << 1) ^ ACOD_HALF_RANGE;
  state->high = ((state->high ^ ACOD_HALF_RANGE) << 1) | ACOD_HALF_RANGE | 1;
  assert(state->low < state->high);
  _acod_try_progress_stage(state);
}