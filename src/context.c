#pragma once
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef void *(*malloc_fn)(size_t);
typedef void *(*calloc_fn)(size_t, size_t);
typedef void *(*realloc_fn)(void *, size_t);
typedef void (*free_fn)(void *);

#define ERROR_MESSAGE_LENGTH 512

typedef struct {
  char message[ERROR_MESSAGE_LENGTH];
  size_t message_length;
  /** False when there's no error */
  bool is_present;
} error;

// TODO: we need to rethink our approach to handling context failures
// let's accept a function that would be called on error instead

/** Context contains values that should be global for the compression process.
True global values are not great because of multithreading. */
typedef struct {
  malloc_fn malloc;
  calloc_fn calloc;
  realloc_fn realloc;
  free_fn free;
  error error;
} context;

// TODO: potentially this library can live without linking to anything at all in runtime
// I need to test it with ldd, and if it does link to something - figure out what and why, and if I could help it
context *context_new(malloc_fn custom_malloc, calloc_fn custom_calloc, realloc_fn custom_realloc, free_fn custom_free) {
  context *result = custom_malloc(sizeof(context));
  result->malloc = custom_malloc;
  result->calloc = custom_calloc;
  result->realloc = custom_realloc;
  result->free = custom_free;
  result->error.is_present = false;
  result->error.message_length = 0;
  return result;
}

void context_free(context *context, void *pointer) {
  context->free(pointer);
}

void context_delete(context *context) {
  context_free(context, context);
}

bool context_is_errored(context *context) {
  return context->error.is_present;
}

error *context_get_error(context *context) {
  return &context->error;
}

void context_clear_error(context *context) {
  context->error.is_present = false;
  context->error.message_length = 0;
}

void context_set_error(context *context, const char *format, ...) {
  if (context_is_errored(context)) {
    // don't overwrite the first error, it's usually the most meaningful
    return;
  }

  va_list args;
  va_start(args, format);
  context->error.message_length = vsnprintf(context->error.message, ERROR_MESSAGE_LENGTH, format, args);
  context->error.is_present = true;
  va_end(args);
}

/** Sets malloc and calloc functions through which everything in the library allocates memory.
It only makes sense to set both at the same time; when either function is unset, default implementation is used. */
void context_set_allocators(context *context, malloc_fn mlc, calloc_fn clc, realloc_fn rlc, free_fn fre) {
  context->malloc = mlc;
  context->calloc = clc;
  context->realloc = rlc;
  context->free = fre;
}

void *context_allocate(context *context, size_t element_count, size_t single_element_size) {
  size_t byte_size = element_count * single_element_size;
  void *result = context->malloc(byte_size);
  if (result == NULL) {
    context_set_error(context, "Failed to allocate memory: malloc(%zu)", byte_size);
  }
  return result;
}

void *context_reallocate(context *context, void *base, size_t new_element_count, size_t single_element_size) {
  size_t byte_size = new_element_count * single_element_size;
  void *result = context->realloc(base, byte_size);
  if (result == NULL) {
    context_set_error(context, "Failed to allocate memory: realloc(%zu, %zu)", base, byte_size);
  }
  return result;
}

void *context_allocate_zero_init(context *context, size_t element_count, size_t single_element_size) {
  void *result = context->calloc(element_count, single_element_size);
  if (result == NULL) {
    context_set_error(context, "Failed to allocate memory: calloc(%zu, %zu)", element_count, single_element_size);
  }
  return result;
}