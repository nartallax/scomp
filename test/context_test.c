#pragma once
#include "test_utils.c"

// there's not much to test
// as in, everything is there to test is covered by other tests

const char *test_context_double_error() {
  context_set_error(test_context, "123");
  context_set_error(test_context, "123456");
  TEST_ASSERT(context_get_error(test_context)->message_length == 3);
  return NULL;
}

void *test_malloc_return_null(memsize size) {
  (void)size;
  return NULL;
}

void *test_calloc_return_null(memsize size, memsize count) {
  (void)size;
  (void)count;
  return NULL;
}

const char *test_context_allocation_errors() {
  context c;
  TEST_ASSERT(!context_init(&c, test_malloc_return_null, calloc, realloc, free));
  return NULL;
}

const char *test_context_zero_init() {
  context c;

  TEST_ASSERT(context_init(&c, malloc, calloc, realloc, free));
  byte *b = context_allocate_zero_init(&c, 5, sizeof(byte));
  TEST_ASSERT(b);
  context_free(&c, b);
  context_deinit(&c);

  TEST_ASSERT(context_init(&c, malloc, test_calloc_return_null, realloc, free));
  TEST_ASSERT(!context_is_errored(&c));
  b = context_allocate_zero_init(&c, 5, sizeof(byte));
  TEST_ASSERT(b == NULL);
  TEST_ASSERT(context_is_errored(&c));
  context_deinit(&c);

  return NULL;
}