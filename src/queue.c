#pragma once
#include "commons.c"
#include "context.c"
#include <assert.h>
#include <stddef.h>
#include <string.h>

// #define it?
const size_t QUEUE_DEFAULT_SIZE = 16;

/** A queue data structure.
Can store data of arbitrary size (passed in constructor).
Because of that, its api does not operate with values themselves, but rather with pointers to those values. */
typedef struct {
  context *context;
  /** Size of one element. */
  size_t value_size;
  /** Storage for queue's contents.
  It's typed as byte array to make pointer arithmetics easier. */
  byte *values;
  // length of the `values` array, in elements. is always power-of-two.
  size_t length;
  // head is the last occupied spot in values array
  size_t head;
  // tail is first free spot in values array
  size_t tail;
} queue;

queue *queue_new(context *context, size_t value_size) {
  queue *result = context_allocate(context, 1, sizeof(queue));
  if (result == NULL) {
    return result;
  }

  result->length = QUEUE_DEFAULT_SIZE;
  result->head = 0;
  result->tail = 0;
  result->context = context;
  result->value_size = value_size;
  result->values = context_allocate(context, result->length, value_size);
  if (!result->values) {
    context_free(context, result);
    return NULL;
  }

  return result;
}

/** If the items in the queue are heap-allocated, delete them manually first */
void queue_delete(queue *queue) {
  context_free(queue->context, queue->values);
  context_free(queue->context, queue);
}

/** Returns amount of items in queue. */
size_t queue_get_count(queue *queue) {
  return queue->head <= queue->tail ? queue->tail - queue->head : queue->tail + queue->length - queue->head;
}

size_t _queue_increment(queue *queue, size_t i) {
  return (i + 1) & (queue->length - 1);
}

bool _queue_maybe_grow(queue *queue) {
  if (queue_get_count(queue) < queue->length - 1) {
    return true;
  }

  size_t new_length = queue->length * 2;
  byte *new_values = context_allocate(queue->context, new_length, queue->value_size);
  if (!new_values) {
    return false;
  }

  size_t i = 0;
  size_t qsize = queue->value_size;
  for (size_t pointer = queue->head; pointer != queue->tail; pointer = _queue_increment(queue, pointer)) {
    for (size_t byte_pointer = 0; byte_pointer < queue->value_size; byte_pointer++) {
      new_values[(i * qsize) + byte_pointer] = queue->values[(pointer * qsize) + byte_pointer];
    }
    i++;
  }
  context_free(queue->context, queue->values);

  queue->length = new_length;
  queue->values = new_values;
  queue->head = 0;
  queue->tail = i;
  return true;
}

/** Returns pointer to the next free space in the queue. This space is now considered occupied.
It's up for the caller to fill this space with actual values. */
void *queue_push(queue *queue) {
  if (!_queue_maybe_grow(queue)) {
    return NULL;
  }
  void *result_pointer = queue->values + (queue->tail * queue->value_size);
  queue->tail = _queue_increment(queue, queue->tail);
  return result_pointer;
}

/** Returns pointer to the next element in the queue. This element is now considered consumed, and removed from the queue.
It's up for the caller to `free()` the item, if needed. */
void *queue_pop(queue *queue) {
  assert(queue_get_count(queue) > 0 && "Queue should not underflow");
  void *result = queue->values + (queue->head * queue->value_size);
  queue->head = _queue_increment(queue, queue->head);
  return result;
}

/** Returns pointer to the next element in the queue without removing it. */
void *queue_peek(queue *queue) {
  assert(queue_get_count(queue) > 0 && "Queue should not underflow on peek");
  return queue->values + (queue->head * queue->value_size);
}

void *queue_peek_tail(queue *queue) {
  assert(queue_get_count(queue) > 0 && "Queue should not underflow on tail peek");
  return queue->values + ((queue->tail - 1) * queue->value_size);
}