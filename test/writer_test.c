#pragma once

#include "../src/writer.c"
#include "./test_utils.c"

const char *test_writer_bytes() {
  writer *writer = writer_new(test_context, 2);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 0, "No bytes stored right after creation");
  TEST_ASSERT(writer_consume_full_buffer(writer).length == 0, "Consume on empty writer returns empty buffer");
  TEST_ASSERT(writer_consume_nonempty_buffer(writer).length == 0, "Consume nonempty on empty writer returns empty buffer");

  writer_write_byte(writer, 5);
  writer_write_byte(writer, 4);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 2, "Some bytes should be stored");
  buffer b = writer_consume_full_buffer(writer);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 0, "No bytes should be stored after consumption");
  TEST_ASSERT(b.length == 2, "Full buffer should have length of the writer");
  TEST_ASSERT(b.data[0] == 5 && b.data[1] == 4, "Buffer should store expected bytes");
  free(b.data);

  writer_write_byte(writer, 3);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 1, "Some bytes should be stored");
  TEST_ASSERT(writer_consume_full_buffer(writer).length == 0, "Consume on non-full writer returns empty buffer");
  b = writer_consume_nonempty_buffer(writer);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 0, "No bytes should be stored after consumption");
  TEST_ASSERT(b.length == 1, "Nonempty buffer should have length of 1");
  TEST_ASSERT(b.data[0] == 3, "Nonempty buffer should store expected bytes");
  free(b.data);

  writer_write_byte(writer, 2);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 1, "Some bytes should be stored");
  TEST_ASSERT(writer_consume_full_buffer(writer).length == 0, "Consume on non-full writer returns empty buffer");

  writer_write_byte(writer, 6);
  writer_write_byte(writer, 7);
  writer_write_byte(writer, 8);
  writer_write_byte(writer, 9);
  writer_write_byte(writer, 10);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 6, "Some bytes should be stored");
  // this is supposed to check if nonempty_consume can also return full buffer, not just the tail one
  b = writer_consume_nonempty_buffer(writer);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 4, "Reduced number of bytes should be stored after consumption");
  TEST_ASSERT(b.length == 2, "Full buffer should have length of the writer");
  TEST_ASSERT(b.data[0] == 2 && b.data[1] == 6, "Buffer should store expected bytes");
  free(b.data);

  b = writer_consume_all_buffers(writer);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 0, "No bytes should be stored after consumption");
  TEST_ASSERT(b.length == 4, "Full buffer should have the remaining bytes");
  TEST_ASSERT(b.data[0] == 7 && b.data[1] == 8 && b.data[2] == 9 && b.data[3] == 10, "Buffer should store expected bytes");
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
  TEST_ASSERT(writer_get_bytes_stored(writer) == 0, "No bytes stored right after creation");

  writer_write_bit(writer, 1);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 1, "One bit stored counts as one byte");

  writer_write_bit(writer, 1);
  writer_write_bit(writer, 0);
  writer_write_bit(writer, 0);
  writer_write_bit(writer, 1);
  writer_write_bit(writer, 0);
  writer_write_bit(writer, 1);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 1, "Seven bits stored counts as one byte");

  writer_write_bit(writer, 0);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 1, "Eight bits stored counts as one byte");

  writer_write_bit(writer, 1);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 2, "Nine bits stored counts as two bytes");

  TEST_ASSERT(writer_consume_full_buffer(writer).length == 0, "Consume on non-full writer returns empty buffer");
  b = writer_consume_nonempty_buffer(writer);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 0, "No bytes stored after consumption");
  TEST_ASSERT(b.length == 2, "Consume of 9-bits full writer returns buffer of 2");
  TEST_ASSERT(b.data[0] == 0b01010011 && b.data[1] == 0b00000001, "9-bits buffer should have expected content");
  free(b.data);

  _write_byte_as_bits(writer, 0b00110101);
  _write_byte_as_bits(writer, 0b11001010);
  writer_write_bit(writer, 1);
  writer_write_bit(writer, 0);
  writer_write_bit(writer, 1);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 3, "19 bits stored counts as three bytes");
  b = writer_consume_all_buffers(writer);
  TEST_ASSERT(writer_get_bytes_stored(writer) == 0, "No bytes stored after consumption");
  TEST_ASSERT(b.length == 3, "Buffer has expected length");
  TEST_ASSERT(b.data[0] == 0b00110101 && b.data[1] == 0b11001010 && b.data[2] == 0b00000101, "Buffer has expected content");
  free(b.data);

  writer_delete(writer);
  return NULL;
}

const char *test_writer_early_close() {
  writer *writer = writer_new(test_context, 2);
  TEST_ASSERT(context_is_errored(test_context) == false, "No errors initially");
  writer_write_byte(writer, 1);
  writer_write_byte(writer, 2);
  writer_write_byte(writer, 3);
  TEST_ASSERT(context_is_errored(test_context) == false, "No errors after writes");
  writer_delete(writer);
  TEST_ASSERT(context_is_errored(test_context) == true, "Some errors after early close");

  return NULL;
}

const char *test_writer_buffer_reuse() {
  writer *writer = writer_new(test_context, 2);
  buffer b;

  writer_write_byte(writer, 2);
  writer_write_byte(writer, 3);
  writer_write_byte(writer, 4);
  b = writer_consume_full_buffer(writer);
  TEST_ASSERT(b.data[0] == 2 && b.data[1] == 3, "Buffer has expected content");
  byte *reused_array = b.data;

  writer_supply_dirty_buffer(writer, b.data);
  writer_write_byte(writer, 5);
  writer_write_byte(writer, 6);
  _write_byte_as_bits(writer, 0b10000000);
  b = writer_consume_full_buffer(writer);
  TEST_ASSERT(b.data[0] == 4 && b.data[1] == 5, "Buffer has expected content");
  free(b.data);
  b = writer_consume_full_buffer(writer);
  TEST_ASSERT(b.data[0] == 6 && b.data[1] == 0b10000000, "Buffer has expected content");
  TEST_ASSERT(b.data == reused_array, "Buffer is actually reused");
  free(reused_array);

  // this checks that all unused free buffers are deleted too
  byte *other_array = malloc(sizeof(byte) * 2);
  writer_supply_zeroinit_buffer(writer, other_array);

  writer_delete(writer);

  return NULL;
}

const char *test_writer_allocation_failure() {
  for (int i = 0; i < 6; i++) {
    setup_test_context(i);
    TEST_ASSERT(writer_new(test_context, 2) == NULL, "Writer should be null on allocation fail");
  }

  setup_test_context(6);
  writer *writer = writer_new(test_context, 2);
  writer_write_byte(writer, 1);
  buffer b = writer_consume_all_buffers(writer);
  TEST_ASSERT(b.length == 0, "All-buffer allocation fail should return empty buffer");
  writer_delete(writer);

  return NULL;
}