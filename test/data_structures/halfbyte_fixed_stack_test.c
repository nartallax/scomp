#pragma once
#include "../../src/data_structures/halfbyte_fixed_stack.c"
#include "../test_utils.c"

const char *test_hbfstack_simple() {
  halfbyte_fixed_stack s;
  TEST_ASSERT(hbfstack_init(&s, test_context, 4));
  TEST_ASSERT(hbfstack_get_capacity(&s) == 4);
  TEST_ASSERT(hbfstack_get_count(&s) == 0);

  hbfstack_push(&s, 4);
  TEST_ASSERT(hbfstack_get_count(&s) == 1);
  hbfstack_push(&s, 5);
  TEST_ASSERT(hbfstack_get_count(&s) == 2);
  hbfstack_push(&s, 7);
  TEST_ASSERT(hbfstack_get_count(&s) == 3);
  hbfstack_push(&s, 9);
  TEST_ASSERT(hbfstack_get_count(&s) == 4);

  TEST_ASSERT(hbfstack_pop(&s) == 9);
  TEST_ASSERT(hbfstack_pop(&s) == 7);
  TEST_ASSERT(hbfstack_pop(&s) == 5);
  TEST_ASSERT(hbfstack_pop(&s) == 4);

  hbfstack_deinit(&s, test_context);
  return NULL;
}

const char *test_hbfstack_allocation_failures() {
  halfbyte_fixed_stack s;
  setup_test_context(0);
  TEST_ASSERT(!hbfstack_init(&s, test_context, 8));

  setup_test_context(1);
  TEST_ASSERT(hbfstack_init(&s, test_context, 8));

  hbfstack_deinit(&s, test_context);
  return NULL;
}