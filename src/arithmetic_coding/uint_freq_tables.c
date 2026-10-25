#pragma once
#include "../math.c"
#include "frequency_table.c"

constexpr int64 _ACOD_UINT_BIT_GROUP_SIZE = 4; // one symbol, which is part of uint, contains this much bits of information
constexpr int64 _ACOD_UINT_SYMBOL_MASK = (1 << _ACOD_UINT_BIT_GROUP_SIZE) - 1;

typedef struct {
  // length_table is used when writing amount of groups of bits.
  // uint is broken into groups of bits; each group is used as a symbol
  ftable length_table;
  // this table is used when writing all groups of bits of offset or length but last
  // it's always stays at 1; when we are using this table, probability of each symbol is equal,
  // so there's no reason to increment it.
  ftable uint_parts_table;
  // last_part_table tables are used when writing last group of bits.
  // this table is incremented after use;
  // last group of bits is the most significant, and is much more likely to be skewed
  // (because uints are usually not equally distributed - smaller values are much more likely)
  ftable last_part_table;
} uint_freq_tables;

void uint_freq_tables_deinit(uint_freq_tables *tables, context *context) {
  ftable_deinit(&tables->uint_parts_table, context);
  ftable_deinit(&tables->last_part_table, context);
  ftable_deinit(&tables->length_table, context);

  *tables = (uint_freq_tables){0};
}

NODISCARD bool uint_freq_tables_init(uint_freq_tables *tables, context *context, uint64 max_value) {
  *tables = (uint_freq_tables){0};

  if (!ftable_init(&tables->uint_parts_table, context, 1 << _ACOD_UINT_BIT_GROUP_SIZE, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
    uint_freq_tables_deinit(tables, context);
    return false;
  }

  if (!ftable_init(&tables->last_part_table, context, 1 << _ACOD_UINT_BIT_GROUP_SIZE, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
    uint_freq_tables_deinit(tables, context);
    return false;
  }

  int32 max_value_bits = most_significant_bit_index(max_value);
  int32 symbol_count = (max_value_bits + (_ACOD_UINT_BIT_GROUP_SIZE - 1)) / _ACOD_UINT_BIT_GROUP_SIZE;
  if (!ftable_init(&tables->length_table, context, symbol_count + 1, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
    uint_freq_tables_deinit(tables, context);
    return false;
  }

  return true;
}