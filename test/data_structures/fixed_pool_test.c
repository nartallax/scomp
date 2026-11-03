#pragma once
#include "../../src/data_structures/fixed_pool.c"
#include "../test_utils.c"

const char *test_fixed_pool_simple() {
  fixed_pool p;
  fpool_init(&p, test_context, sizeof(uint64), 4);
  TEST_ASSERT(fpool_has_space(&p));
  uint64 *slot_a = fpool_acquire(&p);
  TEST_ASSERT(fpool_get_offset(&p, slot_a) == 0);
  TEST_ASSERT(fpool_offset_to_value(&p, 0) == slot_a);
  *slot_a = 111;
  uint64 *slot_b = fpool_acquire(&p);
  *slot_b = 222;
  TEST_ASSERT(fpool_get_offset(&p, slot_b) == 1);
  TEST_ASSERT(fpool_offset_to_value(&p, 1) == slot_b);

  TEST_ASSERT(fpool_has_space(&p));
  fpool_release(&p, slot_a);
  slot_a = fpool_acquire(&p);
  *slot_a = 333;
  TEST_ASSERT(fpool_get_offset(&p, slot_a) == 0);
  TEST_ASSERT(fpool_offset_to_value(&p, 0) == slot_a);

  uint64 *slot_c = fpool_acquire(&p);
  *slot_c = 444;
  TEST_ASSERT(fpool_get_offset(&p, slot_c) == 2);
  TEST_ASSERT(fpool_offset_to_value(&p, 2) == slot_c);

  TEST_ASSERT(fpool_has_space(&p));
  uint64 *slot_d = fpool_acquire(&p);
  *slot_d = 111;
  TEST_ASSERT(fpool_get_offset(&p, slot_d) == 3);
  TEST_ASSERT(fpool_offset_to_value(&p, 3) == slot_d);
  TEST_ASSERT(!fpool_has_space(&p));

  fpool_deinit(&p);

  return NULL;
}