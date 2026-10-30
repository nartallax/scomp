#pragma once
#include "../commons.c"
#include "../context.c"
#include "buffer.c"
#include "queue.c"
#include <assert.h>
#include <limits.h>

/** Power-of-two, for max amount of bytes in the reader.
Lower limit to this value is amount of bytes a single read operation may consume,
which for this project is a JSON number.
There's no real tangible benefit of raising this number above minimum
(other than general robustness against various statistical improbabilities) */
constexpr int64 READER_SAFE_SIZE_SHIFT = 9;
constexpr int64 READER_SAFE_SIZE = 1 << READER_SAFE_SIZE_SHIFT;

/** Reader is a buffer that can accept an array of bytes, and supply bytes or bits to consumers.
Underflow causes an error; it's expected to never happen during normal functioning.
Because of that consumers must be aware of reader capacity and available data. */
typedef struct {
  queue buffers;
  buffer current_buffer;
  int64 exhausted_buffers_count;
  // index of the first not consumed bit in current_buffer
  int64 bit_pointer;
  int64 unconsumed_buffer_bytes_count;
  context *context;
  bool is_at_initial_buffer;
} reader;

void reader_deinit(reader *r, context *context) {
  while (queue_get_count(&r->buffers) > 0) {
    buffer *b = queue_pop(&r->buffers);
    buffer_free(*b, context);
  }
  queue_deinit(&r->buffers, context);
  // shouldn't have to free current buffer either way
  // it's either exhausted and consumed, which means it's out of our control now,
  // or it was in buffers queue and was freed above.
  // (or it's initial empty buffer and there's no harm in not freeing it)

  *r = (reader){0};
}

NODISCARD bool reader_init(reader *r, context *context) {
  *r = (reader){0};

  r->bit_pointer = 0;
  r->context = context;
  r->current_buffer = EMPTY_BUFFER;
  r->exhausted_buffers_count = 0;
  r->unconsumed_buffer_bytes_count = 0;
  r->is_at_initial_buffer = true;

  if (!queue_init(&r->buffers, context, sizeof(buffer), 4)) {
    reader_deinit(r, context);
    return false;
  }

  return true;
}

INLINE int64 reader_get_bits_available(reader *r) {
  return (r->unconsumed_buffer_bytes_count << 3) - r->bit_pointer;
}

INLINE int64 reader_get_bytes_available(reader *r) {
  assert((r->bit_pointer & 7) == 0 && "Cannot mix bitwise and byte-wise read modes");
  return r->unconsumed_buffer_bytes_count - (r->bit_pointer >> 3);
}

/** "safe" is defined as "next read operation won't error out" */
INLINE bool reader_has_safe_amount_of_data(reader *r) {
  return reader_get_bytes_available(r) >= READER_SAFE_SIZE;
}

/** Add a buffer to the reader. Returns false in case of failed allocation.
`length` of the buffer supplied is respected; reader won't read past that. */
NODISCARD bool reader_push_buffer(reader *r, buffer b) {
  buffer *slot = queue_push(&r->buffers);
  if (slot == NULL) {
    return false;
  }
  *slot = b;
  r->unconsumed_buffer_bytes_count += b.length;
  return true;
}

INLINE bool reader_has_exhausted_buffer(reader *r) {
  return r->exhausted_buffers_count > 0;
}

/** Returns a buffer that was read completely.
Assumes that such buffer exists; check first with `reader_has_exhausted_buffer` */
buffer reader_recover_exhausted_buffer(reader *r) {
  assert(reader_has_exhausted_buffer(r) && "When recovering exhausted buffer - such buffer must exist");
  buffer *slot = queue_pop(&r->buffers);
  r->exhausted_buffers_count--;
  return *slot;
}

NODISCARD bool _reader_get_next_buffer(reader *r) {
  int64 total_buffer_count = queue_get_count(&r->buffers) + (r->is_at_initial_buffer ? 1 : 0);
  if (total_buffer_count - 1 == r->exhausted_buffers_count) {
    context_set_error(r->context, "Reader underflow");
    return false;
  }

  r->exhausted_buffers_count += r->is_at_initial_buffer ? 0 : 1;
  r->unconsumed_buffer_bytes_count -= r->current_buffer.length;
  buffer *slot = queue_peek_at(&r->buffers, r->exhausted_buffers_count);
  r->current_buffer = *slot;
  r->is_at_initial_buffer = false;
  r->bit_pointer = 0;

  return true;
}

NODISCARD INLINE bool _reader_ensure_buffer(reader *r) {
  // while here in case of empty buffers in the list, which could happen if someone misuses library API
  while (r->current_buffer.length <= r->bit_pointer >> 3) {
    if (!_reader_get_next_buffer(r)) {
      return false;
    }
  }
  return true;
}

INLINE byte reader_read_byte(reader *r) {
  assert((r->bit_pointer & 7) == 0 && "Cannot mix bitwise and byte-wise read modes");
  if (!_reader_ensure_buffer(r)) {
    return 0;
  }

  byte result = r->current_buffer.data[r->bit_pointer >> 3];
  r->bit_pointer += 8;
  return result;
}

INLINE byte reader_read_bit(reader *r) {
  if ((r->bit_pointer & 7) != 0 && !_reader_ensure_buffer(r)) {
    return 0;
  }

  byte b = r->current_buffer.data[r->bit_pointer >> 3];
  byte bit = (b >> (r->bit_pointer & 7)) & 1;
  r->bit_pointer++;
  return bit;
}