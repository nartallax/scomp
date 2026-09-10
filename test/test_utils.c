#pragma once
#include "../src/commons.c"
#include "../src/context.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ASSERT(condition, comment)                                                                                                                                                                \
  do {                                                                                                                                                                                                 \
    if (!(condition)) {                                                                                                                                                                                \
      return (comment);                                                                                                                                                                                \
    }                                                                                                                                                                                                  \
  } while (0)

// TODO: rm?
void merge_byte_arrays(byte **receiver, size_t *receiver_length, byte *b, size_t b_len) {
  byte *old_receiver = *receiver;
  byte *result = malloc(sizeof(byte) * (*receiver_length + b_len));
  memcpy(result, *receiver, *receiver_length);
  memcpy(result + *receiver_length, b, b_len);
  *receiver = result;
  *receiver_length += b_len;
  free(old_receiver);
}

int test_allocations_before_failure = 0;
context *test_context = NULL;

void _test_reset_allocators() {
  context_set_allocators(test_context, malloc, calloc, free);
}

void *test_malloc_with_counter(size_t size) {
  if (test_allocations_before_failure < 1) {
    _test_reset_allocators();
    return NULL;
  }
  test_allocations_before_failure--;
  return malloc(size);
}

void *test_calloc_with_counter(size_t count, size_t size) {
  if (test_allocations_before_failure < 1) {
    _test_reset_allocators();
    return NULL;
  }
  test_allocations_before_failure--;
  return calloc(count, size);
}

void free_test_context() {
  if (test_context) {
    context_delete(test_context);
    test_context = NULL;
  }
}

void setup_test_context(int allocations_before_failure_count) {
  free_test_context();
  malloc_fn mlc = malloc;
  calloc_fn clc = calloc;

  if (allocations_before_failure_count >= 0) {
    mlc = test_malloc_with_counter;
    clc = test_calloc_with_counter;
    // +1 for the context allocation itself
    test_allocations_before_failure = allocations_before_failure_count + 1;
  }

  test_context = context_new(mlc, clc, free);
}

void setup_test_context_default() {
  setup_test_context(-1);
}