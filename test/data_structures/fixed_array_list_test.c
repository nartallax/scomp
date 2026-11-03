#pragma once
#include "../../src/data_structures/fixed_array_list.c"
#include "../test_utils.c"

const char *test_fal_simple() {
  fixed_array_list fal;
  fal_init(&fal, test_context, 4);

  TEST_ASSERT(fal_is_empty(&fal));
  TEST_ASSERT(fal_has_free_space(&fal));
  fal_entry *a = fal_push_head(&fal, 111);
  TEST_ASSERT(!fal_is_empty(&fal));
  TEST_ASSERT(fal_has_free_space(&fal));
  TEST_ASSERT(a->value == 111);
  fal_remove_entry(&fal, a);
  TEST_ASSERT(fal_is_empty(&fal));
  TEST_ASSERT(fal_has_free_space(&fal));

  a = fal_push_head(&fal, 222);
  fal_push_head(&fal, 333);
  fal_entry *c = fal_push_head(&fal, 444);
  fal_push_head(&fal, 555);
  TEST_ASSERT(!fal_has_free_space(&fal));
  fal_remove_entry(&fal, c);
  TEST_ASSERT(fal_has_free_space(&fal));
  c = fal_push_head(&fal, 666);
  fal_remove_entry(&fal, a);
  TEST_ASSERT(fal_pop_tail(&fal)->value == 333);
  TEST_ASSERT(fal_pop_tail(&fal)->value == 555);
  TEST_ASSERT(fal_pop_tail(&fal)->value == 666);
  TEST_ASSERT(fal_is_empty(&fal));
  fal_deinit(&fal);

  return NULL;
}