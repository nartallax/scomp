#pragma once
#include "../../src/data_structures/reader.c"
#include "../test_utils.c"

const char *test_reader_simple() {
  reader r;

  buffer buffer_a = malloc_string_buffer("123456");
  buffer_a.allocated_length = 150; // 150 to test if the reader goes beyond length
  buffer buffer_b = malloc_string_buffer("789");

  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(reader_get_bytes_available(&r) == 0);
  TEST_ASSERT(reader_get_bits_available(&r) == 0);
  TEST_ASSERT(reader_push_buffer(&r, buffer_a));
  TEST_ASSERT(!reader_has_safe_amount_of_data(&r));
  TEST_ASSERT(reader_get_bytes_available(&r) == 6);
  TEST_ASSERT(reader_get_bits_available(&r) == 48);
  TEST_ASSERT(reader_read_byte(&r) == '1');
  TEST_ASSERT(reader_read_byte(&r) == '2');
  TEST_ASSERT(reader_read_byte(&r) == '3');
  TEST_ASSERT(reader_get_bytes_available(&r) == 3);
  TEST_ASSERT(reader_get_bits_available(&r) == 24);
  TEST_ASSERT(reader_read_bit(&r) == ('4' & 1));
  TEST_ASSERT(reader_read_bit(&r) == (('4' >> 1) & 1));
  TEST_ASSERT(reader_read_bit(&r) == (('4' >> 2) & 1));
  TEST_ASSERT(reader_get_bits_available(&r) == 21);
  TEST_ASSERT(reader_read_bit(&r) == (('4' >> 3) & 1));
  TEST_ASSERT(reader_read_bit(&r) == (('4' >> 4) & 1));
  TEST_ASSERT(reader_read_bit(&r) == (('4' >> 5) & 1));
  TEST_ASSERT(reader_read_bit(&r) == (('4' >> 6) & 1));
  TEST_ASSERT(reader_get_bits_available(&r) == 17);
  TEST_ASSERT(reader_read_bit(&r) == (('4' >> 7) & 1));
  TEST_ASSERT(reader_get_bytes_available(&r) == 2);
  TEST_ASSERT(reader_get_bits_available(&r) == 16);
  TEST_ASSERT(reader_read_byte(&r) == '5');
  TEST_ASSERT(reader_get_bytes_available(&r) == 1);
  TEST_ASSERT(!reader_has_exhausted_buffer(&r));
  TEST_ASSERT(reader_push_buffer(&r, buffer_b));
  TEST_ASSERT(reader_get_bytes_available(&r) == 4);
  TEST_ASSERT(reader_read_byte(&r) == '6');
  TEST_ASSERT(reader_get_bytes_available(&r) == 3);
  TEST_ASSERT(!reader_has_exhausted_buffer(&r));
  TEST_ASSERT(reader_read_byte(&r) == '7');
  TEST_ASSERT(reader_get_bytes_available(&r) == 2);
  TEST_ASSERT(reader_has_exhausted_buffer(&r));
  buffer b = reader_recover_exhausted_buffer(&r);
  TEST_ASSERT(b.data == buffer_a.data);
  free(b.data);

  reader_deinit(&r, test_context);
  TEST_ASSERT(!context_is_errored(test_context));
  return NULL;
}

const char *test_reader_underflow() {
  reader r;
  byte *b;

  TEST_ASSERT(reader_init(&r, test_context));
  b = malloc(3);
  memcpy(b, "123", 3);
  TEST_ASSERT(reader_push_buffer(&r, buffer_make(b, 3, 3)));
  TEST_ASSERT(reader_read_byte(&r) == '1');
  TEST_ASSERT(reader_read_byte(&r) == '2');
  TEST_ASSERT(reader_read_byte(&r) == '3');
  TEST_ASSERT(!context_is_errored(test_context));
  TEST_ASSERT(reader_read_byte(&r) == 0);
  TEST_ASSERT(context_is_errored(test_context));
  reader_deinit(&r, test_context);

  setup_test_context_default();
  TEST_ASSERT(reader_init(&r, test_context));
  b = malloc(3);
  memcpy(b, "123", 3);
  TEST_ASSERT(reader_push_buffer(&r, buffer_make(b, 3, 3)));
  TEST_ASSERT(reader_read_byte(&r) == '1');
  TEST_ASSERT(reader_read_byte(&r) == '2');
  TEST_ASSERT(reader_read_byte(&r) == '3');
  TEST_ASSERT(!context_is_errored(test_context));
  TEST_ASSERT(reader_read_bit(&r) == 0);
  TEST_ASSERT(context_is_errored(test_context));
  reader_deinit(&r, test_context);

  return NULL;
}

