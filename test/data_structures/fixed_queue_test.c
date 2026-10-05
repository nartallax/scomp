#pragma once
#include "../../src/data_structures/fixed_queue.c"
#include "../test_utils.c"

const char *test_fixed_queue_simple() {
  fixed_queue q;
  TEST_ASSERT(fqueue_init(&q, test_context, sizeof(int), 2));

  TEST_ASSERT(fqueue_get_capacity(&q) == 3);
  TEST_ASSERT(fqueue_get_count(&q) == 0);

  int *slot = fqueue_push(&q);
  TEST_ASSERT(fqueue_get_count(&q) == 1);
  *slot = 5;
  slot = fqueue_push(&q);
  TEST_ASSERT(fqueue_get_count(&q) == 2);
  *slot = 6;
  slot = fqueue_pop(&q);
  TEST_ASSERT(fqueue_get_count(&q) == 1);
  TEST_ASSERT(*slot == 5);
  slot = fqueue_pop(&q);
  TEST_ASSERT(fqueue_get_count(&q) == 0);
  TEST_ASSERT(*slot == 6);

  slot = fqueue_push(&q);
  TEST_ASSERT(fqueue_get_count(&q) == 1);
  *slot = 7;
  slot = fqueue_push(&q);
  TEST_ASSERT(fqueue_get_count(&q) == 2);
  *slot = 8;
  slot = fqueue_push(&q);
  TEST_ASSERT(fqueue_get_count(&q) == 3);
  *slot = 9;

  slot = fqueue_peek(&q);
  TEST_ASSERT(*slot == 7);
  slot = fqueue_peek_tail(&q);
  TEST_ASSERT(*slot == 9);
  slot = fqueue_pop(&q);
  TEST_ASSERT(fqueue_get_count(&q) == 2);
  TEST_ASSERT(*slot == 7);
  slot = fqueue_pop(&q);
  TEST_ASSERT(fqueue_get_count(&q) == 1);
  TEST_ASSERT(*slot == 8);
  slot = fqueue_pop(&q);
  TEST_ASSERT(fqueue_get_count(&q) == 0);
  TEST_ASSERT(*slot == 9);

  fqueue_deinit(&q, test_context);
  return NULL;
}

const char *test_fixed_queue_wrapping() {
  fixed_queue q;
  TEST_ASSERT(fqueue_init(&q, test_context, sizeof(int), 2));

  *(int *)fqueue_push(&q) = 5;
  *(int *)fqueue_push(&q) = 6;
  *(int *)fqueue_push(&q) = 7;
  TEST_ASSERT(*(int *)fqueue_pop(&q) == 5);
  TEST_ASSERT(*(int *)fqueue_pop(&q) == 6);
  TEST_ASSERT(*(int *)fqueue_pop(&q) == 7);
  *(int *)fqueue_push(&q) = 8;
  TEST_ASSERT(*(int *)fqueue_peek(&q) == 8);
  TEST_ASSERT(*(int *)fqueue_peek_tail(&q) == 8);
  *(int *)fqueue_push(&q) = 9;
  TEST_ASSERT(*(int *)fqueue_peek(&q) == 8);
  TEST_ASSERT(*(int *)fqueue_peek_tail(&q) == 9);
  TEST_ASSERT(*(int *)fqueue_pop(&q) == 8);
  TEST_ASSERT(*(int *)fqueue_peek(&q) == 9);
  TEST_ASSERT(*(int *)fqueue_peek_tail(&q) == 9);
  TEST_ASSERT(*(int *)fqueue_pop(&q) == 9);

  fqueue_deinit(&q, test_context);
  return NULL;
}

const char *test_fixed_queue_allocation_failures() {
  fixed_queue q;
  setup_test_context(0);
  TEST_ASSERT(!fqueue_init(&q, test_context, sizeof(int), 2));

  setup_test_context(1);
  TEST_ASSERT(fqueue_init(&q, test_context, sizeof(int), 2));

  fqueue_deinit(&q, test_context);
  return NULL;
}