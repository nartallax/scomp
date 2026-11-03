#pragma once
#include "../../src/data_structures/writer.c"
#include "../../src/string_compression/byte_stream_matcher.c"
#include "../../src/string_compression/string_compressor.c"
#include "../../src/string_compression/string_decompressor.c"
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
  strcomp_init(&comp, test_context, &encoder);

  for (int i = 0; source[i] != 0; i++) {
    strcomp_write(&comp, source[i]);
  }

  strcomp_flush(&comp);
  strcomp_deinit(&comp);
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
  reader r;
  if (!reader_init(&r, test_context) || !reader_push_buffer(&r, src)) {
    printf("Failed to init reader\n");
    return EMPTY_BUFFER;
  }
  writer w;
  if (!writer_init(&w, test_context, 1024, 4)) {
    printf("Failed to init writer\n");
    return EMPTY_BUFFER;
  }
  acod_decoder decoder;
  acod_decoder_init(&decoder, &r);
  string_decompressor decomp;
  strdecomp_init(&decomp, test_context, &w, &decoder);

  while (strdecomp_read(&decomp)) {
    // decomp everything
  }

  strdecomp_deinit(&decomp);
  buffer_or_error result = writer_consume_all_buffers(&w);
  if (result.is_error) {
    printf("Failed to consume\n");
    return EMPTY_BUFFER;
  }
  acod_decoder_deinit(&decoder);
  writer_deinit(&w, test_context);
  reader_deinit(&r, test_context);
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

  buffer_free(decompressed, test_context);

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
