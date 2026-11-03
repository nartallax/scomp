#pragma once
#include "../../src/data_structures/fixed_stack.c"
#include "../test_utils.c"

const char *test_hbfstack_simple() {
  fixed_stack s;
  fstack_init(&s, test_context, sizeof(uint16), 4);
  TEST_ASSERT(fstack_get_capacity(&s) == 4);
  TEST_ASSERT(fstack_get_count(&s) == 0);

  uint16 *slot;
  slot = fstack_push(&s);
  *slot = 4;
  TEST_ASSERT(fstack_get_count(&s) == 1);
  TEST_ASSERT(*((uint16 *)fstack_peek(&s)) == 4);
  slot = fstack_push(&s);
  *slot = 5;
  TEST_ASSERT(fstack_get_count(&s) == 2);
  TEST_ASSERT(*((uint16 *)fstack_peek(&s)) == 5);
  slot = fstack_push(&s);
  *slot = 7;
  TEST_ASSERT(fstack_get_count(&s) == 3);
  TEST_ASSERT(*((uint16 *)fstack_peek(&s)) == 7);
  slot = fstack_push(&s);
  *slot = 9;
  TEST_ASSERT(fstack_get_count(&s) == 4);
  TEST_ASSERT(*((uint16 *)fstack_peek(&s)) == 9);

  TEST_ASSERT(*((uint16 *)fstack_pop(&s)) == 9);
  TEST_ASSERT(*((uint16 *)fstack_peek(&s)) == 7);
  TEST_ASSERT(*((uint16 *)fstack_pop(&s)) == 7);
  TEST_ASSERT(*((uint16 *)fstack_peek(&s)) == 5);
  TEST_ASSERT(*((uint16 *)fstack_pop(&s)) == 5);
  TEST_ASSERT(*((uint16 *)fstack_peek(&s)) == 4);
  TEST_ASSERT(*((uint16 *)fstack_pop(&s)) == 4);

  fstack_deinit(&s);
  return NULL;
}
