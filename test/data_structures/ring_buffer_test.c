#pragma once
#include "../../src/data_structures/ring_buffer.c"
#include "../test_utils.c"

const char *test_ring_buffer_simple() {
  ring_buffer rb;
  TEST_ASSERT(ring_buffer_init(&rb, test_context, 2));
  TEST_ASSERT(ring_buffer_get_length(&rb) == 4);
  ring_buffer_push(&rb, 5);
  ring_buffer_push(&rb, 6);
  ring_buffer_push(&rb, 7);
  ring_buffer_push(&rb, 8);
  TEST_ASSERT(ring_buffer_get(&rb, 0) == 5);
  TEST_ASSERT(ring_buffer_get(&rb, 1) == 6);
  TEST_ASSERT(ring_buffer_get(&rb, 2) == 7);
  TEST_ASSERT(ring_buffer_get(&rb, 3) == 8);
  TEST_ASSERT(ring_buffer_get(&rb, 4) == 5);
  TEST_ASSERT(ring_buffer_get(&rb, 9) == 6);
  TEST_ASSERT(ring_buffer_get(&rb, 14) == 7);
  TEST_ASSERT(ring_buffer_get(&rb, 19) == 8);
  ring_buffer_push(&rb, 9);
  ring_buffer_push(&rb, 10);
  TEST_ASSERT(ring_buffer_get(&rb, 0) == 9);
  TEST_ASSERT(ring_buffer_get(&rb, 1) == 10);
  TEST_ASSERT(ring_buffer_get(&rb, 2) == 7);
  TEST_ASSERT(ring_buffer_get(&rb, 3) == 8);
  TEST_ASSERT(ring_buffer_get(&rb, 4) == 9);
  TEST_ASSERT(ring_buffer_get(&rb, 9) == 10);
  TEST_ASSERT(ring_buffer_get(&rb, 14) == 7);
  TEST_ASSERT(ring_buffer_get(&rb, 19) == 8);
  ring_buffer_deinit(&rb, test_context);
  return NULL;
}

const char *test_ring_buffer_allocation_fail() {
  setup_test_context(0);
  ring_buffer rb;
  TEST_ASSERT(!ring_buffer_init(&rb, test_context, 2));
  return NULL;
}