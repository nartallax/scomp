#pragma once
#include "../../src/data_structures/fixed_array_list.c"
#include "../test_utils.c"

const char *test_fal_simple() {
  fixed_array_list fal;
  fal_init(&fal, test_context, 4);

  TEST_ASSERT(fal_is_empty(&fal));
  TEST_ASSERT(fal_has_free_space(&fal));
  fal_entry *a = fal_push_head(&fal, "111");
  TEST_ASSERT(fal_is_own_entry(&fal, a));
  TEST_ASSERT(!fal_is_empty(&fal));
  TEST_ASSERT(fal_has_free_space(&fal));
  TEST_ASSERT(strcmp(a->value, "111") == 0);
  fal_remove_entry(&fal, a);
  TEST_ASSERT(fal_is_empty(&fal));
  TEST_ASSERT(fal_has_free_space(&fal));

  a = fal_push_head(&fal, "222");
  fal_entry *b = fal_push_head(&fal, "333");
  fal_entry *c = fal_push_head(&fal, "444");
  fal_entry *d = fal_push_head(&fal, "555");
  TEST_ASSERT(fal_is_own_entry(&fal, a));
  TEST_ASSERT(fal_is_own_entry(&fal, b));
  TEST_ASSERT(fal_is_own_entry(&fal, c));
  TEST_ASSERT(fal_is_own_entry(&fal, d));
  TEST_ASSERT(!fal_has_free_space(&fal));

  fal_entry stray;
  TEST_ASSERT(!fal_is_own_entry(&fal, &stray));

  fal_remove_entry(&fal, c);
  TEST_ASSERT(fal_has_free_space(&fal));
  c = fal_push_head(&fal, "666");
  TEST_ASSERT(fal_is_own_entry(&fal, c));
  fal_remove_entry(&fal, a);
  TEST_ASSERT(strcmp((char *)fal_pop_tail(&fal), "333") == 0);
  TEST_ASSERT(strcmp((char *)fal_pop_tail(&fal), "555") == 0);
  TEST_ASSERT(strcmp((char *)fal_pop_tail(&fal), "666") == 0);
  TEST_ASSERT(fal_is_empty(&fal));

  fal_push_head(&fal, "a");
  fal_push_head(&fal, "b");
  fal_push_head(&fal, "c");
  fal_push_head(&fal, "d");
  fal_reset(&fal);
  fal_push_head(&fal, "e");
  fal_push_head(&fal, "f");

  fal_deinit(&fal);

  return NULL;
}