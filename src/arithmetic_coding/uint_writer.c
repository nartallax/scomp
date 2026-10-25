#pragma once
#include "encoder.c"
#include "uint_freq_tables.c"

void uint_writer_write(uint_freq_tables *tables, acod_encoder *encoder, uint64 value) {
  int64 bit_index = most_significant_bit_index(value);
  symbol length_symbols_symbol = (bit_index + _ACOD_UINT_BIT_GROUP_SIZE) / _ACOD_UINT_BIT_GROUP_SIZE;
  acod_encoder_write(encoder, &tables->length_table, length_symbols_symbol);
  ftable_increment(&tables->length_table, length_symbols_symbol);

  for (symbol i = 0; i < length_symbols_symbol; i++) {
    byte part = value & _ACOD_UINT_SYMBOL_MASK;
    value = value >> _ACOD_UINT_BIT_GROUP_SIZE;
    bool is_last_part = i == (length_symbols_symbol - 1);
    if (is_last_part) {
      acod_encoder_write(encoder, &tables->last_part_table, part);
      ftable_increment(&tables->last_part_table, part);
    } else {
      acod_encoder_write(encoder, &tables->uint_parts_table, part);
    }
  }
}