#pragma once
#include "../commons.c"
#include "../context.c"
#include <stdbool.h>

typedef struct {
  byte *content;
  int64 content_position;
  int64 length_mask;
} ring_buffer;

int64 ring_buffer_get_length(ring_buffer *rb) {
  return rb->length_mask + 1;
}

NODISCARD bool ring_buffer_init(ring_buffer *rb, context *context, int64 length_shift) {
  *rb = (ring_buffer){0};

  int64 length = 1 << length_shift;
  rb->length_mask = length - 1;
  rb->content_position = 0;
  rb->content = context_allocate_zero_init(context, length, sizeof(byte));
  return rb->content != NULL;
}

void ring_buffer_deinit(ring_buffer *rb, context *context) {
  context_free(context, rb->content);

  *rb = (ring_buffer){0};
}

/** Get index of next free byte in the buffer */
INLINE int64 ring_buffer_get_index(ring_buffer *rb) {
  return rb->content_position;
}

INLINE void ring_buffer_push(ring_buffer *rb, byte b) {
  rb->content[rb->content_position] = b;
  rb->content_position = (rb->content_position + 1) & rb->length_mask;
}

/** Returns a byte from the ring buffer.
Index may be bigger than the length of the buffer and will be truncated */
INLINE byte ring_buffer_get(ring_buffer *rb, int64 index) {
  return rb->content[index & rb->length_mask];
}