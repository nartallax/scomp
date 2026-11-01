#pragma once
#include "../arithmetic_coding/frequency_table.c"
#include "../commons.c"
#include "../data_structures/reader.c"
#include "../data_structures/writer.c"

// see also: https://github.com/JentGent/tANS/blob/main/ans.ipynb

// TODO: tune
constexpr int64 ANS_BLOCK_SIZE = 16;

typedef struct {
  writer *writer;
} ans_encoder;

void ans_encoder_init(ans_encoder *encoder, writer *writer) {
  *encoder = (ans_encoder){0};

  encoder->writer = writer;
}

uint64 _ans_calc_next_state(symbol s, uint64 state, ftable *table) {
  symbol_frequency freq = ftable_get_frequency(table, s);
  symbol_frequency low = ftable_get_low(table, s);
  symbol_frequency total = ftable_get_total(table);

  assert(freq > 0);

  uint64 next_state = ((state / freq) * total) + (state % freq) + low;
  return next_state;
}

uint64 _ans_calc_prev_state(symbol s, uint64 state, ftable *table) {
  symbol_frequency freq = ftable_get_frequency(table, s);
  symbol_frequency low = ftable_get_low(table, s);
  symbol_frequency total = ftable_get_total(table);

  assert(total > 0);

  uint64 prev_state = freq * (state / total) + ((state % total) - low);
  return prev_state;
}

uint64 _ans_find_block_initial_state(ftable *table, symbol last_symbol) {
  symbol_frequency total = ftable_get_total(table);
  for (uint64 state = 0; state < total; state++) {
    if (_ans_calc_next_state(last_symbol, state, table) >= total) {
      return state;
    }
  }
  assert(false && "Failed to find initial state for the block");
  return total; // no idea what do to here, tbd
}

uint64 ans_encoder_write_block(ans_encoder *encoder, const symbol *block_symbols, int64 block_length, ftable *table, bool is_incrementing_ftable) {
  assert(block_length > 0 && "No zero-length blocks allowed");

  symbol_frequency total = ftable_get_total(table);
  // uint64 state = _ans_find_block_initial_state(table, block_symbols[block_length - 1]);
  uint64 state = total;
  for (int64 i = block_length - 1; i >= 0; i--) {
    symbol s = block_symbols[i];
    uint64 normalized_state = state;
    uint64 predicted = _ans_calc_next_state(s, normalized_state, table);
    while ((predicted >> 1) >= total) {
      writer_write_bit(encoder->writer, normalized_state & 1);
      normalized_state >>= 1;
      predicted = _ans_calc_next_state(s, normalized_state, table);
    }
    state = predicted;
    if (is_incrementing_ftable) {
      ftable_increment(table, s);
    }
  }
  return state;
}

typedef struct {
  reader *reader;
  uint64 state;
} ans_decoder;

void ans_decoder_init(ans_decoder *decoder, reader *reader) {
  *decoder = (ans_decoder){0};

  decoder->reader = reader;
  decoder->state = 0;
}

symbol _ans_find_symbol_by_offset(ftable *frequencies, uint64 offset) {
  symbol start = 0;
  symbol end = _ftable_get_symbol_limit(frequencies);
  while (end - start > 1) {
    symbol middle = (start + end) >> 1;
    if (ftable_get_low(frequencies, middle) > offset) {
      end = middle;
    } else {
      start = middle;
    }
  }
  assert(ftable_get_frequency(frequencies, start) > 0);
  assert(ftable_get_low(frequencies, start) <= offset && offset < ftable_get_low(frequencies, start) + ftable_get_frequency(frequencies, start));
  return start;
}

symbol ans_decoder_read(ans_decoder *decoder, ftable *table) {
  uint64 total = ftable_get_total(table);
  uint64 offset = decoder->state % total;
  symbol s = _ans_find_symbol_by_offset(table, offset);

  decoder->state = _ans_calc_prev_state(s, decoder->state, table);
  while (decoder->state < total) {
    byte bit = reader_read_bit(decoder->reader);
    uint64 updated_state = (decoder->state << 1) | bit;
    decoder->state = updated_state;
  }
  return s;
}
