#pragma once
#include "../../src/string_compression/byte_stream_matcher.c"
#include "../../src/string_compression/string_compressor.c"
#include "../../src/string_compression/string_decompressor.c"
#include "../../src/writer.c"
#include "../test_utils.c"
#include <limits.h>

buffer _test_strcomp_compress(const char *source) {
  writer w;
  if (!writer_init(&w, test_context, 1024, 4)) {
    printf("Failed to init writer\n");
    return EMPTY_BUFFER;
  }
  acod_encoder encoder;
  acod_encoder_init(&encoder, &w);
  string_compressor comp;
  if (!strcomp_init(&comp, test_context, &encoder)) {
    printf("Failed to init comp\n");
    return EMPTY_BUFFER;
  }

  for (int i = 0; source[i] != 0; i++) {
    strcomp_write(&comp, source[i]);
  }

  strcomp_flush(&comp);
  strcomp_deinit(&comp, test_context);
  acod_encoder_deinit(&encoder);
  buffer_or_error result = writer_consume_all_buffers(&w);
  if (result.is_error) {
    printf("Failed to produce compressed buffer\n");
    return EMPTY_BUFFER;
  }
  writer_deinit(&w, test_context);
  return result.buffer;
}

buffer _test_strcomp_decompress(buffer src) {
  writer w;
  if (!writer_init(&w, test_context, 1024, 4)) {
    printf("Failed to init writer\n");
    return EMPTY_BUFFER;
  }
  acod_decoder decoder;
  acod_decoder_init(&decoder);
  string_decompressor decomp;
  if (!strdecomp_init(&decomp, test_context, &w, &decoder)) {
    printf("Failed to init decomp\n");
    return EMPTY_BUFFER;
  }

  bool has_eof = false;
  for (int64 i = 0; i < src.length; i++) {
    byte b = src.data[i];
    for (int j = 0; j < CHAR_BIT; j++) {
      bool is_eof = strdecomp_push_bit(&decomp, b & 1);
      b >>= 1;
      if (is_eof) {
        has_eof = true;
        if (i == src.length - 1) {
          // unlikely to ever happen
          break;
        } else {
          printf("Unexpected EOF\n");
          return EMPTY_BUFFER;
        }
      }
    }
  }

  for (int i = 0; i < 64; i++) {
    bool is_eof = strdecomp_push_bit(&decomp, 0);
    if (is_eof) {
      has_eof = true;
      break;
    }
  }

  if (!has_eof) {
    printf("No eof after end of source bytes\n");
    return EMPTY_BUFFER;
  }

  strdecomp_deinit(&decomp, test_context);
  buffer_or_error result = writer_consume_all_buffers(&w);
  if (result.is_error) {
    printf("Failed to consume\n");
    return EMPTY_BUFFER;
  }
  writer_deinit(&w, test_context);
  return result.buffer;
}

bool _test_strcomp_comp_decomp(const char *src) {
  buffer compressed = _test_strcomp_compress(src);
  // printf("compressed length: %zu\n", compressed.length);
  buffer decompressed = _test_strcomp_decompress(compressed);
  int64 i = 0;
  for (i = 0; src[i] != 0; i++) {
    if (decompressed.length <= i) {
      printf("Decompressed is %zu bytes long, but the source data is longer than that.\n", decompressed.length);
      return false;
    }
    if (decompressed.data[i] != src[i]) {
      printf("At %zu, decompressed.data[i] != src[i]: %c != %c\n", i, decompressed.data[i], src[i]);
      return false;
    }
  }
  if (i != decompressed.length) {
    printf("Decompressed data is longer than source: %zu > %zu\n", decompressed.length, i);
    printf("%c %c %c\n", decompressed.data[decompressed.length - 3], decompressed.data[decompressed.length - 2], decompressed.data[decompressed.length - 1]);
    return false;
  }

  free(compressed.data);
  free(decompressed.data);

  return true;
}

const char *test_string_compression_simple() {
  TEST_ASSERT(_test_strcomp_comp_decomp("0123456789"));
  TEST_ASSERT(_test_strcomp_comp_decomp("01234567890123456789012345678901234567890123456789"));
  TEST_ASSERT(_test_strcomp_comp_decomp("0123456789A0123456789B0123456789C"));
  return NULL;
}

const char *test_string_compression_wrapping() {
  int64 buffer_length = (1 << _BSM_LENGTH_SHIFT) * 2;
  char *str = malloc(sizeof(char) * (buffer_length + 11));
  for (int64 i = 0; i < buffer_length - 15; i++) {
    str[i] = '0';
  }
  for (int64 i = 0; i < 10; i++) {
    str[i + (buffer_length - 15)] = '0' + i;
    str[i + (buffer_length - 5)] = '0' + i;
  }
  str[buffer_length + 5] = 0;
  TEST_ASSERT(_test_strcomp_comp_decomp(str));

  for (int64 i = 0; i < buffer_length + 10; i++) {
    // 8 here because if source data repeats every 8 bytes - then it will perfectly loop over the ring buffer
    // which may cause BSM to return matches that go past recently written bits into the territory of old written bits
    // and that's a special case I'd like to test
    str[i] = '0' + (i % 8);
  }
  str[buffer_length + 10] = 0;
  TEST_ASSERT(_test_strcomp_comp_decomp(str));

  free(str);
  return NULL;
}

const char *test_string_compression_allocation_failures() {
  int comp_alloc_count = 16;
  for (int i = 0; i <= comp_alloc_count; i++) {
    string_compressor comp;
    acod_encoder encoder;
    writer writer;
    setup_test_context_default();
    TEST_ASSERT(writer_init(&writer, test_context, 10, 3));
    acod_encoder_init(&encoder, &writer);
    update_test_context_for_alloc_failure(i);
    bool is_last = i == comp_alloc_count;
    TEST_ASSERT(is_last == strcomp_init(&comp, test_context, &encoder));
    if (is_last) {
      strcomp_deinit(&comp, test_context);
    }
    acod_encoder_deinit(&encoder);
    writer_deinit(&writer, test_context);
  }

  int decomp_alloc_count = 13;
  for (int i = 0; i <= decomp_alloc_count; i++) {
    string_decompressor decomp;
    acod_decoder decoder;
    writer writer;
    setup_test_context_default();
    TEST_ASSERT(writer_init(&writer, test_context, 10, 3));
    acod_decoder_init(&decoder);
    update_test_context_for_alloc_failure(i);
    bool is_last = i == decomp_alloc_count;
    TEST_ASSERT(is_last == strdecomp_init(&decomp, test_context, &writer, &decoder));
    if (is_last) {
      strdecomp_deinit(&decomp, test_context);
    }
    writer_deinit(&writer, test_context);
  }

  return NULL;
}