const char *test_reader_snapshots() {
  reader r;
  reader_snapshot snapshot;

  // empty state
  TEST_ASSERT(reader_init(&r, test_context));
  snapshot = reader_take_snapshot(&r);
  TEST_ASSERT(reader_push_buffer(&r, malloc_string_buffer("54321")));
  TEST_ASSERT(reader_get_bytes_available(&r) == 5);
  reader_restore_snapshot(&r, snapshot);
  TEST_ASSERT(reader_get_bytes_available(&r) == 5);
  TEST_ASSERT(reader_read_byte(&r) == '5');
  TEST_ASSERT(reader_read_byte(&r) == '4');
  TEST_ASSERT(reader_get_bytes_available(&r) == 3);
  reader_restore_snapshot(&r, snapshot);
  TEST_ASSERT(reader_get_bytes_available(&r) == 5);
  TEST_ASSERT(reader_read_byte(&r) == '5');
  TEST_ASSERT(reader_read_byte(&r) == '4');
  TEST_ASSERT(reader_get_bytes_available(&r) == 3);
  reader_deinit(&r, test_context);

  // non-empty state
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(reader_push_buffer(&r, malloc_string_buffer("54321")));
  TEST_ASSERT(reader_read_byte(&r) == '5');
  TEST_ASSERT(reader_read_byte(&r) == '4');
  TEST_ASSERT(reader_get_bytes_available(&r) == 3);
  snapshot = reader_take_snapshot(&r);
  TEST_ASSERT(reader_read_byte(&r) == '3');
  TEST_ASSERT(reader_read_byte(&r) == '2');
  TEST_ASSERT(reader_get_bytes_available(&r) == 1);
  reader_restore_snapshot(&r, snapshot);
  TEST_ASSERT(reader_get_bytes_available(&r) == 3);
  TEST_ASSERT(reader_read_byte(&r) == '3');
  TEST_ASSERT(reader_read_byte(&r) == '2');
  TEST_ASSERT(reader_get_bytes_available(&r) == 1);
  reader_restore_snapshot(&r, snapshot);
  TEST_ASSERT(reader_get_bits_available(&r) == 24);
  TEST_ASSERT(reader_read_bit(&r) == (('3' >> 0) & 1));
  TEST_ASSERT(reader_read_bit(&r) == (('3' >> 1) & 1));
  TEST_ASSERT(reader_read_bit(&r) == (('3' >> 2) & 1));
  TEST_ASSERT(reader_get_bits_available(&r) == 21);
  reader_restore_snapshot(&r, snapshot);
  TEST_ASSERT(reader_get_bits_available(&r) == 24);
  TEST_ASSERT(reader_read_bit(&r) == (('3' >> 0) & 1));
  TEST_ASSERT(reader_read_bit(&r) == (('3' >> 1) & 1));
  TEST_ASSERT(reader_read_bit(&r) == (('3' >> 2) & 1));
  TEST_ASSERT(reader_get_bits_available(&r) == 21);
  reader_deinit(&r, test_context);

  // go one buffer back
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(reader_push_buffer(&r, malloc_string_buffer("54")));
  TEST_ASSERT(reader_push_buffer(&r, malloc_string_buffer("32")));
  TEST_ASSERT(reader_read_byte(&r) == '5');
  snapshot = reader_take_snapshot(&r);
  TEST_ASSERT(reader_read_byte(&r) == '4');
  TEST_ASSERT(reader_read_byte(&r) == '3');
  TEST_ASSERT(reader_get_bytes_available(&r) == 1);
  reader_restore_snapshot(&r, snapshot);
  TEST_ASSERT(reader_get_bytes_available(&r) == 3);
  TEST_ASSERT(reader_read_byte(&r) == '4');
  TEST_ASSERT(reader_read_byte(&r) == '3');
  TEST_ASSERT(reader_read_byte(&r) == '2'); // get to the end of the buffer
  reader_restore_snapshot(&r, snapshot);
  TEST_ASSERT(reader_get_bytes_available(&r) == 3);
  TEST_ASSERT(reader_read_byte(&r) == '4');
  reader_deinit(&r, test_context);

  // skip several buffers back
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(reader_push_buffer(&r, malloc_string_buffer("98")));
  TEST_ASSERT(reader_push_buffer(&r, malloc_string_buffer("76")));
  TEST_ASSERT(reader_push_buffer(&r, malloc_string_buffer("54")));
  TEST_ASSERT(reader_push_buffer(&r, malloc_string_buffer("32")));
  TEST_ASSERT(reader_push_buffer(&r, malloc_string_buffer("10")));
  TEST_ASSERT(reader_get_bytes_available(&r) == 10);
  TEST_ASSERT(reader_read_byte(&r) == '9');
  TEST_ASSERT(reader_read_byte(&r) == '8');
  TEST_ASSERT(reader_read_byte(&r) == '7');
  TEST_ASSERT(reader_get_bytes_available(&r) == 7);
  snapshot = reader_take_snapshot(&r);
  TEST_ASSERT(reader_read_byte(&r) == '6');
  TEST_ASSERT(reader_read_byte(&r) == '5');
  TEST_ASSERT(reader_read_byte(&r) == '4');
  TEST_ASSERT(reader_read_byte(&r) == '3');
  TEST_ASSERT(reader_get_bytes_available(&r) == 3);
  reader_restore_snapshot(&r, snapshot);
  TEST_ASSERT(reader_get_bytes_available(&r) == 7);
  TEST_ASSERT(reader_read_byte(&r) == '6');
  reader_deinit(&r, test_context);

  return NULL;
}

const char *test_reader_allocation_failures() {
  reader r;

  buffer b = buffer_make(NULL, 1, 1);

  setup_test_context(0);
  TEST_ASSERT(!reader_init(&r, test_context));

  setup_test_context(1);
  TEST_ASSERT(reader_init(&r, test_context));
  for (int64 i = 0; i < (1 << READER_QUEUE_DEFAULT_SHIFT) - 1; i++) {
    TEST_ASSERT(reader_push_buffer(&r, b));
  }
  TEST_ASSERT(!reader_push_buffer(&r, b));
  reader_deinit(&r, test_context);

  return NULL;
}