#pragma once

#include "commons.c"
#include "context.c"
#include "data_structures/queue.c"
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
  size_t single_buffer_byte_size;
  queue free_buffers;
} writer;

typedef struct {
  byte *data;
  size_t length;
} buffer;

constexpr buffer EMPTY_BUFFER = (buffer){.data = NULL, .length = 0};

typedef struct {
  bool is_error;
  buffer buffer;
} buffer_or_error;

constexpr buffer_or_error ERROR_ERROR_BUFFER = (buffer_or_error){.is_error = true, .buffer = EMPTY_BUFFER};
constexpr buffer_or_error EMPTY_ERROR_BUFFER = (buffer_or_error){.is_error = false, .buffer = EMPTY_BUFFER};

void _writer_allocate_next_buffer(writer *writer) {
  byte *buffer;
  if (queue_get_count(&writer->free_buffers) > 0) {
    byte **existing_buffer_slot = queue_pop(&writer->free_buffers);
    buffer = *existing_buffer_slot;
  } else {
    buffer = context_allocate(writer->context, writer->single_buffer_byte_size, sizeof(byte));
  }

  if (buffer) {
    byte **new_buffer_slot = queue_push(&writer->buffers);
    if (new_buffer_slot) {
      *new_buffer_slot = buffer;
    } else {
      context_free(writer->context, buffer);
    }
  }
  // when allocation fails - it's bad, but we can't do anything about it
  // context is considered broken and must not be used
  // but that's not supposed to happen anytime often
  // so, what we are doing: we are still writing to SOME buffer;
  // this overwrites existing output, which makes output of the whole library garbadge;
  // but it's fine, because error is set on context, and should be checked, which means output is not supposed to be used anyway
  writer->current_bit_index = 0;
}

void _writer_maybe_allocate_next_buffer(writer *writer) {
  if ((writer->current_bit_index >> 3) >= writer->single_buffer_byte_size) {
    _writer_allocate_next_buffer(writer);
  }
}

size_t writer_get_bytes_stored(writer *writer) {
  int buffer_count = (int)queue_get_count(&writer->buffers);
  if (buffer_count > 0) {
    // there should always be at least 1 buffer
    // except for case when allocation for the first buffer failed
    // to avoid underflow, this condition exists
    buffer_count--;
  }
  return (buffer_count * writer->single_buffer_byte_size) + ((writer->current_bit_index + 7) >> 3);
}

void writer_deinit(writer *w, context *context) {
  while (queue_get_count(&w->buffers) > 0) {
    byte **slot = queue_pop(&w->buffers);
    context_free(context, *slot);
  }
  queue_deinit(&w->buffers, context);

  while (queue_get_count(&w->free_buffers) > 0) {
    byte **slot = queue_pop(&w->free_buffers);
    context_free(context, *slot);
  }
  queue_deinit(&w->free_buffers, context);
}

NODISCARD bool writer_init(writer *w, context *context, size_t single_buffer_byte_size, size_t queues_size_shift) {
  *w = (writer){0};

  w->single_buffer_byte_size = single_buffer_byte_size;
  w->context = context;
  w->current_bit_index = 0;

  if (!queue_init(&w->buffers, context, sizeof(byte *), queues_size_shift)) {
    writer_deinit(w, context);
    return false;
  }

  if (!queue_init(&w->free_buffers, context, sizeof(byte *), queues_size_shift)) {
    writer_deinit(w, context);
    return false;
  }

  _writer_allocate_next_buffer(w);
  if (context_is_errored(context)) {
    writer_deinit(w, context);
    return false;
  }

  return true;
}

/** Resets offset writer uses to locate end of current buffer.
This leads writer to overwrite values already in the buffer, effectively reusing it.
Existing data in the buffer will be lost. */
void writer_reset_current_buffer(writer *w) {
  w->current_bit_index = 0;
}

/** Returns the buffer writer is currently writing into without consuming it. */
buffer writer_peek_current_buffer(writer *w) {
  byte **slot = queue_peek_tail(&w->buffers);
  byte *tail_buffer = *slot;
  return (buffer){.data = tail_buffer, .length = ((w->current_bit_index + 7) >> 3)};
}

/** Returns oldest non-consumed buffer full of bytes, with length of `writer->size`.
Returns buffer of length zero if there's no full buffer.
Writer won't track this array of bytes anymore. It's up for caller to `free()` it.
Can only return completely full buffers. Won't return partially full buffers, see `writer_consume_nonempty_buffer()` */
NODISCARD buffer writer_consume_full_buffer(writer *writer) {
  if (queue_get_count(&writer->buffers) < 2) {
    // there always should be at least 1 non-full buffer in the buffer queue
    // if there's only 1 buffer - it's not full, so we must not return it
    return EMPTY_BUFFER;
  }
  byte **slot = queue_pop(&writer->buffers);
  return (buffer){.data = *slot, .length = writer->single_buffer_byte_size};
}

