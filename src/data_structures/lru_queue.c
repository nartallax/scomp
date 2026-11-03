#pragma once
#include "../commons.c"
#include "../context.c"
#include "fixed_array_list.c"

/** Least Recently Used queue, a base for LRU cache. */
typedef struct {
  fixed_array_list queue;
} lru_queue;

void lru_queue_deinit(lru_queue *q) {
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

/** Put a new value in the queue.
Returns an entry. This entry can be used to bump the value later.
This entry also indicates if new value needs to be allocated (result.value == null),
or if existing one was kicked out of the cache and the memory is expected to be reused (result.value != null, pointer to that kicked out entry) */
NODISCARD INLINE fal_entry *lru_queue_push(lru_queue *q) {
  if (fal_has_free_space(&q->queue)) {
    return fal_push_head(&q->queue, NULL);
  }

  void *old_value = fal_pop_tail(&q->queue);
  return fal_push_head(&q->queue, old_value);
}

/** Bumps value back to top. Entry pointer will change and is expected to be saved. */
NODISCARD INLINE fal_entry *lru_queue_bump(lru_queue *q, fal_entry *entry) {
  void *value = entry->value;
  fal_remove_entry(&q->queue, entry);
  return fal_push_head(&q->queue, value);
}