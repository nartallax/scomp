#pragma once
#include "../../src/json/json_compressor.c"
#include "../../src/json/json_decompressor.c"
#include "../../src/string_compression/string_compressor.c"
#include "../../src/string_compression/string_decompressor.c"
#include "../test_utils.c"
#include <string.h>

typedef struct {
  reader reader;
  writer writer;
  acod_encoder encoder;
  string_compressor strcomp;
  json_compressor comp;
} test_jcomp;

void test_jcomp_free(test_jcomp *jcomp) {
  if (jcomp == NULL) {
    return;
  }
  json_compressor_deinit(&jcomp->comp);
  strcomp_deinit(&jcomp->strcomp);
  writer_deinit(&jcomp->writer, test_context);
  reader_deinit(&jcomp->reader, test_context);
  free(jcomp);
}

test_jcomp *test_jcomp_alloc() {
  test_jcomp *jcomp = calloc(1, sizeof(test_jcomp));
  if (!reader_init(&jcomp->reader, test_context) || !writer_init(&jcomp->writer, test_context, 1024, 4)) {
    test_jcomp_free(jcomp);
    return NULL;
  }
  acod_encoder_init(&jcomp->encoder, &jcomp->writer);
  strcomp_init(&jcomp->strcomp, test_context, &jcomp->encoder);
  json_compressor_init(&jcomp->comp, test_context, &jcomp->reader, &jcomp->encoder, &jcomp->strcomp);

  return jcomp;
}

buffer test_jcomp_apply_compression(const char *src) {
  buffer result = EMPTY_BUFFER;
  test_jcomp *jcomp = test_jcomp_alloc();
  if (!jcomp) {
    goto deinits;
  }
  if (!reader_push_buffer(&jcomp->reader, malloc_string_buffer(src))) {
    goto deinits;
  }

  while (reader_get_bytes_available(&jcomp->reader) > 0) {
    if (!json_compressor_process_next_token(&jcomp->comp)) {
      break;
    }
  }
  json_compressor_finalize(&jcomp->comp);
  acod_encoder_deinit(&jcomp->encoder);

  buffer_or_error result_or_error = writer_consume_all_buffers(&jcomp->writer);
  if (result_or_error.is_error) {
    goto deinits;
  }
  result = result_or_error.buffer;

deinits:
  test_jcomp_free(jcomp);

  return result;
}

typedef struct {
  acod_decoder decoder;
  reader reader;
  writer writer;
  json_decompressor decomp;
  string_decompressor strdecomp;
} test_jdecomp;

void test_jdecomp_free(test_jdecomp *jdecomp) {
  if (jdecomp == NULL) {
    return;
  }
  json_decompressor_deinit(&jdecomp->decomp);
  strdecomp_deinit(&jdecomp->strdecomp);
  acod_decoder_deinit(&jdecomp->decoder);
  writer_deinit(&jdecomp->writer, test_context);
  reader_deinit(&jdecomp->reader, test_context);
  free(jdecomp);
}

test_jdecomp *test_jdecomp_alloc() {
  test_jdecomp *jdecomp = calloc(1, sizeof(test_jdecomp));
  if (!reader_init(&jdecomp->reader, test_context)) {
    test_jdecomp_free(jdecomp);
    return NULL;
  }
  if (!writer_init(&jdecomp->writer, test_context, 1024, 4)) {
    test_jdecomp_free(jdecomp);
    return NULL;
  }
  acod_decoder_init(&jdecomp->decoder, &jdecomp->reader);
  strdecomp_init(&jdecomp->strdecomp, test_context, &jdecomp->writer, &jdecomp->decoder);
  json_decompressor_init(&jdecomp->decomp, test_context, &jdecomp->decoder, &jdecomp->strdecomp, &jdecomp->writer);

  return jdecomp;
}

