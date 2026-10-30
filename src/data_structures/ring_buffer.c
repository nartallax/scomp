#pragma once
#include "../commons.c"
#include "../context.c"
#include <stdbool.h>
#include <string.h>

typedef struct {
  byte *content;
  int64 content_position;
  int64 length_mask;
} ring_buffer;

INLINE int64 ring_buffer_get_length(ring_buffer *rb) {
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

/** Fills the ring buffer from the data. Returns amount of bytes consumed.
Won't consume more than one length of the buffer. */
NODISCARD INLINE int64 ring_buffer_fill(ring_buffer *rb, const byte *data, int64 data_length) {
  int64 available_forward_space = ring_buffer_get_length(rb) - rb->content_position;
  int64 first_copy_amount = available_forward_space < data_length ? available_forward_space : data_length;
  memcpy(rb->content + rb->content_position, data, first_copy_amount);
  rb->content_position = (rb->content_position + first_copy_amount) & rb->length_mask;

  int64 remaining_amount = data_length - first_copy_amount;
  int64 remaining_space = ring_buffer_get_length(rb) - first_copy_amount;
  int64 second_copy_amount = remaining_amount < remaining_space ? remaining_amount : remaining_space;
  if (second_copy_amount > 0) {
    memcpy(rb->content + rb->content_position, data + first_copy_amount, second_copy_amount);
    rb->content_position = (rb->content_position + second_copy_amount) & rb->length_mask;
  }

  return first_copy_amount + second_copy_amount;
}