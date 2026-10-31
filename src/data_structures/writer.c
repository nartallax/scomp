#pragma once

#include "../commons.c"
#include "../context.c"
#include "buffer.c"
#include "queue.c"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/** Writer is a structure that receives bytes and bits from various producers.
Received data is stored inside the structure, and then can be consumed with `writer_consume_...()` methods.
Bits are written first in lowest-value bit of a byte.
Buffers can be reused via `writer_supply_...()` methods. */
typedef struct {
  context *context;
  queue buffers;
  int64 bytes_stored;
  int64 current_bit_index;
  int64 single_buffer_byte_size;
  // TODO: do one queue of buffers like reader does instead of two?
  queue free_buffers;
} writer;

bool _writer_produce_next_buffer(writer *writer) {
  writer->current_bit_index = 0;
  buffer result;
  if (queue_get_count(&writer->free_buffers) > 0) {
    buffer *existing_buffer_slot = queue_pop(&writer->free_buffers);
    result = *existing_buffer_slot;
  } else {
    byte *bytes = context_allocate(writer->context, writer->single_buffer_byte_size, sizeof(byte));
    if (!bytes) {
      // when allocation fails - it's bad, but we can't do anything about it
      // context is considered broken and must not be used
      // but that's not supposed to happen anytime often
      // so, what we are doing: we are still writing to SOME buffer;
      // this overwrites existing output, which makes output of the whole library garbadge;
      // but it's fine, because error is set on context, and should be checked, which means output is not supposed to be used anyway
      return false;
    }
    result = buffer_make(bytes, 0, writer->single_buffer_byte_size);
  }

  buffer *new_buffer_slot = queue_push(&writer->buffers);
  if (!new_buffer_slot) {
    buffer_free(result, writer->context);
    return false;
  }

  *new_buffer_slot = result;
  return true;
}

// TODO: store current buffer in a field, just like reader does? instead of fetching it from the queue every time. may improve write perf
INLINE buffer _writer_ensure_buffer(writer *writer) {
  buffer *slot = queue_peek_tail(&writer->buffers);
  if ((writer->current_bit_index >> 3) >= slot->allocated_length) {
    writer->bytes_stored += slot->allocated_length;
    _writer_produce_next_buffer(writer);
    slot = queue_peek_tail(&writer->buffers);
  }
  return *slot;
}

INLINE int64 writer_get_bytes_stored(writer *writer) {
  return writer->bytes_stored + ((writer->current_bit_index + CHAR_BIT - 1) >> 3);
}

void writer_deinit(writer *w, context *context) {
  while (queue_get_count(&w->buffers) > 0) {
    buffer *slot = queue_pop(&w->buffers);
    buffer_free(*slot, context);
  }
  queue_deinit(&w->buffers, context);

  while (queue_get_count(&w->free_buffers) > 0) {
    buffer *slot = queue_pop(&w->free_buffers);
    buffer_free(*slot, context);
  }
  queue_deinit(&w->free_buffers, context);

  *w = (writer){0};
}

NODISCARD bool writer_init(writer *w, context *context, int64 single_buffer_byte_size, int64 queues_size_shift) {
  *w = (writer){0};

  w->single_buffer_byte_size = single_buffer_byte_size;
  w->context = context;
  w->current_bit_index = 0;

  if (!queue_init(&w->buffers, context, sizeof(buffer), queues_size_shift)) {
    writer_deinit(w, context);
    return false;
  }

  if (!queue_init(&w->free_buffers, context, sizeof(buffer), queues_size_shift)) {
    writer_deinit(w, context);
    return false;
  }

  _writer_produce_next_buffer(w);
  if (context_is_errored(context)) {
    writer_deinit(w, context);
    return false;
  }

  return true;
}

/** Resets offset writer uses to locate end of current buffer.
This leads writer to overwrite values already in the buffer, effectively reusing it.
Existing data in the buffer will be lost. */
INLINE void writer_reset_current_buffer(writer *w) {
  w->current_bit_index = 0;
}

/** Returns the buffer writer is currently writing into without consuming it. */
INLINE buffer writer_peek_current_buffer(writer *w) {
  buffer *slot = queue_peek_tail(&w->buffers);
  slot->length = (w->current_bit_index + CHAR_BIT - 1) >> 3;
  return *slot;
}

