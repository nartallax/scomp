#pragma once
#include "commons.c"
#include "context.c"
#include <stdbool.h>

typedef struct {
  context *context;
  byte *content;
  size_t content_position;
  size_t length_mask;
} ring_buffer;

NODISCARD bool ring_buffer_init(ring_buffer *rb, context *context, size_t length_shift) {
  *rb = (ring_buffer){0};

  rb->context = context;
  size_t length = (size_t)1 << length_shift;
  rb->length_mask = length - 1;
  rb->content_position = 0;
  rb->content = context_allocate_zero_init(context, length, sizeof(byte));
  return !!rb->content;
}

void ring_buffer_deinit(ring_buffer *rb) {
  context_free(rb->context, rb->content);
}

/** Get index of next free byte in the buffer */
size_t ring_buffer_get_index(ring_buffer *rb) {
  return rb->content_position;
}

void ring_buffer_push(ring_buffer *rb, byte b) {
  rb->content[rb->content_position] = b;
  rb->content_position = (rb->content_position + 1) & rb->length_mask;
}

/** Returns a byte from the ring buffer.
Index may be bigger than the length of the buffer and will be truncated */
byte ring_buffer_get(ring_buffer *rb, size_t index) {
  return rb->content[index & rb->length_mask];
}