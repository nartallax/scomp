#pragma once
#include "context.c"
#include <stddef.h>

const size_t QUEUE_DEFAULT_SIZE = 16;

typedef struct {
  context *context;
  void **values;
  // length of the `values` array. is always power-of-two.
  size_t length;
  size_t head;
  size_t tail;
} queue;

queue *queue_new(context *context) {
  queue *result = context_allocate(context, 1, sizeof(queue));
  if (result == NULL) {
    return result;
  }

  result->length = QUEUE_DEFAULT_SIZE;
  result->head = 0;
  result->tail = 0;
  result->context = context;
  result->values = context_allocate(context, result->length, sizeof(void *));
  if (!result->values) {
    context_free(context, result);
    return NULL;
  }

  return result;
}

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

void _queue_maybe_grow(queue *queue) {
  if (queue_get_count(queue) < queue->length - 1) {
    return;
  }

  size_t new_length = queue->length * 2;
  void **new_values = context_allocate(queue->context, new_length, sizeof(void *));
  if (!new_values) {
    return;
  }

  size_t i = 0;
  for (size_t pointer = queue->head; pointer != queue->tail; pointer = _queue_increment(queue, pointer)) {
    new_values[i] = queue->values[pointer];
    i++;
  }
  context_free(queue->context, queue->values);

  queue->length = new_length;
  queue->values = new_values;
  queue->head = 0;
  queue->tail = i;
}

void queue_push(queue *queue, void *new_value) {
  _queue_maybe_grow(queue);
  queue->values[queue->tail] = new_value;
  queue->tail = _queue_increment(queue, queue->tail);
}

void *queue_pop(queue *queue) {
  if (queue_get_count(queue) < 1) {
    context_set_error(queue->context, "Queue underflow");
    return NULL;
  }

  void *result = queue->values[queue->head];
  queue->head = _queue_increment(queue, queue->head);
  return result;
}

void *queue_peek(queue *queue) {
  if (queue_get_count(queue) < 1) {
    context_set_error(queue->context, "Queue underflow on peek");
    return NULL;
  }

  return queue->values[queue->head];
}