#pragma once
#include "../../src/json/json_compressor.c"
#include "../../src/json/json_decompressor.c"
#include "../../src/string_compression/string_compressor.c"
#include "../../src/string_compression/string_decompressor.c"
#include "../test_utils.c"
#include <string.h>

buffer test_jcomp_apply_compression(const char *src) {
  reader reader = {0};
  writer writer = {0};
  acod_encoder encoder = {0};
  string_compressor strcomp = {0};
  json_compressor comp = {0};
  buffer result = EMPTY_BUFFER;
  if (!reader_init(&reader, test_context) || !writer_init(&writer, test_context, 1024, 4)) {
    goto deinits;
  }
  acod_encoder_init(&encoder, &writer);
  if (!strcomp_init(&strcomp, test_context, &encoder)) {
    goto deinits;
  }

  if (!json_compressor_init(&comp, test_context, &reader, &encoder, &strcomp)) {
    goto deinits;
  }

  if (!reader_push_buffer(&reader, malloc_string_buffer(src))) {
    goto deinits;
  }

  while (reader_get_bytes_available(&reader) > 0) {
    if (!json_compressor_process_next_token(&comp)) {
      break;
    }
  }
  json_compressor_finalize(&comp);
  acod_encoder_deinit(&encoder);

  buffer_or_error result_or_error = writer_consume_all_buffers(&writer);
  if (result_or_error.is_error) {
    goto deinits;
  }
  result = result_or_error.buffer;

deinits:
  json_compressor_deinit(&comp, test_context);
  strcomp_deinit(&strcomp, test_context);
  writer_deinit(&writer, test_context);
  reader_deinit(&reader, test_context);

  return result;
}

buffer test_jcomp_apply_decompression(buffer src) {
  acod_decoder decoder = {0};
  reader r = {0};
  writer w = {0};
  json_decompressor decomp = {0};
  string_decompressor strdecomp = {0};
  buffer result = EMPTY_BUFFER;
  if (!reader_init(&r, test_context) || !reader_push_buffer(&r, src)) {
    buffer_free(src, test_context);
    goto deinit;
  }
  if (!writer_init(&w, test_context, 1024, 4)) {
    goto deinit;
  }
  acod_decoder_init(&decoder, &r);
  if (!strdecomp_init(&strdecomp, test_context, &w, &decoder) || !json_decompressor_init(&decomp, test_context, &decoder, &strdecomp, &w)) {
    goto deinit;
  }

  while (json_decompressor_process_next_token(&decomp)) {
    // process tokens
  }

  buffer_or_error result_or_error = writer_consume_all_buffers(&w);
  if (result_or_error.is_error) {
    goto deinit;
  }
  result = result_or_error.buffer;

deinit:
  json_decompressor_deinit(&decomp, test_context);
  strdecomp_deinit(&strdecomp, test_context);
  writer_deinit(&w, test_context);
  reader_deinit(&r, test_context);

  return result;
}

NODISCARD bool test_jcomp_compression(const char *src) {
  buffer compressed = test_jcomp_apply_compression(src);
  if (compressed.length == 0) {
    printf("Failed to compress; context error: %s\n", context_get_error(test_context)->message);
    return false;
  }

  buffer decompressed = test_jcomp_apply_decompression(compressed);
  if (decompressed.length == 0) {
    printf("Failed to decompress; context error: %s\n", context_get_error(test_context)->message);
    return false;
  }

  int64 src_len = strlen(src);
  if (decompressed.length != src_len) {
    printf("Decompressed length mismatch: expected %zi, got %zi\n", src_len, decompressed.length);
    return false;
  }

  for (int64 i = 0; i < src_len; i++) {
    if (decompressed.data[i] != (byte)src[i]) {
      printf("Decompressed mismatches source: at %zi, expected to have '%c' (%i), but got '%c' (%i)\n", i, src[i], (byte)src[i], decompressed.data[i], decompressed.data[i]);
      return false;
    }
  }

  buffer_free(decompressed, test_context);

  return true;
}

const char *test_json_compression_simple() {
  TEST_ASSERT(test_jcomp_compression("0"));
  TEST_ASSERT(test_jcomp_compression("[1,2,3]"));
  TEST_ASSERT(test_jcomp_compression("\"test\""));
  TEST_ASSERT(test_jcomp_compression("\"Привет, мир!\""));
  TEST_ASSERT(test_jcomp_compression("\"\\u1234\\n\\r\\t\""));
  TEST_ASSERT(test_jcomp_compression("true"));
  TEST_ASSERT(test_jcomp_compression("[true\n,\tfalse , null]"));
  TEST_ASSERT(test_jcomp_compression("{\"key\": [\"value\", -12.34e-56, {\"key2\":\"value2\"}]}"));
  TEST_ASSERT(test_jcomp_compression("{\"key\": [\"value\", ")); // early stop
  TEST_ASSERT(test_jcomp_compression("{\"key\": [\"val"));       // early stop mid string

  // broken tokens
  buffer compressed;
  buffer decompressed;
  compressed = test_jcomp_apply_compression("{\"key\": [1awawa");
  decompressed = test_jcomp_apply_decompression(compressed);
  TEST_ASSERT(strncmp((char *)decompressed.data, "{\"key\": [1", decompressed.length) == 0);
  buffer_free(decompressed, test_context);
  return NULL;
}

const char *test_json_compression_allocation_failures() {
  // TODO: this much cumulative allocations is nuts. most of it comes from frequency tables
  // we can do two things - stop allocating compaction table for each ftable individually and use one communal compaction table from context,
  // and have a one big array for ftable space instead of allocating some each time a ftable is created
  int i = 0;
  for (; i < 87; i++) {
    setup_test_context(i);
    TEST_ASSERT(test_jcomp_apply_compression("[123]").length == 0);
  }
  setup_test_context(i);
  buffer result;
  result = test_jcomp_apply_compression("[123]");
  TEST_ASSERT(result.length > 0);
  buffer_free(result, test_context);

  for (i = 0; i < 84; i++) {
    setup_test_context_default();
    buffer compressed = test_jcomp_apply_compression("[123]");
    setup_test_context(i);
    TEST_ASSERT(test_jcomp_apply_decompression(compressed).length == 0);
  }
  setup_test_context_default();
  buffer compressed = test_jcomp_apply_compression("[123]");
  setup_test_context(i);
  result = test_jcomp_apply_decompression(compressed);
  TEST_ASSERT(result.length > 0);
  buffer_free(result, test_context);

  return NULL;
}