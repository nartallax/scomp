#pragma once
#include "../commons.c"
#include "../context.c"
#include <assert.h>

typedef struct {
  byte *data;
  memsize offset;
  memsize length;
} arena;

void arena_deinit(arena *a, context *context) {
  context_free(context, a->data);

  *a = (arena){0};
}

NODISCARD bool arena_init(arena *a, context *context, memsize length) {
  *a = (arena){0};

  a->offset = 0;
  a->length = length;
  a->data = context_allocate(context, a->length, sizeof(byte));
  if (a->data == NULL) {
    arena_deinit(a, context);
    return false;
  }

  return true;
}

void *arena_allocate(arena *arena, int64 element_count, memsize byte_size) {
  void *result = arena->data + arena->offset;

  memsize allocation_size = element_count * byte_size;
  assert(arena->offset + allocation_size < arena->length && "Arena overflow. Create bigger arena.");
  arena->offset += allocation_size;

  return result;
}