/** Returns oldest non-consumed buffer. Buffer counts as consumed (writer won't store it anymore).
Returns buffer of length zero if no bytes are left to be consumed.
Only use this function if you are sure this writer will receive no more writes.
If this writer is used to write individual bits - last byte of the buffer may be partially written.
This is okay if you are closing the writer, but if more bits are to be written in this writer - next byte would be corrupted. */
NODISCARD buffer_or_error writer_consume_nonempty_buffer(writer *writer) {
  if (context_is_errored(writer->context)) {
    return ERROR_ERROR_BUFFER;
  }

  buffer full_buffer = writer_consume_full_buffer(writer);
  if (full_buffer.length > 0) {
    return (buffer_or_error){.is_error = false, .buffer = full_buffer};
  }

  if (writer->current_bit_index == 0) {
    return EMPTY_ERROR_BUFFER;
  }
  size_t bit_index = writer->current_bit_index;
  // allocating before pop, to avoid losing data
  _writer_allocate_next_buffer(writer);
  if (context_is_errored(writer->context)) {
    // we are reusing the buffer. we must not pop it.
    return ERROR_ERROR_BUFFER;
  }
  byte **slot = queue_pop(&writer->buffers);
  buffer result_buffer = {.data = *slot, .length = (bit_index + 7) >> 3};
  return (buffer_or_error){.is_error = false, .buffer = result_buffer};
}

/** Allocates new array of bytes. All the bytes contained in the writer are written into the array and consumed.
Returns buffer of length zero if no bytes are left to be consumed, of if there was an allocation problem.
Writer won't track byte array returned, it's up for caller to `free()` it. Internal buffers (not returned from this function) are freed by the writer.
Restriction about partially-written bytes apply, see comments to `writer_consume_nonempty_buffer` */
NODISCARD buffer_or_error writer_consume_all_buffers(writer *writer) {
  size_t length = writer_get_bytes_stored(writer);
  size_t index = 0;
  byte *bytes = context_allocate(writer->context, length, sizeof(byte));
  if (!bytes) {
    return ERROR_ERROR_BUFFER;
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

  buffer_or_error last_buffer = writer_consume_nonempty_buffer(writer);
  if (last_buffer.is_error) {
    free(bytes);
    return last_buffer;
  }

  if (last_buffer.buffer.length > 0) {
    memcpy(bytes + index, last_buffer.buffer.data, last_buffer.buffer.length);
    context_free(writer->context, last_buffer.buffer.data);
    index += last_buffer.buffer.length;
  }

  return (buffer_or_error){.is_error = false, .buffer = (buffer){.length = index, .data = bytes}};
}

void writer_write_bit(writer *writer, byte bit) {
  assert(bit == 1 || bit == 0);
  byte **slot = queue_peek_tail(&writer->buffers);
  byte *tail_buffer = *slot;
  byte b = tail_buffer[writer->current_bit_index >> 3];
  // TODO: faster way to do this?
  if ((writer->current_bit_index & 0x07) == 0) {
    // we could zero-init buffers on allocation
    // but that will negatively affect mode when consumers write whole bytes, and reuse buffers
    // because those buffers will need to be cleaned for nothing, as data in them will be overwritten anyways
    // so we zero-init individual bytes on write
    b = 0;
  }
  b |= bit << (writer->current_bit_index & 7);
  tail_buffer[writer->current_bit_index >> 3] = b;
  writer->current_bit_index++;
  _writer_maybe_allocate_next_buffer(writer);
}

void writer_write_byte(writer *writer, byte value) {
  assert((writer->current_bit_index & 7) == 0 && "Cannot mix bit- and byte-level writes in a single writer instance.");
  byte **slot = queue_peek_tail(&writer->buffers);
  byte *tail_buffer = *slot;
  tail_buffer[writer->current_bit_index >> 3] = value;
  writer->current_bit_index += 8;
  _writer_maybe_allocate_next_buffer(writer);
}

/** Provide writer with a buffer. Buffer must have length of `writer->size`.
Buffer will be used as a normal writer buffer.
Returns false in case of allocation errors.

Idea behind this method is to reduce amount of allocations.
If mode of consumption allows you to retain buffers - might as well reuse them. */
NODISCARD bool writer_supply_buffer(writer *writer, byte *buffer) {
  byte **slot = queue_push(&writer->free_buffers);
  if (!slot) {
    return false;
  }
  *slot = buffer;
  return true;
}

// TODO: unicode-aware string compression
// 1 freq table for charcode start, 0-4
// 0 means "not a unicode byte". we know it's something in range 128-255, so we only write lower 7 bits as a symbol in a freq table for 0
// 1 means "1 unicode byte". likewise, we know it's 0-127, and we can only write 7 bits, using another freq table, for 1
// 2 means "2 unicode bytes". likewise, only write meaningful bits of first byte as a symbol, and second byte as a symbol, using two more freq tables for that
// 3, 4 - likewise. only meaningful bits for the first byte, the rest of them as-is, using separate unicode tables
// wonder if it will be significantly worse on non-unicode streams
// this also implies doing something about byte stream matcher in stream compression pipeline
// as it may match up to the middle of the utf-8 sequence, breaking it, and caller would need to handle this situation
