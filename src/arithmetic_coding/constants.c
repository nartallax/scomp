#pragma once
#include "../commons.c"

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