#pragma once
#include "../../src/string_compression/compressor.c"
#include "../../src/string_compression/decompressor.c"
#include "../../src/writer.c"
#include "../test_utils.c"
#include <limits.h>

buffer _test_strcomp_compress(const char *source) {
  writer w;
  if (!writer_init(&w, test_context, 1024)) {
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
  if (!writer_init(&w, test_context, 1024)) {
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
  for (size_t i = 0; i < src.length; i++) {
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
  size_t i = 0;
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
    printf("%c %c %c\n", decompressed.data[48], decompressed.data[49], decompressed.data[50]);
    return false;
  }

  free(compressed.data);
  free(decompressed.data);

  return true;
}

const char *test_string_compression_simple() {
  // TODO: revive the test
  // TEST_ASSERT(_test_strcomp_comp_decomp("0123456789"));
  // TEST_ASSERT(_test_strcomp_comp_decomp("01234567890123456789012345678901234567890123456789"));
  return NULL;
}

const char *test_string_compression_allocation_failures() {
  return NULL;
}