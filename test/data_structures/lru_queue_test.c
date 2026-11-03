#pragma once
#include "../../src/data_structures/lru_queue.c"
#include "../test_utils.c"

const char *test_lru_queue_simple() {
  lru_queue q;
  lru_queue_init(&q, test_context, 3);

  fal_entry *a = lru_queue_push(&q);
  TEST_ASSERT(a->value == NULL);
  a->value = "aaa";

  fal_entry *b = lru_queue_push(&q);
  TEST_ASSERT(b->value == NULL);
  b->value = "bbb";

  fal_entry *c = lru_queue_push(&q);
  TEST_ASSERT(c->value == NULL);
  c->value = "ccc";

  TEST_ASSERT(a != b);
  TEST_ASSERT(b != c);
  TEST_ASSERT(a != c);
  b = lru_queue_bump(&q, b);
  TEST_ASSERT(a != b);
  TEST_ASSERT(b != c);
  TEST_ASSERT(a != c);
  TEST_ASSERT(strcmp(a->value, "aaa") == 0);
  TEST_ASSERT(strcmp(b->value, "bbb") == 0);
  TEST_ASSERT(strcmp(c->value, "ccc") == 0);

  fal_entry *d = lru_queue_push(&q);
  TEST_ASSERT(strcmp(d->value, "aaa") == 0);
  d->value = "ddd";

  fal_entry *e = lru_queue_push(&q);
  TEST_ASSERT(strcmp(e->value, "ccc") == 0);
  e->value = "eee";
  TEST_ASSERT(strcmp(b->value, "bbb") == 0);
  TEST_ASSERT(strcmp(d->value, "ddd") == 0);
  TEST_ASSERT(strcmp(e->value, "eee") == 0);

  lru_queue_deinit(&q);
  return NULL;
}