#pragma once

#include "commons.c"
#include "context.c"
#include "queue.c"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/** Writer is a structure that receives bytes and bits from various producers.
Received data is stored inside the structure, and then can be consumed with `writer_consume_...()` methods.
Bits are written first in lowest-value bit of a byte.
Buffers can be reused via `writer_supply_...()` methods. */
typedef struct {
  context *context;
  queue *buffers;
  size_t current_bit_index;
  size_t size;
  queue *free_buffers;
} writer;

typedef struct {
  byte *data;
  size_t length;
} buffer;

const buffer EMPTY_BUFFER = (buffer){.data = NULL, .length = 0};

void _writer_allocate_next_buffer(writer *writer) {
  byte *buffer;
  if (queue_get_count(writer->free_buffers) > 0) {
    buffer = queue_pop(writer->free_buffers);
  } else {
    buffer = context_allocate_zero_init(writer->context, writer->size, sizeof(byte));
    if (!buffer) {
      return;
    }
  }
  queue_push(writer->buffers, buffer);
  writer->current_bit_index = 0;
}

void _writer_maybe_allocate_next_buffer(writer *writer) {
  if ((writer->current_bit_index >> 3) >= writer->size) {
    _writer_allocate_next_buffer(writer);
  }
}

size_t writer_get_bytes_stored(writer *writer) {
  int buffer_count = (int)queue_get_count(writer->buffers);
  if (buffer_count > 0) {
    // there should always be at least 1 buffer
    // except for case when allocation for the first buffer failed
    // to avoid underflow, this condition exists
    buffer_count--;
  }
  return (buffer_count * writer->size) + ((writer->current_bit_index + 7) >> 3);
}

void writer_delete(writer *writer) {
  size_t bytes_stored = writer_get_bytes_stored(writer);
  if (bytes_stored > 0) {
    context_set_error(writer->context, "Writer is deleted while still having %zu non-consumed bytes", bytes_stored);
  }

  while (queue_get_count(writer->buffers) > 0) {
    context_free(writer->context, queue_pop(writer->buffers));
  }
  queue_delete(writer->buffers);

  while (queue_get_count(writer->free_buffers) > 0) {
    context_free(writer->context, queue_pop(writer->free_buffers));
  }
  queue_delete(writer->free_buffers);

  context_free(writer->context, writer);
}

writer *writer_new(context *context, size_t size) {
  queue *buffers = queue_new(context);
  if (!buffers) {
    return NULL;
  }

  queue *free_buffers = queue_new(context);
  if (!free_buffers) {
    queue_delete(buffers);
    return NULL;
  }

  writer *w = context_allocate(context, 1, sizeof(writer));
  if (!w) {
    queue_delete(buffers);
    queue_delete(free_buffers);
    return NULL;
  }
  w->buffers = buffers;
  w->free_buffers = free_buffers;
  w->size = size;
  w->context = context;
  w->current_bit_index = 0;
  _writer_allocate_next_buffer(w);
  if (queue_get_count(w->buffers) == 0) {
    writer_delete(w);
    return NULL;
  }
  return w;
}

/** Returns oldest non-consumed buffer full of bytes, with length of `writer->size`.
Returns buffer of length zero if there's no full buffer.
Writer won't track this array of bytes anymore. It's up for caller to `free()` it.
Can only return completely full buffers. Won't return partially full buffers, see `writer_consume_nonempty_buffer()` */
buffer writer_consume_full_buffer(writer *writer) {
  if (queue_get_count(writer->buffers) < 2) {
    return EMPTY_BUFFER;
  }
  return (buffer){.data = queue_pop(writer->buffers), .length = writer->size};
}

/** Returns oldest non-consumed buffer. Buffer counts as consumed (writer won't store it anymore).
Returns buffer of length zero if no bytes are left to be consumed.
Only use this function if you are sure this writer will receive no more writes.
If this writer is used to write individual bits - last byte of the buffer may be partially written.
This is okay if you are closing the writer, but if more bits are to be written in this writer - next byte would be corrupted. */
buffer writer_consume_nonempty_buffer(writer *writer) {
  buffer full_buffer = writer_consume_full_buffer(writer);
  if (full_buffer.length > 0) {
    return full_buffer;
  }
  if (writer->current_bit_index == 0) {
    return EMPTY_BUFFER;
  }
  buffer result = {.data = queue_pop(writer->buffers), .length = (writer->current_bit_index + 7) >> 3};
  _writer_allocate_next_buffer(writer);
  return result;
}

/** Allocates new array of bytes. All the bytes contained in the writer are written into the array and consumed.
Returns buffer of length zero if no bytes are left to be consumed, of if there was an allocation problem.
Writer won't track byte array returned, it's up for caller to `free()` it. Internal buffers (not returned from this function) are freed by the writer.
Restriction about partially-written bytes apply, see comments to `writer_consume_nonempty_buffer` */
buffer writer_consume_all_buffers(writer *writer) {
  size_t length = writer_get_bytes_stored(writer);
  size_t index = 0;
  byte *bytes = context_allocate(writer->context, length, sizeof(byte));
  if (!bytes) {
    return EMPTY_BUFFER;
  }

  while (true) {
    buffer full_buffer = writer_consume_full_buffer(writer);
    if (full_buffer.length == 0) {
      break;
    }
    memcpy(bytes + index, full_buffer.data, full_buffer.length);
    context_free(writer->context, full_buffer.data);
    index += full_buffer.length;
  }

  buffer last_buffer = writer_consume_nonempty_buffer(writer);
  if (last_buffer.length > 0) {
    memcpy(bytes + index, last_buffer.data, last_buffer.length);
    context_free(writer->context, last_buffer.data);
    index += last_buffer.length;
  }

  return (buffer){.length = index, .data = bytes};
}

void writer_write_bit(writer *writer, byte bit) {
  assert(bit == 1 || bit == 0);
  byte *tail_buffer = queue_peek_tail(writer->buffers);
  tail_buffer[writer->current_bit_index >> 3] |= bit << (writer->current_bit_index & 7);
  writer->current_bit_index++;
  _writer_maybe_allocate_next_buffer(writer);
}

void writer_write_byte(writer *writer, byte value) {
  assert((writer->current_bit_index & 7) == 0 && "Cannot mix bit- and byte-level writes in a single writer instance.");
  byte *tail_buffer = queue_peek_tail(writer->buffers);
  tail_buffer[writer->current_bit_index >> 3] = value;
  writer->current_bit_index += 8;
  _writer_maybe_allocate_next_buffer(writer);
}

/** Like `writer_supply_dirty_buffer()`, but assumes that buffer is already zero-initialized. */
void writer_supply_zeroinit_buffer(writer *writer, byte *buffer) {
  queue_push(writer->free_buffers, buffer);
}

/** Provide writer with a buffer. Buffer must have length of `writer->size`.
Buffer will be zero-filled, and then reused as a normal writer buffer.

Idea behind this method is to reduce amount of allocations.
If mode of consumption allows you to retain buffers - might as well reuse them. */
void writer_supply_dirty_buffer(writer *writer, byte *buffer) {
  for (size_t i = 0; i < writer->size; i++) {
    buffer[i] = 0;
  }
  writer_supply_zeroinit_buffer(writer, buffer);
}