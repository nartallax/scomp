#pragma once
#include "../../src/data_structures/byte_fixed_stack.c"
#include "../test_utils.c"

const char *test_hbfstack_simple() {
  byte_fixed_stack s;
  TEST_ASSERT(bfstack_init(&s, test_context, 4));
  TEST_ASSERT(bfstack_get_capacity(&s) == 4);
  TEST_ASSERT(bfstack_get_count(&s) == 0);

  bfstack_push(&s, 4);
  TEST_ASSERT(bfstack_get_count(&s) == 1);
  TEST_ASSERT(bfstack_peek(&s) == 4);
  bfstack_push(&s, 5);
  TEST_ASSERT(bfstack_get_count(&s) == 2);
  TEST_ASSERT(bfstack_peek(&s) == 5);
  bfstack_push(&s, 7);
  TEST_ASSERT(bfstack_get_count(&s) == 3);
  TEST_ASSERT(bfstack_peek(&s) == 7);
  bfstack_push(&s, 9);
  TEST_ASSERT(bfstack_get_count(&s) == 4);
  TEST_ASSERT(bfstack_peek(&s) == 9);

  TEST_ASSERT(bfstack_pop(&s) == 9);
  TEST_ASSERT(bfstack_peek(&s) == 7);
  TEST_ASSERT(bfstack_pop(&s) == 7);
  TEST_ASSERT(bfstack_peek(&s) == 5);
  TEST_ASSERT(bfstack_pop(&s) == 5);
  TEST_ASSERT(bfstack_peek(&s) == 4);
  TEST_ASSERT(bfstack_pop(&s) == 4);

  bfstack_deinit(&s, test_context);
  return NULL;
}

const char *test_hbfstack_allocation_failures() {
  byte_fixed_stack s;
  setup_test_context(0);
  TEST_ASSERT(!bfstack_init(&s, test_context, 8));

  setup_test_context(1);
  TEST_ASSERT(bfstack_init(&s, test_context, 8));

  bfstack_deinit(&s, test_context);
  return NULL;
}