#pragma once
#include "decoder.c"
#include "uint_freq_tables.c"

typedef struct {
  uint64 value;
  byte read_symbol_count;
  byte total_symbol_count;
  bool is_expecting_count;
} uint_reader_state;

void uint_reader_reset(uint_reader_state *state) {
  *state = (uint_reader_state){0};
  state->is_expecting_count = true;
}

uint64 uint_reader_consume(uint_reader_state *state) {
  assert(state->read_symbol_count == state->total_symbol_count);
  assert(!state->is_expecting_count);

  uint64 result = state->value;
  uint_reader_reset(state);
  return result;
}

/** Make reader read next symbol from the decoder and update its state.
If it returns true - you should call consume function to get the value and reset the state. */
NODISCARD bool uint_reader_read(uint_reader_state *state, uint_freq_tables *tables, acod_decoder *decoder) {
  if (state->is_expecting_count) {
    state->is_expecting_count = false;
    state->total_symbol_count = acod_decoder_read(decoder, &tables->length_table);
    ftable_increment(&tables->length_table, state->total_symbol_count);
    return state->total_symbol_count == state->read_symbol_count; // this check is here in case of 0
  }

  assert(state->read_symbol_count <= state->total_symbol_count && "Proper usage of reader consumes values after they are read; otherwise, overflow occurs");

  bool is_last_part = state->read_symbol_count == state->total_symbol_count - 1;
  symbol part;
  if (is_last_part) {
    part = acod_decoder_read(decoder, &tables->last_part_table);
    ftable_increment(&tables->last_part_table, part);
  } else {
    part = acod_decoder_read(decoder, &tables->uint_parts_table);
  }

  state->value |= (part << (state->read_symbol_count * _ACOD_UINT_BIT_GROUP_SIZE));
  state->read_symbol_count++;
  return is_last_part;
}