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
  queue buffers;
  size_t current_bit_index;
  size_t size;
  queue free_buffers;
} writer;

typedef struct {
  byte *data;
  size_t length;
} buffer;

const buffer EMPTY_BUFFER = (buffer){.data = NULL, .length = 0};

bool _writer_allocate_next_buffer(writer *writer) {
  byte *buffer;
  if (queue_get_count(&writer->free_buffers) > 0) {
    byte **existing_buffer_slot = queue_pop(&writer->free_buffers);
    buffer = *existing_buffer_slot;
  } else {
    buffer = context_allocate_zero_init(writer->context, writer->size, sizeof(byte));
    if (!buffer) {
      return false;
    }
  }

  byte **new_buffer_slot = queue_push(&writer->buffers);
  if (!new_buffer_slot) {
    context_free(writer->context, buffer);
    return false;
  }

  *new_buffer_slot = buffer;
  writer->current_bit_index = 0;
  return true;
}

bool _writer_maybe_allocate_next_buffer(writer *writer) {
  if ((writer->current_bit_index >> 3) >= writer->size) {
    return _writer_allocate_next_buffer(writer);
  }
  return true;
}

size_t writer_get_bytes_stored(writer *writer) {
  int buffer_count = (int)queue_get_count(&writer->buffers);
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

  while (queue_get_count(&writer->buffers) > 0) {
    byte **slot = queue_pop(&writer->buffers);
    context_free(writer->context, *slot);
  }
  queue_deinit(&writer->buffers);

  while (queue_get_count(&writer->free_buffers) > 0) {
    byte **slot = queue_pop(&writer->free_buffers);
    context_free(writer->context, *slot);
  }
  queue_deinit(&writer->free_buffers);

  context_free(writer->context, writer);
}

writer *writer_new(context *context, size_t size) {
  writer *w = context_allocate(context, 1, sizeof(writer));
  if (!w) {
    return NULL;
  }
  w->size = size;
  w->context = context;
  w->current_bit_index = 0;

  if (!queue_init(&w->buffers, context, sizeof(byte *))) {
    context_free(context, w);
    return NULL;
  }

  if (!queue_init(&w->free_buffers, context, sizeof(byte *))) {
    queue_deinit(&w->buffers);
    context_free(context, w);
    return NULL;
  }

  if (!_writer_allocate_next_buffer(w)) {
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
  if (queue_get_count(&writer->buffers) < 2) {
    // there always should be at least 1 non-full buffer in the buffer queue
    // if there's only 1 buffer - it's not full, so we must not return it
    return EMPTY_BUFFER;
  }
  byte **slot = queue_pop(&writer->buffers);
  return (buffer){.data = *slot, .length = writer->size};
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
  byte **slot = queue_pop(&writer->buffers);
  buffer result = {.data = *slot, .length = (writer->current_bit_index + 7) >> 3};
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

bool writer_write_bit(writer *writer, byte bit) {
  assert(bit == 1 || bit == 0);
  byte **slot = queue_peek_tail(&writer->buffers);
  byte *tail_buffer = *slot;
  tail_buffer[writer->current_bit_index >> 3] |= bit << (writer->current_bit_index & 7);
  writer->current_bit_index++;
  return _writer_maybe_allocate_next_buffer(writer);
}

bool writer_write_byte(writer *writer, byte value) {
  assert((writer->current_bit_index & 7) == 0 && "Cannot mix bit- and byte-level writes in a single writer instance.");
  byte **slot = queue_peek_tail(&writer->buffers);
  byte *tail_buffer = *slot;
  tail_buffer[writer->current_bit_index >> 3] = value;
  writer->current_bit_index += 8;
  return _writer_maybe_allocate_next_buffer(writer);
}

/** Like `writer_supply_dirty_buffer()`, but assumes that buffer is already zero-initialized. */
void writer_supply_zeroinit_buffer(writer *writer, byte *buffer) {
  byte **slot = queue_push(&writer->free_buffers);
  *slot = buffer;
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