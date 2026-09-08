#pragma once
#include "../src/allocators.c"
#include "../src/commons.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ASSERT(condition, comment)                                                                                                                                                                \
  do {                                                                                                                                                                                                 \
    if (!(condition)) {                                                                                                                                                                                \
      return (comment);                                                                                                                                                                                \
    }                                                                                                                                                                                                  \
  } while (0)

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

void *test_malloc_with_counter(size_t size) {
  if (test_allocations_before_failure < 1) {
    reset_malloc_calloc();
    return NULL;
  }
  test_allocations_before_failure--;
  return malloc(size);
}

void *test_calloc_with_counter(size_t count, size_t size) {
  if (test_allocations_before_failure < 1) {
    reset_malloc_calloc();
    return NULL;
  }
  test_allocations_before_failure--;
  return calloc(count, size);
}

void set_malloc_to_fail_after(int allocations_count) {
  test_allocations_before_failure = allocations_count;
  set_malloc_calloc(test_malloc_with_counter, test_calloc_with_counter);
}