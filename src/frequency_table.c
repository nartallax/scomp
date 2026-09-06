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
  // this is not very performant
  // but this method will be called once per millions of symbols (with the right settings)
  // so, whatever.
  symbol length = ftree_length(table->frequencies);
  symbol_frequency *source_freqs = ftree_to_source_array(table->frequencies);
  symbol_frequency new_total = 0;
  for (symbol i = 0; i < length; i++) {
    symbol_frequency freq = source_freqs[i];
    if (freq & 1) {
      freq++;
    }
    freq /= 2;
    source_freqs[i] = freq;
    new_total += freq;
  }

  ftree_delete(table->frequencies);
  table->frequencies = ftree_from_values(length, source_freqs, 0);
  free(source_freqs);
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

ftable *ftable_new(symbol length, int init_flags) {
  symbol eof_padding = _ftable_get_eof_length_padding(init_flags);

  ftable *table = malloc(sizeof(ftable));
  table->frequencies = ftree_new(length + eof_padding);
  table->total = 0;
  table->is_eof_included = eof_padding > 0;

  if (table->is_eof_included) {
    ftable_increment(table, length);
  }

  if (_ftable_get_init_value(init_flags)) {
    for (symbol i = 0; i < length; i++) {
      ftable_increment(table, i);
    }
  }

  return table;
}

// Constructs a frequency table from the specified array of symbol frequencies.
ftable *ftable_from_frequencies(symbol length, symbol_frequency *frequencies, int init_flags) {
  symbol_frequency total = 0;
  for (symbol i = 0; i < length; i++) {
    total += frequencies[i];
  }

  symbol eof_padding = _ftable_get_eof_length_padding(init_flags);
  ftable *table = malloc(sizeof(ftable));
  table->frequencies = ftree_from_values(length, frequencies, eof_padding);
  table->total = total;
  table->is_eof_included = eof_padding > 0;

  if (table->is_eof_included) {
    ftable_increment(table, length);
  }

  return table;
}

void ftable_delete(ftable *table) {
  ftree_delete(table->frequencies);
  free(table);
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
  return ftree_sum(table->frequencies, symbol - 1);
}

/** Returns the sum of the frequencies of the specified symbol and all the symbols below. */
symbol_frequency ftable_get_high(ftable *table, symbol symbol) {
  return ftree_sum(table->frequencies, symbol);
}
