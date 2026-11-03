#pragma once
#include "../commons.c"
#include "../context.c"
#include "fixed_stack.c"

// TODO: unused?
/** A region of memory, of fixed size, that holds some amount of values of the same type.
This data structure allows to add or remove elements in O(1) */
typedef struct {
  byte *data;
  int64 item_size;
  /** Stack that holds pointer to unoccupied item slots in data array.
  This could be a queue, in theory. But it's not, because
  this pool is used to hold shapes, which are then referenced in the output stream (as offsets from zero).
  We want to keep those offsets as small as possible; for that reason, we want to recycle lower slots as often as possible,
  and stack implicitly does it better than queue.
  TODO: test if it's actually better to have stack than queue */
  fixed_stack free_slots;
} fixed_pool;

void fpool_deinit(fixed_pool *pool) {
  *pool = (fixed_pool){0};
}

void fpool_init(fixed_pool *pool, context *context, int64 item_size, int64 item_count) {
  *pool = (fixed_pool){0};

  pool->data = context_arena_allocate_zero_init(context, item_count, item_size);
  pool->item_size = item_size;
  fstack_init(&pool->free_slots, context, sizeof(void *), item_count);

  for (int64 i = item_count - 1; i >= 0; i--) {
    void **slot = fstack_push(&pool->free_slots);
    *slot = pool->data + (i * item_size);
  }
}

INLINE bool fpool_has_space(fixed_pool *pool) {
  return fstack_get_count(&pool->free_slots) > 0;
}

INLINE void *fpool_acquire(fixed_pool *pool) {
  assert(fpool_has_space(pool) && "Must have space to acquire anything from the pool");
  return *(void **)fstack_pop(&pool->free_slots);
}

void fpool_release(fixed_pool *pool, void *value) {
  assert(fstack_get_count(&pool->free_slots) < fstack_get_capacity(&pool->free_slots) && "Underflow on fixed pool release, you have released more than you have acquired");
  void **slot = fstack_push(&pool->free_slots);
  *slot = value;
}

INLINE int64 fpool_get_offset(fixed_pool *pool, void *value) {
  int64 offset = (((byte *)value) - pool->data) / pool->item_size;
  assert(offset >= 0 && offset < fstack_get_capacity(&pool->free_slots) && "Offset is expected to stay inside the pool, otherwise it's some pointer that doesn't belong in the pool");
  return offset;
}

INLINE void *fpool_offset_to_value(fixed_pool *pool, int64 offset) {
  return &pool->data[offset * pool->item_size];
}