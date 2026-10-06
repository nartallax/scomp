#pragma once
#include "../commons.c"
#include "../context.c"
#include <assert.h>

/** Queue of a fixed size. */
typedef struct {
  /** Size of one element. */
  size_t value_size;
  /** Storage for queue's contents.
  It's typed as byte array to make pointer arithmetics easier. */
  byte *values;
  size_t length_mask;
  size_t head;
  size_t tail;
} fixed_queue;

void fqueue_deinit(fixed_queue *q, context *context) {
  context_free(context, q->values);
}

void fqueue_reset(fixed_queue *q) {
  q->head = 0;
  q->tail = 0;
}

NODISCARD bool fqueue_init(fixed_queue *q, context *context, size_t value_size, size_t length_shift) {
  *q = (fixed_queue){0};

  size_t length = 1 << length_shift;
  q->length_mask = length - 1;
  q->head = 0;
  q->tail = 0;
  q->values = context_allocate(context, length, value_size);
  q->value_size = value_size;
  return !!q->values;
}

void *fqueue_push(fixed_queue *q) {
  void *result_pointer = q->values + (q->tail * q->value_size);
  q->tail = (q->tail + 1) & q->length_mask;
  assert(q->tail != q->head && "Fixed-size queue overflow");
  return result_pointer;
}

void *fqueue_pop(fixed_queue *q) {
  assert(q->tail != q->head && "Fixed-size queue underflow: cannot fqueue_pop() on empty queue");
  void *result_pointer = q->values + (q->head * q->value_size);
  q->head = (q->head + 1) & q->length_mask;
  return result_pointer;
}

void *fqueue_peek(fixed_queue *q) {
  assert(q->tail != q->head && "Fixed-size queue underflow: cannot fqueue_peek() on empty queue");
  return q->values + (q->head * q->value_size);
}

void *fqueue_peek_tail(fixed_queue *q) {
  assert(q->tail != q->head && "Fixed-size queue underflow: cannot fqueue_peek_tail() on empty queue");
  // it's q->tail - 1, but without underflows
  size_t last_meaningful_value_index = (q->tail + q->length_mask) & q->length_mask;
  return q->values + (last_meaningful_value_index * q->value_size);
}

/** Get total capacity of the queue.
Queue can hold at most this much values. */
size_t fqueue_get_capacity(fixed_queue *q) {
  return q->length_mask;
}

/** Return amount of values in the queue at this moment. */
size_t fqueue_get_count(fixed_queue *q) {
  // the same trick as above, subtraction without overflows
  return (q->tail + q->length_mask + 1 - q->head) & q->length_mask;
}