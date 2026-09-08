#pragma once
#include "./error.c"
#include <stddef.h>
#include <stdlib.h>

typedef void *(*malloc_fn)(size_t);
typedef void *(*calloc_fn)(size_t, size_t);

/** Allocator substitutions, for tests.
Or for running the library in an environment that provides its own allocators, like nginx. */
malloc_fn allocators_malloc = NULL;
calloc_fn allocators_calloc = NULL;

/** Sets malloc and calloc functions through which everything in the library allocates memory.
It only makes sense to set both at the same time; when either function is unset, default implementation is used. */
void set_malloc_calloc(malloc_fn mlc, calloc_fn clc) {
  allocators_malloc = mlc;
  allocators_calloc = clc;
}

/** Use default implementations of malloc and calloc. */
void reset_malloc_calloc() {
  set_malloc_calloc(NULL, NULL);
}

void *allocate(size_t element_count, size_t single_element_size) {
  size_t byte_size = element_count * single_element_size;
  void *result;
  if (allocators_malloc != NULL) {
    result = allocators_malloc(byte_size);
  } else {
    result = malloc(byte_size);
  }
  if (result == NULL) {
    error_set("Failed to allocate memory: malloc(%zu)", byte_size);
  }
  return result;
}

void *allocate_zero_init(size_t element_count, size_t single_element_size) {
  void *result;
  if (allocators_calloc != NULL) {
    result = allocators_calloc(element_count, single_element_size);
  } else {
    result = calloc(element_count, single_element_size);
  }
  if (result == NULL) {
    error_set("Failed to allocate memory: calloc(%zu, %zu)", element_count, single_element_size);
  }
  return result;
}