/** Returns oldest non-consumed buffer full of bytes, with length of `writer->size`.
Returns buffer of length zero if there's no full buffer.
Writer won't track this array of bytes anymore. It's up for caller to `free()` it.
Can only return completely full buffers. Won't return partially full buffers, see `writer_consume_nonempty_buffer()` */
NODISCARD INLINE buffer writer_consume_full_buffer(writer *writer) {
  if (queue_get_count(&writer->buffers) < 2) {
    // there always should be at least 1 non-full buffer in the buffer queue
    // if there's only 1 buffer - it's not full, so we must not return it
    return EMPTY_BUFFER;
  }
  buffer *slot = queue_pop(&writer->buffers);
  slot->length = slot->allocated_length;
  writer->bytes_stored -= slot->length;
  return *slot;
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
  int64 bit_index = writer->current_bit_index;
  // allocating before pop, to avoid losing data
  if (!_writer_produce_next_buffer(writer)) {
    // we are reusing the buffer. we must not pop it.
    return ERROR_ERROR_BUFFER;
  }
  buffer *slot = queue_pop(&writer->buffers);
  slot->length = (bit_index + CHAR_BIT - 1) >> 3;
  return (buffer_or_error){.is_error = false, .buffer = *slot};
}

/** Allocates new array of bytes. All the bytes contained in the writer are written into the array and consumed.
Returns buffer of length zero if no bytes are left to be consumed, of if there was an allocation problem.
Writer won't track byte array returned, it's up for caller to `free()` it. Internal buffers (not returned from this function) are freed by the writer.
Restriction about partially-written bytes apply, see comments to `writer_consume_nonempty_buffer` */
NODISCARD buffer_or_error writer_consume_all_buffers(writer *writer) {
  int64 length = writer_get_bytes_stored(writer);
  int64 index = 0;
  byte *bytes = context_allocate(writer->context, length, sizeof(byte));
  if (!bytes) {
    return ERROR_ERROR_BUFFER;
  }

  while (true) {
    buffer full_buffer = writer_consume_full_buffer(writer);
    if (full_buffer.length == 0) {
      break;
    }
    // TODO: move memcpy to context, to avoid libc
    memcpy(bytes + index, full_buffer.data, full_buffer.length);
    buffer_free(full_buffer, writer->context);
    index += full_buffer.length;
  }

  buffer_or_error last_buffer = writer_consume_nonempty_buffer(writer);
  if (last_buffer.is_error) {
    free(bytes);
    return last_buffer;
  }

  if (last_buffer.buffer.length > 0) {
    memcpy(bytes + index, last_buffer.buffer.data, last_buffer.buffer.length);
    buffer_free(last_buffer.buffer, writer->context);
    index += last_buffer.buffer.length;
  }

  return (buffer_or_error){.is_error = false, .buffer = buffer_make(bytes, length, length)};
}

// TODO: store currently written byte in a field instead of fetching it from the buffer? may help with write speed
INLINE void writer_write_bit(writer *writer, byte bit) {
  assert(bit == 1 || bit == 0);
  buffer buf = _writer_ensure_buffer(writer);
  byte b = buf.data[writer->current_bit_index >> 3];
  // TODO: faster way to do this?
  if ((writer->current_bit_index & 0x07) == 0) {
    // we could zero-init buffers on allocation
    // but that will negatively affect mode when consumers write whole bytes, and reuse buffers
    // because those buffers will need to be cleaned for nothing, as data in them will be overwritten anyways
    // so we zero-init individual bytes on write
    b = 0;
  }
  b |= bit << (writer->current_bit_index & 7);
  buf.data[writer->current_bit_index >> 3] = b;
  writer->current_bit_index++;
  _writer_ensure_buffer(writer);
}

INLINE void writer_write_byte(writer *writer, byte value) {
  assert((writer->current_bit_index & 7) == 0 && "Cannot mix bit- and byte-level writes in a single writer instance.");
  buffer buf = _writer_ensure_buffer(writer);
  buf.data[writer->current_bit_index >> 3] = value;
  writer->current_bit_index += 8;
  _writer_ensure_buffer(writer);
}

/** Provide writer with a buffer.
Buffer will be used as a normal writer buffer.
Returns false in case of allocation errors.

Idea behind this method is to reduce amount of allocations.
If mode of consumption allows you to retain buffers - might as well reuse them. */
NODISCARD bool writer_supply_buffer(writer *writer, buffer b) {
  assert(b.allocated_length > 0 && "Writer only accepts non-zero-length buffers");
  buffer *slot = queue_push(&writer->free_buffers);
  if (!slot) {
    return false;
  }
  *slot = b;
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
