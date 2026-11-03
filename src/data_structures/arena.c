#pragma once
#include "../commons.c"
#include <assert.h>

typedef struct {
  byte *data;
  memsize offset;
  memsize length;
} arena;

void arena_deinit(arena *a, free_fn _free) {
  _free(a->data);

  *a = (arena){0};
}

NODISCARD bool arena_init(arena *a, malloc_fn _malloc, memsize length) {
  *a = (arena){0};

  a->offset = 0;
  a->length = length;
  a->data = _malloc(a->length * sizeof(byte));
  if (a->data == NULL) {
    *a = (arena){0};
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
