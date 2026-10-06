#pragma once
#include "../commons.c"
#include "../context.c"
#include <assert.h>

constexpr size_t ARENA_DEFAULT_LENGTH = 1024;

// TODO: do we still need it?
typedef struct {
  context *context;
  byte *data;
  size_t offset;
  size_t length;
  int allocations_count;
} arena;

NODISCARD bool arena_init(arena *a, context *context) {
  *a = (arena){0};

  a->offset = 0;
  a->length = ARENA_DEFAULT_LENGTH;
  a->context = context;
  a->allocations_count = 0;
  a->data = context_allocate(context, a->length, sizeof(byte));
  if (!a->data) {
    return false;
  }
  return true;
}

void arena_deinit(arena *arena, context *context) {
  context_free(context, arena->data);
}

/** Allocates some memory in the arena.
Returns offset. Offset can be turned into pointer with `arena_offset_to_pointer()`.
Offsets are valid until arena resets.

Returns 0 on allocation errors. */
size_t arena_allocate(arena *arena, size_t byte_size) {
  while (arena->offset + byte_size >= arena->length) {
    size_t new_length = arena->length * 2;
    byte *new_data = context_reallocate(arena->context, arena->data, new_length, sizeof(byte));
    if (!new_data) {
      return 0;
    }
    arena->data = new_data;
    arena->length = new_length;
  }

  size_t result = arena->offset;
  arena->offset += byte_size;
  arena->allocations_count++;
  return result + 1;
}

/** Converts offset (see `arena_allocate()`) into pointer of usable memory.
Don't store the pointer between `arena_allocate()` calls, as it may be invalidated on arena growth. */
void *arena_offset_to_pointer(arena *arena, size_t offset) {
  assert(offset > 0 && "Zero offset is invalid");
  return arena->data + (offset - 1);
}

/** Notify arena that some offset is no longer required.
Arena resets once all the allocations are gone.
By the nature of an arena, it doesn't need to know which allocation is being free'd, as long as amount of free-s matches amount of allocations. */
void arena_free(arena *arena) {
  arena->allocations_count--;
  assert(arena->allocations_count >= 0);
  if (arena->allocations_count == 0) {
    arena->offset = 0;
  }
}