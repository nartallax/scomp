#pragma once
#include "decoder.c"
#include "uint_writer_base.c"

typedef struct {
  acod_decoder *decoder;
  uint_writer_base base;
  uint64 value;
  byte read_symbol_count;
  byte total_symbol_count;
  bool is_expecting_count;
} uint_reader;

void uint_reader_deinit(uint_reader *reader, context *context) {
  _uint_writer_base_deinit(&reader->base, context);

  *reader = (uint_reader){0};
}

void uint_reader_reset(uint_reader *reader) {
  reader->value = 0;
  reader->read_symbol_count = 0;
  reader->total_symbol_count = 0;
  reader->is_expecting_count = true;
}

NODISCARD bool uint_reader_init(uint_reader *reader, context *context, acod_decoder *decoder, uint64 max_value) {
  *reader = (uint_reader){0};

  reader->decoder = decoder;
  uint_reader_reset(reader);

  if (!_uint_writer_base_init(&reader->base, context, max_value)) {
    uint_reader_deinit(reader, context);
    return false;
  }

  return true;
}

uint64 uint_reader_consume(uint_reader *reader) {
  assert(reader->read_symbol_count == reader->total_symbol_count);
  assert(!reader->is_expecting_count);

  uint64 result = reader->value;
  uint_reader_reset(reader);
  return result;
}

/** Make reader read next symbol from the decoder and update its state.
If it returns true - you should call consume function to get the value and reset the state. */
NODISCARD bool uint_reader_read(uint_reader *reader) {
  if (reader->is_expecting_count) {
    reader->is_expecting_count = false;
    reader->total_symbol_count = acod_decoder_read(reader->decoder, &reader->base.length_table);
    ftable_increment(&reader->base.length_table, reader->total_symbol_count);
    return reader->total_symbol_count == reader->read_symbol_count; // this check is here in case of 0
  }

  assert(reader->read_symbol_count <= reader->total_symbol_count && "Proper usage of reader consumes values after they are read; otherwise, overflow occurs");

  bool is_last_part = reader->read_symbol_count == reader->total_symbol_count - 1;
  symbol part;
  if (is_last_part) {
    part = acod_decoder_read(reader->decoder, &reader->base.last_part_table);
    ftable_increment(&reader->base.last_part_table, part);
  } else {
    part = acod_decoder_read(reader->decoder, &reader->base.uint_parts_table);
  }

  reader->value |= (part << (reader->read_symbol_count * _ACOD_UINT_BIT_GROUP_SIZE));
  reader->read_symbol_count++;
  return is_last_part;
}