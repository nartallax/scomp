#pragma once
#include "../../src/data_structures/reader.c"
#include "../test_utils.c"

const char *test_reader_simple() {
  reader r;

  byte *buffer_a = malloc(6);
  memcpy(buffer_a, "123456", 6);
  byte *buffer_b = malloc(3);
  memcpy(buffer_b, "789", 3);

  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(reader_get_bytes_available(&r) == 0);
  TEST_ASSERT(reader_get_bits_available(&r) == 0);
  TEST_ASSERT(reader_push_buffer(&r, buffer_make(buffer_a, 6, 150))); // 150 to test if the reader goes beyond length
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
  TEST_ASSERT(reader_push_buffer(&r, buffer_make(buffer_b, 3, 3)));
  TEST_ASSERT(reader_get_bytes_available(&r) == 4);
  TEST_ASSERT(reader_read_byte(&r) == '6');
  TEST_ASSERT(reader_get_bytes_available(&r) == 3);
  TEST_ASSERT(!reader_has_exhausted_buffer(&r));
  TEST_ASSERT(reader_read_byte(&r) == '7');
  TEST_ASSERT(reader_get_bytes_available(&r) == 2);
  TEST_ASSERT(reader_has_exhausted_buffer(&r));
  buffer b = reader_recover_exhausted_buffer(&r);
  TEST_ASSERT(b.data == buffer_a);
  free(b.data);

  reader_deinit(&r, test_context);
  TEST_ASSERT(!context_is_errored(test_context));
  return NULL;
}

const char *test_reader_underflow() {
  return NULL;
}

const char *test_reader_allocation_failures() {
  return NULL;
}