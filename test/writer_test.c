#pragma once

#include "../src/writer.c"
#include "test_utils.c"

const char *test_writer_bytes() {
  writer *w = malloc(sizeof(writer));
  TEST_ASSERT(writer_init(w, test_context, 2));
  TEST_ASSERT(writer_get_bytes_stored(w) == 0);
  TEST_ASSERT(writer_consume_full_buffer(w).length == 0);
  TEST_ASSERT(writer_consume_nonempty_buffer(w).buffer.length == 0);

  writer_write_byte(w, 5);
  writer_write_byte(w, 4);
  TEST_ASSERT(writer_get_bytes_stored(w) == 2);
  buffer b = writer_consume_full_buffer(w);
  TEST_ASSERT(writer_get_bytes_stored(w) == 0);
  TEST_ASSERT(b.length == 2);
  TEST_ASSERT(b.data[0] == 5 && b.data[1] == 4);
  free(b.data);

  writer_write_byte(w, 3);
  TEST_ASSERT(writer_get_bytes_stored(w) == 1);
  TEST_ASSERT(writer_consume_full_buffer(w).length == 0);
  b = writer_consume_nonempty_buffer(w).buffer;
  TEST_ASSERT(writer_get_bytes_stored(w) == 0);
  TEST_ASSERT(b.length == 1);
  TEST_ASSERT(b.data[0] == 3);
  free(b.data);

  writer_write_byte(w, 2);
  TEST_ASSERT(writer_get_bytes_stored(w) == 1);
  TEST_ASSERT(writer_consume_full_buffer(w).length == 0);

  writer_write_byte(w, 6);
  writer_write_byte(w, 7);
  writer_write_byte(w, 8);
  writer_write_byte(w, 9);
  writer_write_byte(w, 10);
  TEST_ASSERT(writer_get_bytes_stored(w) == 6);
  // this is supposed to check if nonempty_consume can also return full buffer, not just the tail one
  b = writer_consume_nonempty_buffer(w).buffer;
  TEST_ASSERT(writer_get_bytes_stored(w) == 4);
  TEST_ASSERT(b.length == 2);
  TEST_ASSERT(b.data[0] == 2 && b.data[1] == 6);
  free(b.data);

  b = writer_consume_all_buffers(w).buffer;
  TEST_ASSERT(writer_get_bytes_stored(w) == 0);
  TEST_ASSERT(b.length == 4);
  TEST_ASSERT(b.data[0] == 7 && b.data[1] == 8 && b.data[2] == 9 && b.data[3] == 10);
  free(b.data);

  writer_deinit(w, test_context);
  free(w);

  return NULL;
}

void _write_byte_as_bits(writer *writer, byte value) {
  for (int i = 0; i < 8; i++) {
    byte bit = value & (1 << i) ? 1 : 0;
    writer_write_bit(writer, bit);
  }
}

const char *test_writer_bits() {
  writer *w = malloc(sizeof(writer));
  TEST_ASSERT(writer_init(w, test_context, 2));
  buffer b;
  TEST_ASSERT(writer_get_bytes_stored(w) == 0);

  writer_write_bit(w, 1);
  TEST_ASSERT(writer_get_bytes_stored(w) == 1);

  writer_write_bit(w, 1);
  writer_write_bit(w, 0);
  writer_write_bit(w, 0);
  writer_write_bit(w, 1);
  writer_write_bit(w, 0);
  writer_write_bit(w, 1);
  TEST_ASSERT(writer_get_bytes_stored(w) == 1);

  writer_write_bit(w, 0);
  TEST_ASSERT(writer_get_bytes_stored(w) == 1);

  writer_write_bit(w, 1);
  TEST_ASSERT(writer_get_bytes_stored(w) == 2);

  TEST_ASSERT(writer_consume_full_buffer(w).length == 0);
  b = writer_consume_nonempty_buffer(w).buffer;
  TEST_ASSERT(writer_get_bytes_stored(w) == 0);
  TEST_ASSERT(b.length == 2);
  TEST_ASSERT(b.data[0] == 0b01010011 && b.data[1] == 0b00000001);
  free(b.data);

  _write_byte_as_bits(w, 0b00110101);
  _write_byte_as_bits(w, 0b11001010);
  writer_write_bit(w, 1);
  writer_write_bit(w, 0);
  writer_write_bit(w, 1);
  TEST_ASSERT(writer_get_bytes_stored(w) == 3);
  b = writer_consume_all_buffers(w).buffer;
  TEST_ASSERT(writer_get_bytes_stored(w) == 0);
  TEST_ASSERT(b.length == 3);
  TEST_ASSERT(b.data[0] == 0b00110101 && b.data[1] == 0b11001010 && b.data[2] == 0b00000101);
  free(b.data);

  writer_deinit(w, test_context);
  free(w);
  return NULL;
}

