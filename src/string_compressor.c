#pragma once
#include "arithmetic_coding.c"
#include "commons.c"
#include "context.c"

typedef struct {
  context *context;
  acod_encoder *encoder;
  ftable *table;
  byte *buffer;
} string_compressor;

bool strcomp_compressor_init(string_compressor *compressor, context *context, acod_encoder *encoder, ftable *table, size_t buffer_size) {
  compressor->encoder = encoder;
  compressor->table = table;
  compressor->context = context;
  compressor->buffer = context_allocate(context, buffer_size, sizeof(byte));
  return !!compressor->buffer;
}

void strcomp_compressor_deinit(string_compressor *compressor) {
  context_free(compressor->context, compressor->buffer);
}

bool strcomp_compressor_write(string_compressor *compressor, byte b) {
  // TODO: implementation
  return false;
}

/** Write out everything the compressor has in its buffer */
bool strcomp_compressor_flush(string_compressor *compressor) {
  // TODO: impl
  return false;
}