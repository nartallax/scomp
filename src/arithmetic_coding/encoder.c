#pragma once
#include "../commons.c"
#include "../data_structures/writer.c"
#include "arithmetic_coding_base.c"
#include "constants.c"
#include "frequency_table.c"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  _acod_state state;
  // Number of saved underflow bits. This value can grow without bound, so a truly correct implementation would use a BigInteger.
  // (of course, it's very unlikely and can potentially happen only to very, very long sequences of input symbols)
  uint64 underflows;
  writer *writer;
} acod_encoder;

void acod_encoder_init(acod_encoder *encoder, writer *writer) {
  *encoder = (acod_encoder){0};

  _acod_state_init(&encoder->state);
  encoder->underflows = 0;
  encoder->writer = writer;
}

void _acod_encoder_write_shift_bit(acod_encoder *encoder, byte bit) {
  writer_write_bit(encoder->writer, bit);

  // Write out the saved underflow bits
  while (encoder->underflows > 0) {
    writer_write_bit(encoder->writer, bit ^ 1);
    encoder->underflows--;
  }
}

void _acod_encoder_finalize(acod_encoder *encoder) {
  // This makes the final interval unambiguous
  encoder->underflows++;

  byte final_bit = encoder->state.low < ACOD_QUARTER_RANGE ? 0 : 1;
  _acod_encoder_write_shift_bit(encoder, final_bit);
}

/** Flush remaining state, and delete the encoder.
Must be called before deleting underlying writer.
Returns true if finalized successfully. */
void acod_encoder_deinit(acod_encoder *encoder) {
  _acod_encoder_finalize(encoder);

  *encoder = (acod_encoder){0};
}

// TODO: consider having acod_encoder_write_and_increment, and corresponding method on decoding side
/** Writes a single symbol with the encoder.
Returns true if the write was successful. */
void acod_encoder_write(acod_encoder *encoder, ftable *frequencies, symbol symbol) {
  _acod_shrink_the_range(&encoder->state, frequencies, symbol);

  while (_acod_is_shiftable(&encoder->state)) {
    byte bit = encoder->state.low >> (ACOD_STATE_SIZE_BITS - 1);
    _acod_encoder_write_shift_bit(encoder, bit);
    _acod_perform_shift(&encoder->state);
  }

  while (_acod_is_underflowable(&encoder->state)) {
    encoder->underflows++;
    _acod_perform_underflow(&encoder->state);
  }
}
