#pragma once
#include "decoder.c"
#include "uint_freq_tables.c"

INLINE uint64 _uint_reader_append_part(uint64 result, symbol part, int64 read_symbol_count) {
  return result | (part << (read_symbol_count * _ACOD_UINT_BIT_GROUP_SIZE));
}

/** Make reader read next symbol from the decoder and update its state.
If it returns true - you should call consume function to get the value and reset the state. */
uint64 uint_reader_read(uint_freq_tables *tables, acod_decoder *decoder) {
  uint64 result = 0;
  int64 total_symbol_count = (int64)acod_decoder_read(decoder, &tables->length_table);
  ftable_increment(&tables->length_table, total_symbol_count);

  symbol part;
  for (int64 i = 0; i < total_symbol_count - 1; i++) {
    part = acod_decoder_read(decoder, &tables->uint_parts_table);
    result = _uint_reader_append_part(result, part, i);
  }

  // last part is special
  if (total_symbol_count > 0) {
    part = acod_decoder_read(decoder, &tables->last_part_table);
    ftable_increment(&tables->last_part_table, part);
    result = _uint_reader_append_part(result, part, total_symbol_count - 1);
  }

  return result;
}