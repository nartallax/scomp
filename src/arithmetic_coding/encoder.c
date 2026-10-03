#pragma once
#include "../commons.c"
#include "../writer.c"
#include "base.c"
#include "constants.c"
#include "frequency_table.c"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  _acod_state state;
  // Number of saved underflow bits. This value can grow without bound, so a truly correct implementation would use a BigInteger.
  // (of course, it's very unlikely and can potentially happen only to very, very long sequences of input symbols)
  uint64_t underflows;
  writer *writer;
} acod_encoder;

void acod_encoder_init(acod_encoder *encoder, writer *writer) {
  *encoder = (acod_encoder){0};

  _acod_state_init(&encoder->state);
  encoder->underflows = 0;
  encoder->writer = writer;
}

NODISCARD bool _acod_encoder_write_shift_bit(acod_encoder *encoder, byte bit) {
  if (!writer_write_bit(encoder->writer, bit)) {
    return false;
  }

  // Write out the saved underflow bits
  while (encoder->underflows > 0) {
    if (!writer_write_bit(encoder->writer, bit ^ 1)) {
      return false;
    }
    encoder->underflows--;
  }

  return true;
}

NODISCARD bool _acod_encoder_finalize(acod_encoder *encoder) {
  // This makes the final interval unambiguous.
  encoder->underflows++;

  byte final_bit = encoder->state.low < ACOD_QUARTER_RANGE ? 0 : 1;
  return _acod_encoder_write_shift_bit(encoder, final_bit);
}

/** Flush remaining state, and delete the encoder.
Must be called before deleting underlying writer.
Returns true if finalized successfully. */
NODISCARD bool acod_encoder_deinit(acod_encoder *encoder, context *context) {
  if (context_is_errored(context)) {
    // if context is errored - we must not attempt to write more stuff into the writer
    // as it may be in the broken state
    return false;
  }
  return _acod_encoder_finalize(encoder);
}

/** Writes a single symbol with the encoder.
Returns true if the write was successful. */
NODISCARD bool acod_encoder_write(acod_encoder *encoder, ftable *frequencies, symbol symbol) {
  _acod_perform_update_start(&encoder->state, frequencies, symbol);

  while (encoder->state.stage == ACOD_STAGE_SHIFT) {
    byte bit = encoder->state.low >> (ACOD_STATE_SIZE_BITS - 1);
    if (!_acod_encoder_write_shift_bit(encoder, bit)) {
      return false;
    }
    _acod_perform_shift(&encoder->state);
  }

  while (encoder->state.stage == ACOD_STAGE_UNDERFLOW) {
    encoder->underflows++;
    _acod_perform_underflow(&encoder->state);
  }

  return true;
}
