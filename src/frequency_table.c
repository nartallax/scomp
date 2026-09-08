#pragma once
#include "./fenwick_tree.c"
#include "commons.c"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define FTABLE_INCLUDE_EOF (1 << 0)
#define FTABLE_EXCLUDE_EOF (1 << 1)
#define FTABLE_INIT_ZERO (1 << 2)
#define FTABLE_INIT_ONE (1 << 3)

/** Frequency table - a table (aka map) of symbol frequencies (symbol -> symbol_frequency).
Each symbol has a frequency, which is a non-negative integer.
Frequency table objects are primarily used for getting cumulative symbol frequencies. */
typedef struct {
  ftree frequencies;
  // sum of all frequencies
  symbol_frequency total;
  bool is_eof_included;
  // this exists to avoid reallocation on each compaction
  symbol_frequency *compaction_buffer;
} ftable;

symbol_frequency _ftable_get_init_value(int init_flags) {
  return init_flags & FTABLE_INIT_ONE ? 1 : 0;
}

symbol _ftable_get_eof_length_padding(int init_flags) {
  return init_flags & FTABLE_INCLUDE_EOF ? 1 : 0;
}

// Divides all frequencies by half, rounding up.
// This allows to avoid overflows on long sequences, at cost of losing some precision
void _ftable_divide_by_half(ftable *table) {
  symbol length = ftree_length(table->frequencies);
  ftree_to_source_array(table->frequencies, table->compaction_buffer);

  symbol_frequency new_total = 0;
  for (symbol i = 0; i < length; i++) {
    symbol_frequency freq = table->compaction_buffer[i];
    if (freq & 1) {
      freq++;
    }
    freq /= 2;
    table->compaction_buffer[i] = freq;
    new_total += freq;
  }

  ftree_fill_from_source_frequencies(table->frequencies, table->compaction_buffer, 0);
  table->total = new_total;
}

void ftable_halve_until_total_below_limit(ftable *table, symbol limit) {
  while (table->total >= limit) {
    _ftable_divide_by_half(table);
  }
}

void ftable_increment(ftable *table, symbol symbol) {
  ftable_halve_until_total_below_limit(table, UINT64_MAX - 1);

  table->total += 1;
  ftree_add(table->frequencies, symbol, 1);
}

void ftable_delete(ftable *table) {
  ftree_delete(table->frequencies);
  free(table->compaction_buffer);
  free(table);
}

ftable *ftable_new(symbol length, int init_flags) {
  symbol eof_padding = _ftable_get_eof_length_padding(init_flags);

  ftable *table = allocate(1, sizeof(ftable));
  if (!table) {
    return NULL;
  }

  table->frequencies = ftree_new(length + eof_padding);
  if (error_is_present()) {
    ftable_delete(table);
    return NULL;
  }

  table->compaction_buffer = allocate(ftree_length(table->frequencies), sizeof(symbol_frequency));
  if (!table->compaction_buffer) {
    ftable_delete(table);
    return NULL;
  }

  table->total = 0;
  table->is_eof_included = eof_padding > 0;

  if (table->is_eof_included) {
    ftable_increment(table, length);
  }

  if (_ftable_get_init_value(init_flags) == 1) {
    for (symbol i = 0; i < length; i++) {
      ftable_increment(table, i);
    }
  }

  return table;
}

/** Returns the number of symbols in this frequency table. Includes EOF marker, if present. */
symbol _ftable_get_symbol_limit(ftable *table) {
  return ftree_length(table->frequencies);
}

/** Returns the number of symbols that invoking code can use.
Does not include EOF marker, if it is present. */
symbol ftable_get_symbol_count(ftable *table) {
  if (!table->is_eof_included) {
    return _ftable_get_symbol_limit(table);
  }
  return _ftable_get_symbol_limit(table) - 1;
}

/** Fills frequency table with provided array of frequencies.
Frequencies are expected to be array of values excluding EOF, even if the EOF flag is passed */
void ftable_fill_from_frequencies(ftable *table, symbol_frequency *frequencies) {
  symbol length = ftable_get_symbol_count(table);
  ftree_fill_from_source_frequencies(table->frequencies, frequencies, table->is_eof_included ? 1 : 0);
  symbol_frequency total = 0;
  for (symbol i = 0; i < length; i++) {
    total += frequencies[i];
  }
  table->total = total;

  if (table->is_eof_included) {
    ftable_increment(table, length);
  }
}

/** For the tables that include eof marker, returns eof symbol.
Meaningless to call for tables without eof marker. */
symbol ftable_get_eof_symbol(ftable *table) {
  return _ftable_get_symbol_limit(table) - 1;
}

symbol_frequency ftable_get_frequency(ftable *table, symbol symbol) {
  return ftree_get(table->frequencies, symbol);
}

/** Returns the sum of the frequencies of all the symbols strictly below the specified symbol value. */
symbol_frequency ftable_get_low(ftable *table, symbol symbol) {
  if (symbol == 0) {
    // frequency range always starts at 0, and this avoids overflow
    return 0;
  }
  return ftree_sum(table->frequencies, symbol - 1);
}

/** Returns the sum of the frequencies of the specified symbol and all the symbols below. */
symbol_frequency ftable_get_high(ftable *table, symbol symbol) {
  return ftree_sum(table->frequencies, symbol);
}