buffer test_jcomp_apply_decompression(buffer src) {
  buffer result = EMPTY_BUFFER;
  test_jdecomp *jdecomp = test_jdecomp_alloc();
  if (!jdecomp) {
    buffer_free(src, test_context);
    goto deinit;
  }
  if (!reader_push_buffer(&jdecomp->reader, src)) {
    buffer_free(src, test_context);
    goto deinit;
  }

  while (json_decompressor_process_next_token(&jdecomp->decomp)) {
    // process tokens
  }

  buffer_or_error result_or_error = writer_consume_all_buffers(&jdecomp->writer);
  if (result_or_error.is_error) {
    goto deinit;
  }
  result = result_or_error.buffer;

deinit:
  test_jdecomp_free(jdecomp);

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
  int64 min_len = decompressed.length < src_len ? decompressed.length : src_len;
  for (int64 i = 0; i < min_len; i++) {
    if (decompressed.data[i] != (byte)src[i]) {
      printf("Decompressed mismatches source: at %zi, expected to have '%c' (%i), but got '%c' (%i)\n", i, src[i], (byte)src[i], decompressed.data[i], decompressed.data[i]);
      return false;
    }
  }

  if (decompressed.length != src_len) {
    printf("Decompressed length mismatch: expected %zi, got %zi\n", src_len, decompressed.length);
    return false;
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
  TEST_ASSERT(test_jcomp_compression("5067680002"));

  // broken tokens
  buffer compressed;
  buffer decompressed;
  compressed = test_jcomp_apply_compression("{\"key\": [1awawa");
  decompressed = test_jcomp_apply_decompression(compressed);
  TEST_ASSERT(strncmp((char *)decompressed.data, "{\"key\": [1", decompressed.length) == 0);
  buffer_free(decompressed, test_context);
  return NULL;
}

const char *test_json_compression_broken_compressed_data() {
  buffer broken_data;
  test_jcomp *jcomp;
  test_jdecomp *jdecomp;

  // bad token outside of string
  jcomp = test_jcomp_alloc();
  jdecomp = test_jdecomp_alloc();
  acod_encoder_write(&jcomp->encoder, &jcomp->comp.base.tables[JSON_STATE_START], JSON_TOKEN_ARRAY_CLOSE);
  acod_encoder_deinit(&jcomp->encoder);
  broken_data = writer_consume_all_buffers(&jcomp->writer).buffer;
  TEST_ASSERT(reader_push_buffer(&jdecomp->reader, broken_data));
  TEST_ASSERT(!context_is_errored(test_context));
  TEST_ASSERT(!json_decompressor_process_next_token(&jdecomp->decomp));
  TEST_ASSERT(context_is_errored(test_context));
  test_jcomp_free(jcomp);
  test_jdecomp_free(jdecomp);

  // bad token mid-string
  setup_test_context_default();
  jcomp = test_jcomp_alloc();
  jdecomp = test_jdecomp_alloc();
  TEST_ASSERT(reader_push_buffer(&jcomp->reader, malloc_string_buffer("\"123")));
  while (reader_get_bytes_available(&jcomp->reader) > 0) {
    TEST_ASSERT(json_compressor_process_next_token(&jcomp->comp));
  }
  strcomp_flush(&jcomp->strcomp);
  acod_encoder_write(&jcomp->encoder, &jcomp->comp.base.tables[JSON_STATE_START], JSON_TOKEN_ARRAY_CLOSE);
  acod_encoder_deinit(&jcomp->encoder);
  broken_data = writer_consume_all_buffers(&jcomp->writer).buffer;
  TEST_ASSERT(reader_push_buffer(&jdecomp->reader, broken_data));
  for (int i = 0; i < 4; i++) {
    TEST_ASSERT(json_decompressor_process_next_token(&jdecomp->decomp));
  }
  TEST_ASSERT(!context_is_errored(test_context));
  TEST_ASSERT(!json_decompressor_process_next_token(&jdecomp->decomp));
  TEST_ASSERT(context_is_errored(test_context));
  test_jcomp_free(jcomp);
  test_jdecomp_free(jdecomp);

  return NULL;
}
