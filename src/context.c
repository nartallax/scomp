#pragma once
#include "commons.c"
#include "data_structures/arena.c"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/** Size of arena context allocates for most of its needs */
// TODO: tune, this is way too much
constexpr memsize CONTEXT_ARENA_SIZE = 32 * 1024 * 1024;

// TODO: error codes
typedef struct {
  char message[512];
  int64 message_length;
  /** False when there's no error */
  bool is_present;
} error;

/** Context contains values that should be global for the compression process.
True global values are not great because of multithreading. */
typedef struct {
  malloc_fn malloc;
  calloc_fn calloc;
  realloc_fn realloc;
  free_fn free;
  error error;
  arena common_arena;
} context;

INLINE void context_free(context *context, void *pointer) {
  context->free(pointer);
}

INLINE bool context_is_errored(context *context) {
  return context->error.is_present;
}

INLINE error *context_get_error(context *context) {
  return &context->error;
}

void context_clear_error(context *context) {
  context->error.is_present = false;
  context->error.message_length = 0;
  context->error.message[0] = '\0';
}

void context_set_error(context *context, const char *format, ...) {
  if (context_is_errored(context)) {
    // don't overwrite the first error, it's usually the most meaningful
    return;
  }

  va_list args;
  va_start(args, format);
  context->error.message_length = vsnprintf(context->error.message, sizeof(context->error.message), format, args);
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

/** Allocate some memory in common arena.
Common arena exists to store all the values the library may want to allocate on init, before any writes or reads.
This works like this because amount of memory for those values is known at compile time; it makes sense to allocate them all at once.
Dynamic values that can grow potentially infinitely (for example, IO buffers and related structures) are allocated on heap via malloc, as usual. */
INLINE void *context_arena_allocate(context *context, memsize element_count, memsize single_element_size) {
  return arena_allocate(&context->common_arena, element_count, single_element_size);
}

INLINE void *context_arena_allocate_zero_init(context *context, memsize element_count, memsize single_element_size) {
  byte *result = arena_allocate(&context->common_arena, element_count, single_element_size);
  for (memsize i = 0; i < element_count * single_element_size; i++) {
    result[i] = 0;
  }
  return result;
}

void _context_set_error_on_malloc(context *context, memsize byte_size) {
  context_set_error(context, "Failed to allocate memory: malloc(%zu)", byte_size);
}

// TODO: never allocate if error is present
INLINE void *context_allocate(context *context, memsize element_count, memsize single_element_size) {
  memsize byte_size = element_count * single_element_size;
  void *result = context->malloc(byte_size);
  if (result == NULL) {
    _context_set_error_on_malloc(context, byte_size);
  }
  return result;
}

INLINE void *context_reallocate(context *context, void *base, memsize new_element_count, memsize single_element_size) {
  memsize byte_size = new_element_count * single_element_size;
  void *result = context->realloc(base, byte_size);
  if (result == NULL) {
    context_set_error(context, "Failed to allocate memory: realloc(%zu, %zu)", base, byte_size);
  }
  return result;
}

// TODO: are we using it? remove if not
INLINE void *context_allocate_zero_init(context *context, memsize element_count, memsize single_element_size) {
  void *result = context->calloc(element_count, single_element_size);
  if (result == NULL) {
    context_set_error(context, "Failed to allocate memory: calloc(%zu, %zu)", element_count, single_element_size);
  }
  return result;
}

void context_deinit(context *c) {
  arena_deinit(&c->common_arena, c->free);

  *c = (context){0};
}

void context_reset(context *c) {
  arena_reset(&c->common_arena);
  context_clear_error(c);
}

// TODO: potentially this library can live without linking to anything at all in runtime
// I need to test it with ldd, and if it does link to something - figure out what and why, and if I could help it
NODISCARD bool context_init(context *c, malloc_fn custom_malloc, calloc_fn custom_calloc, realloc_fn custom_realloc, free_fn custom_free) {
  *c = (context){0};
  c->malloc = custom_malloc;
  c->calloc = custom_calloc;
  c->realloc = custom_realloc;
  c->free = custom_free;

  if (!arena_init(&c->common_arena, custom_malloc, CONTEXT_ARENA_SIZE)) {
    context_deinit(c);
    return false;
  }

  context_reset(c);

  return true;
}