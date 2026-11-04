#pragma once
#include "../../src/data_structures/lm_queue.c"
#include "../test_utils.c"

const char *test_lm_queue_simple() {
  lm_queue q;
  lm_queue_init(&q, test_context, 2, 2);

  fal_entry *a = lm_queue_push(&q);
  TEST_ASSERT(a->value == NULL);
  TEST_ASSERT(!lm_queue_is_overflowing(&q));
  a->value = "aaa";

  fal_entry *b = lm_queue_push(&q);
  TEST_ASSERT(b->value == NULL);
  TEST_ASSERT(lm_queue_is_overflowing(&q));
  b->value = "bbb";

  TEST_ASSERT(strcmp(lm_queue_pop(&q), "aaa") == 0);
  TEST_ASSERT(!lm_queue_is_overflowing(&q));

  b = lm_queue_bump(&q, b);
  TEST_ASSERT(strcmp(b->value, "bbb") == 0);
  b = lm_queue_bump(&q, b);
  TEST_ASSERT(strcmp(b->value, "bbb") == 0);

  fal_entry *c = lm_queue_push(&q);
  TEST_ASSERT(c->value == NULL);
  TEST_ASSERT(!lm_queue_is_overflowing(&q));
  c->value = "ccc";

  fal_entry *d = lm_queue_push(&q);
  TEST_ASSERT(d->value == NULL);
  TEST_ASSERT(lm_queue_is_overflowing(&q));
  d->value = "ddd";
  TEST_ASSERT(strcmp(lm_queue_pop(&q), "ccc") == 0);

  d = lm_queue_bump(&q, d);
  TEST_ASSERT(strcmp(d->value, "ddd") == 0);
  TEST_ASSERT(lm_queue_is_overflowing(&q));
  TEST_ASSERT(strcmp(lm_queue_pop(&q), "bbb") == 0);

  lm_queue_deinit(&q);
  return NULL;
}