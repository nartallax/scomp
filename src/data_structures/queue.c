#pragma once
#include "../commons.c"
#include "../context.c"
#include <assert.h>
#include <stddef.h>
#include <string.h>

/** A queue data structure.
Can store data of arbitrary size (passed in constructor).
Because of that, its api does not operate with values themselves, but rather with pointers to those values. */
typedef struct {
  context *context;
  /** Size of one element. */
  int64 value_size;
  /** Storage for queue's contents.
  It's typed as byte array to make pointer arithmetics easier. */
  byte *values;
  // length of the `values` array, in elements. is always power-of-two.
  int64 length;
  int64 length_mask;
  // head is the last occupied spot in values array
  int64 head;
  // tail is first free spot in values array
  int64 tail;
} queue;

NODISCARD bool queue_init(queue *q, context *context, int64 value_size, int64 starting_length_shift) {
  *q = (queue){0};

  q->length = 1 << starting_length_shift;
  q->length_mask = q->length - 1;
  q->head = 0;
  q->tail = 0;
  q->context = context;
  q->value_size = value_size;
  q->values = context_allocate(context, q->length, value_size);

  return q->values != NULL;
}

void queue_reset(queue *q) {
  q->head = 0;
  q->tail = 0;
}

/** If the items in the queue are heap-allocated, delete them manually first */
void queue_deinit(queue *q, context *context) {
  context_free(context, q->values);

  *q = (queue){0};
}

/** Returns amount of items in queue. */
INLINE int64 queue_get_count(queue *queue) {
  return queue->head <= queue->tail ? queue->tail - queue->head : queue->tail + queue->length - queue->head;
}

INLINE int64 _queue_increment(queue *queue, int64 i) {
  return (i + 1) & queue->length_mask;
}

NODISCARD bool _queue_maybe_grow(queue *queue) {
  int64 count = queue_get_count(queue);
  if (count < queue->length - 1) {
    return true;
  }

  int64 new_length = queue->length * 2;
  byte *new_values = context_allocate(queue->context, new_length, queue->value_size);
  if (!new_values) {
    return false;
  }

  int64 i = 0;
  int64 qsize = queue->value_size;
  // TODO: cringe. use realloc and move only segment that touches the end of the array
  for (int64 pointer = queue->head; pointer != queue->tail; pointer = _queue_increment(queue, pointer)) {
    for (int64 byte_pointer = 0; byte_pointer < queue->value_size; byte_pointer++) {
      new_values[(i * qsize) + byte_pointer] = queue->values[(pointer * qsize) + byte_pointer];
    }
    i++;
  }
  context_free(queue->context, queue->values);

  queue->length = new_length;
  queue->length_mask = queue->length - 1;
  queue->values = new_values;
  queue->head = 0;
  queue->tail = i;
  return true;
}

INLINE void *_queue_pointer_at(queue *queue, int64 index) {
  return queue->values + (index * queue->value_size);
}

/** Returns pointer to the next free space in the queue. This space is now considered occupied.
It's up for the caller to fill this space with actual values. */
INLINE void *queue_push(queue *queue) {
  if (!_queue_maybe_grow(queue)) {
    return NULL;
  }
  void *result_pointer = _queue_pointer_at(queue, queue->tail);
  queue->tail = _queue_increment(queue, queue->tail);
  return result_pointer;
}

/** Returns pointer to the next element in the queue. This element is now considered consumed, and removed from the queue.
It's up for the caller to `free()` the item, if needed. */
INLINE void *queue_pop(queue *queue) {
  assert(queue_get_count(queue) > 0 && "Queue underflow on pop()");
  void *result = _queue_pointer_at(queue, queue->head);
  queue->head = _queue_increment(queue, queue->head);
  return result;
}

/** Returns pointer to the element of the queue `offset` positions back from the head, 0 being the head */
INLINE void *queue_peek_at(queue *queue, int64 offset) {
  assert(queue_get_count(queue) > offset && "Queue should not underflow on peek_at()");
  int64 index = (queue->head + offset) & queue->length_mask;
  return _queue_pointer_at(queue, index);
}

/** Returns pointer to the next element in the queue without removing it. */
INLINE void *queue_peek(queue *queue) {
  return queue_peek_at(queue, 0);
}

INLINE void *queue_peek_tail_at(queue *queue, int64 offset) {
  assert(queue_get_count(queue) > offset && "Queue should not underflow on peek_tail_at()");
  int64 index = ((queue->tail + queue->length - 1) - offset) & queue->length_mask;
  return _queue_pointer_at(queue, index);
}

INLINE void *queue_peek_tail(queue *queue) {
  return queue_peek_tail_at(queue, 0);
}