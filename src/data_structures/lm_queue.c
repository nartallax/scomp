#pragma once
#include "../commons.c"
#include "../context.c"
#include "fixed_array_list.c"
#include "lru_queue.c"

/** LRU-with-maturity queue; aka flush-resistant LRU queue.
Initially values are stored in "fresh" part of the queue;
only when they are referenced again they are put into "mature" part of the queue.
Those two parts of the queue are flushed separately; this way, influx a lot of new unique values cannot flush old values out.

Interface contract from LRU queue (about using list entries as handles) applies here. */
typedef struct {
  fixed_array_list fresh_queue;
  lru_queue mature_queue;
} lm_queue;

void lm_queue_deinit(lm_queue *q) {
  fal_deinit(&q->fresh_queue);
  lru_queue_deinit(&q->mature_queue);

  *q = (lm_queue){0};
}

void lm_queue_reset(lm_queue *q) {
  fal_reset(&q->fresh_queue);
  lru_queue_reset(&q->mature_queue);
}

void lm_queue_init(lm_queue *q, context *context, int64 fresh_size, int64 mature_size) {
  *q = (lm_queue){0};

  fal_init(&q->fresh_queue, context, fresh_size);
  lru_queue_init(&q->mature_queue, context, mature_size);

  lm_queue_reset(q);
}

INLINE bool lm_queue_is_overflowing(lm_queue *q) {
  return lru_queue_is_overflowing(&q->mature_queue) || !fal_has_free_space(&q->fresh_queue);
}

NODISCARD INLINE fal_entry *lm_queue_push(lm_queue *q) {
  assert(!lm_queue_is_overflowing(q));
  return fal_push_head(&q->fresh_queue, NULL);
}

NODISCARD INLINE fal_entry *lm_queue_bump(lm_queue *q, fal_entry *entry) {
  assert(!lm_queue_is_overflowing(q));

  if (fal_is_own_entry(&q->fresh_queue, entry)) {
    // move value from fresh queue to mature queue
    void *value = entry->value;
    fal_remove_entry(&q->fresh_queue, entry);
    fal_entry *result = lru_queue_push(&q->mature_queue);
    result->value = value;
    return result;
  }

  return lru_queue_bump(&q->mature_queue, entry);
}

NODISCARD INLINE void *lm_queue_pop(lm_queue *q) {
  assert(lm_queue_is_overflowing(q));

  if (lru_queue_is_overflowing(&q->mature_queue)) {
    return lru_queue_pop(&q->mature_queue);
  }

  return fal_pop_tail(&q->fresh_queue);
}