const char *test_writer_buffer_reuse() {
  writer *w = malloc(sizeof(writer));
  TEST_ASSERT(writer_init(w, test_context, 2));
  buffer b;

  writer_write_byte(w, 2);
  writer_write_byte(w, 3);
  writer_write_byte(w, 4);
  b = writer_consume_full_buffer(w);
  TEST_ASSERT(b.data[0] == 2 && b.data[1] == 3);
  byte *reused_array = b.data;

  TEST_ASSERT(writer_supply_dirty_buffer(w, b.data));
  writer_write_byte(w, 5);
  writer_write_byte(w, 6);
  _write_byte_as_bits(w, 0b10000000);
  b = writer_consume_full_buffer(w);
  TEST_ASSERT(b.data[0] == 4 && b.data[1] == 5);
  free(b.data);
  b = writer_consume_full_buffer(w);
  TEST_ASSERT(b.data[0] == 6 && b.data[1] == 0b10000000);
  TEST_ASSERT(b.data == reused_array);
  free(reused_array);

  // this checks that all unused free buffers are deleted too
  byte *other_array = malloc(sizeof(byte) * 2);
  TEST_ASSERT(writer_supply_zeroinit_buffer(w, other_array));

  writer_deinit(w, test_context);
  free(w);

  return NULL;
}

const char *test_writer_allocation_failure() {
  // TODO: revive this test
  /*
  writer *w = malloc(sizeof(writer));
  for (int i = 0; i < 3; i++) {
    setup_test_context(i);
    TEST_ASSERT(!writer_init(w, test_context, 2));
    TEST_ASSERT(writer_get_bytes_stored(w) == 0);
  }

  setup_test_context(3);
  TEST_ASSERT(writer_init(w, test_context, 2));
  size_t queue_default_length = w->buffers.length;
  writer_write_byte(w, 1);
  buffer_or_error b = writer_consume_all_buffers(w);
  TEST_ASSERT(b.buffer.length == 0 && b.buffer.data == NULL && b.is_error);
  writer_deinit(w, test_context);

  setup_test_context(3);
  TEST_ASSERT(writer_init(w, test_context, 2));
  writer_write_byte(w, 1);
  TEST_ASSERT(!context_is_errored(test_context));
  writer_write_byte(w, 2);
  TEST_ASSERT(context_is_errored(test_context));
  writer_deinit(w, test_context);

  setup_test_context(3 + queue_default_length - 1);
  TEST_ASSERT(writer_init(w, test_context, 2));
  for (byte i = 0; i < queue_default_length - 2; i++) {
    writer_write_byte(w, i);
    writer_write_byte(w, i * 2);
    TEST_ASSERT(!context_is_errored(test_context));
  }
  writer_write_byte(w, 1);
  TEST_ASSERT(!context_is_errored(test_context));
  writer_write_byte(w, 2);
  TEST_ASSERT(context_is_errored(test_context));
  writer_deinit(w, test_context);

  setup_test_context_default();
  TEST_ASSERT(writer_init(w, test_context, 3));
  writer_write_byte(w, 1);
  update_test_context_for_alloc_failure(0);
  b = writer_consume_nonempty_buffer(w);
  TEST_ASSERT(b.buffer.length == 0 && b.buffer.data == NULL && b.is_error);
  writer_deinit(w, test_context);

  setup_test_context_default();
  TEST_ASSERT(writer_init(w, test_context, 3));
  writer_write_byte(w, 1);
  writer_write_byte(w, 2);
  writer_write_byte(w, 3);
  writer_write_byte(w, 4);
  update_test_context_for_alloc_failure(1);
  b = writer_consume_all_buffers(w);
  TEST_ASSERT(b.buffer.length == 0 && b.buffer.data == NULL && b.is_error);
  writer_deinit(w, test_context);
  free(w);
  */
  return NULL;
}
