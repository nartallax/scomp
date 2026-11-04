#pragma once
#include "../commons.c"
#include "../context.c"
#include "fixed_array_list.c"

/** Least Recently Used queue, a base for LRU cache. */
typedef struct {
  fixed_array_list queue;
} lru_queue;

void lru_queue_deinit(lru_queue *q) {
  fal_deinit(&q->queue);

  *q = (lru_queue){0};
}

void lru_queue_reset(lru_queue *q) {
  fal_reset(&q->queue);
}

void lru_queue_init(lru_queue *q, context *c, int64 size) {
  *q = (lru_queue){0};

  fal_init(&q->queue, c, size);
  lru_queue_reset(q);
}

/** If true - then the queue is full, and you should remove some elements from it. */
INLINE bool lru_queue_is_overflowing(lru_queue *q) {
  return !fal_has_free_space(&q->queue);
}

/** Remove the least worthy element from the queue. */
NODISCARD INLINE void *lru_queue_pop(lru_queue *q) {
  assert(!fal_is_empty(&q->queue));
  return fal_pop_tail(&q->queue);
}

/** Put a new value in the queue.
Returns an entry. This entry can be used to bump the value later.
Expects that you have properly drained queue overflow before doing this. */
NODISCARD INLINE fal_entry *lru_queue_push(lru_queue *q) {
  assert(!lru_queue_is_overflowing(q));
  return fal_push_head(&q->queue, NULL);
}

/** Bumps value back to top. Entry pointer will change and is expected to be saved. */
NODISCARD INLINE fal_entry *lru_queue_bump(lru_queue *q, fal_entry *entry) {
  void *value = entry->value;
  fal_remove_entry(&q->queue, entry);
  return fal_push_head(&q->queue, value);
}