#pragma once
#include "encoder.c"
#include "uint_writer_base.c"

typedef struct {
  acod_encoder *encoder;
  uint_writer_base base;
} uint_writer;

void uint_writer_deinit(uint_writer *writer, context *context) {
  _uint_writer_base_deinit(&writer->base, context);

  *writer = (uint_writer){0};
}

NODISCARD bool uint_writer_init(uint_writer *writer, context *context, acod_encoder *encoder, uint64 max_value) {
  *writer = (uint_writer){0};

  writer->encoder = encoder;

  if (!_uint_writer_base_init(&writer->base, context, max_value)) {
    uint_writer_deinit(writer, context);
    return false;
  }

  return true;
}

void uint_writer_write(uint_writer *writer, uint64 value) {
  int64 bit_index = most_significant_bit_index(value);
  symbol length_symbols_symbol = (bit_index + _ACOD_UINT_BIT_GROUP_SIZE) / _ACOD_UINT_BIT_GROUP_SIZE;
  acod_encoder_write(writer->encoder, &writer->base.length_table, length_symbols_symbol);
  ftable_increment(&writer->base.length_table, length_symbols_symbol);

  for (symbol i = 0; i < length_symbols_symbol; i++) {
    byte part = value & _ACOD_UINT_SYMBOL_MASK;
    value = value >> _ACOD_UINT_BIT_GROUP_SIZE;
    bool is_last_part = i == (length_symbols_symbol - 1);
    if (is_last_part) {
      acod_encoder_write(writer->encoder, &writer->base.last_part_table, part);
      ftable_increment(&writer->base.last_part_table, part);
    } else {
      acod_encoder_write(writer->encoder, &writer->base.uint_parts_table, part);
    }
  }
}