#pragma once
#include "../src/utf8.c"
#include "test_utils.c"

const char *test_utf8_simple() {
  byte buffer[4] = {0, 0, 0, 0};

  TEST_ASSERT(utf8_can_bytes_be_bom_start(buffer, 0));
  TEST_ASSERT(!utf8_can_bytes_be_bom_start(buffer, 1));
  buffer[0] = 0xEF;
  TEST_ASSERT(utf8_can_bytes_be_bom_start(buffer, 1));
  TEST_ASSERT(!utf8_can_bytes_be_bom_start(buffer, 2));
  buffer[1] = 0xBB;
  TEST_ASSERT(utf8_can_bytes_be_bom_start(buffer, 2));
  TEST_ASSERT(!utf8_can_bytes_be_bom_start(buffer, 3));
  buffer[2] = 0xBF;
  TEST_ASSERT(utf8_can_bytes_be_bom_start(buffer, 3));

  TEST_ASSERT(utf8_get_sequence_length_by_first_byte('a') == 1);
  TEST_ASSERT(utf8_get_sequence_length_by_first_byte(0xC3) == 2);
  TEST_ASSERT(utf8_get_sequence_length_by_first_byte(0xE1) == 3);
  TEST_ASSERT(utf8_get_sequence_length_by_first_byte(0xF2) == 4);
  TEST_ASSERT(utf8_get_sequence_length_by_first_byte(0xC0) == 0);
  TEST_ASSERT(utf8_get_sequence_length_by_first_byte(0xFF) == 0);

  buffer[1] = 0xFF;
  TEST_ASSERT(!utf8_are_continuation_bytes_valid(buffer, 3));
  buffer[0] = 0xF0;
  buffer[1] = 0x9F;
  buffer[2] = 0x98;
  buffer[3] = 0x80;
  TEST_ASSERT(utf8_are_continuation_bytes_valid(buffer, 4));

  TEST_ASSERT(utf8_get_meaningful_bits_of_first_byte('a') == 'a');
  TEST_ASSERT(utf8_get_meaningful_bits_of_first_byte(0xF3) == 0b00000011);

  TEST_ASSERT(utf8_get_meaningful_bits_of_continuation_byte(0x9F) == 0b00011111);

  return NULL;
}