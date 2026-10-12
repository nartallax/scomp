#pragma once
#include "../src/math.c"
#include "test_utils.c"

const char *test_most_significant_bit() {
  TEST_ASSERT(most_significant_bit_index(0) == -1);
  TEST_ASSERT(most_significant_bit_index(1) == 0);
  TEST_ASSERT(most_significant_bit_index(2) == 1);
  TEST_ASSERT(most_significant_bit_index(3) == 1);
  TEST_ASSERT(most_significant_bit_index(4) == 2);
  TEST_ASSERT(most_significant_bit_index(5) == 2);
  TEST_ASSERT(most_significant_bit_index(6) == 2);
  TEST_ASSERT(most_significant_bit_index(7) == 2);
  TEST_ASSERT(most_significant_bit_index(8) == 3);
  TEST_ASSERT(most_significant_bit_index(15) == 3);
  TEST_ASSERT(most_significant_bit_index(16) == 4);
  TEST_ASSERT(most_significant_bit_index(31) == 4);
  TEST_ASSERT(most_significant_bit_index(255) == 7);
  TEST_ASSERT(most_significant_bit_index(1024) == 10);
  TEST_ASSERT(most_significant_bit_index(65000) == 15);
  return NULL;
}
