#pragma once
#include "../commons.c"
#include "../context.c"

/** Buffer is a byte array with some helpers */
typedef struct {
  byte *data;
  /** Amount of meaningful bytes `data` contains */
  int64 length;
  /** Amount of bytes allocated for the `data` pointer */
  int64 allocated_length;
} buffer;

constexpr buffer EMPTY_BUFFER = (buffer){0};

INLINE buffer buffer_allocate(context *context, int64 byte_length) {
  return (buffer){.data = context_allocate(context, byte_length, sizeof(byte)), .length = 0, .allocated_length = byte_length};
}

INLINE buffer buffer_make(byte *const data, int64 length, int64 allocated_length) {
  return (buffer){.data = data, .length = length, .allocated_length = allocated_length};
}

INLINE void buffer_free(buffer buffer, context *context) {
  context_free(context, buffer.data);
}

typedef struct {
  bool is_error;
  buffer buffer;
} buffer_or_error;

constexpr buffer_or_error ERROR_ERROR_BUFFER = (buffer_or_error){.is_error = true, .buffer = EMPTY_BUFFER};
constexpr buffer_or_error EMPTY_ERROR_BUFFER = (buffer_or_error){.is_error = false, .buffer = EMPTY_BUFFER};