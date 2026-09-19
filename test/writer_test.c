#pragma once

#include "../src/writer.c"
#include "./test_utils.c"

const char *test_writer_bytes() {
  writer *writer = writer_new(test_context, 2);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 0);
  TEST_ASSERT(writer_consume_full_buffer(writer).length == 0);
  TEST_ASSERT(writer_consume_nonempty_buffer(writer).length == 0);

  writer_write_byte(writer, 5);
  writer_write_byte(writer, 4);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 2);
  buffer b = writer_consume_full_buffer(writer);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 0);
  TEST_ASSERT(b.length == 2);
  TEST_ASSERT(b.data[0] == 5 && b.data[1] == 4);
  free(b.data);

  writer_write_byte(writer, 3);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 1);
  TEST_ASSERT(writer_consume_full_buffer(writer).length == 0);
  b = writer_consume_nonempty_buffer(writer);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 0);
  TEST_ASSERT(b.length == 1);
  TEST_ASSERT(b.data[0] == 3);
  free(b.data);

  writer_write_byte(writer, 2);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 1);
  TEST_ASSERT(writer_consume_full_buffer(writer).length == 0);

  writer_write_byte(writer, 6);
  writer_write_byte(writer, 7);
  writer_write_byte(writer, 8);
  writer_write_byte(writer, 9);
  writer_write_byte(writer, 10);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 6);
  // this is supposed to check if nonempty_consume can also return full buffer, not just the tail one
  b = writer_consume_nonempty_buffer(writer);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 4);
  TEST_ASSERT(b.length == 2);
  TEST_ASSERT(b.data[0] == 2 && b.data[1] == 6);
  free(b.data);

  b = writer_consume_all_buffers(writer);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 0);
  TEST_ASSERT(b.length == 4);
  TEST_ASSERT(b.data[0] == 7 && b.data[1] == 8 && b.data[2] == 9 && b.data[3] == 10);
  free(b.data);

  writer_delete(writer);

  return NULL;
}

void _write_byte_as_bits(writer *writer, byte value) {
  for (int i = 0; i < 8; i++) {
    byte bit = value & (1 << i) ? 1 : 0;
    writer_write_bit(writer, bit);
  }
}

const char *test_writer_bits() {
  writer *writer = writer_new(test_context, 2);
  buffer b;
  TEST_ASSERT(writer_get_bytes_stored(writer) == 0);

  writer_write_bit(writer, 1);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 1);

  writer_write_bit(writer, 1);
  writer_write_bit(writer, 0);
  writer_write_bit(writer, 0);
  writer_write_bit(writer, 1);
  writer_write_bit(writer, 0);
  writer_write_bit(writer, 1);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 1);

  writer_write_bit(writer, 0);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 1);

  writer_write_bit(writer, 1);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 2);

  TEST_ASSERT(writer_consume_full_buffer(writer).length == 0);
  b = writer_consume_nonempty_buffer(writer);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 0);
  TEST_ASSERT(b.length == 2);
  TEST_ASSERT(b.data[0] == 0b01010011 && b.data[1] == 0b00000001);
  free(b.data);

  _write_byte_as_bits(writer, 0b00110101);
  _write_byte_as_bits(writer, 0b11001010);
  writer_write_bit(writer, 1);
  writer_write_bit(writer, 0);
  writer_write_bit(writer, 1);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 3);
  b = writer_consume_all_buffers(writer);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 0);
  TEST_ASSERT(b.length == 3);
  TEST_ASSERT(b.data[0] == 0b00110101 && b.data[1] == 0b11001010 && b.data[2] == 0b00000101);
  free(b.data);

  writer_delete(writer);
  return NULL;
}

const char *test_writer_early_close() {
  writer *writer = writer_new(test_context, 2);
  TEST_ASSERT(context_is_errored(test_context) == false);
  writer_write_byte(writer, 1);
  writer_write_byte(writer, 2);
  writer_write_byte(writer, 3);
  TEST_ASSERT(context_is_errored(test_context) == false);
  writer_delete(writer);
  TEST_ASSERT(context_is_errored(test_context) == true);

  return NULL;
}

const char *test_writer_buffer_reuse() {
  writer *writer = writer_new(test_context, 2);
  buffer b;

  writer_write_byte(writer, 2);
  writer_write_byte(writer, 3);
  writer_write_byte(writer, 4);
  b = writer_consume_full_buffer(writer);
  TEST_ASSERT(b.data[0] == 2 && b.data[1] == 3);
  byte *reused_array = b.data;

  writer_supply_dirty_buffer(writer, b.data);
  writer_write_byte(writer, 5);
  writer_write_byte(writer, 6);
  _write_byte_as_bits(writer, 0b10000000);
  b = writer_consume_full_buffer(writer);
  TEST_ASSERT(b.data[0] == 4 && b.data[1] == 5);
  free(b.data);
  b = writer_consume_full_buffer(writer);
  TEST_ASSERT(b.data[0] == 6 && b.data[1] == 0b10000000);
  TEST_ASSERT(b.data == reused_array);
  free(reused_array);

  // this checks that all unused free buffers are deleted too
  byte *other_array = malloc(sizeof(byte) * 2);
  writer_supply_zeroinit_buffer(writer, other_array);

  writer_delete(writer);

  return NULL;
}

const char *test_writer_allocation_failure() {
  writer *writer;
  for (int i = 0; i < 4; i++) {
    setup_test_context(i);
    TEST_ASSERT(writer_new(test_context, 2) == NULL);
  }

  setup_test_context(4);
  writer = writer_new(test_context, 2);
  writer_write_byte(writer, 1);
  buffer b = writer_consume_all_buffers(writer);
  TEST_ASSERT(b.length == 0);
  writer_delete(writer);

  setup_test_context(4);
  writer = writer_new(test_context, 2);
  TEST_ASSERT(writer_write_byte(writer, 1));
  TEST_ASSERT(!writer_write_byte(writer, 2));
  writer_delete(writer);

  setup_test_context(4 + QUEUE_DEFAULT_SIZE - 1);
  writer = writer_new(test_context, 2);
  for (byte i = 0; i < QUEUE_DEFAULT_SIZE - 2; i++) {
    TEST_ASSERT(writer_write_byte(writer, i));
    TEST_ASSERT(writer_write_byte(writer, i * 2));
  }
  TEST_ASSERT(writer_write_byte(writer, 1));
  TEST_ASSERT(!writer_write_byte(writer, 2));
  writer_delete(writer);

  return NULL;
}
