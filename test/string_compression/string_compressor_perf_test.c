#pragma once
#include "../../src/string_compression/compressor.c"
#include "../../src/writer.c"
#include "../test_utils.c"

const char *test_string_compression_on_a_file(const char *path) {
  TIME_STRUCT start_time = get_current_time();
  writer w;
  TEST_ASSERT(writer_init(&w, test_context, 16 * 1024, 4));
  acod_encoder encoder;
  acod_encoder_init(&encoder, &w);
  string_compressor comp;
  TEST_ASSERT(strcomp_init(&comp, test_context, &encoder));

  FILE *src_file = fopen(path, "r");
  TEST_ASSERT(src_file);

  bool is_success = true;
  size_t bytes_read;
  size_t total_bytes_compressed = 0;
  size_t total_bytes_read = 0;
  char read_buffer[4096];
  while (is_success && (bytes_read = fread(read_buffer, 1, sizeof(read_buffer), src_file)) > 0) {
    total_bytes_read += bytes_read;
    for (size_t i = 0; i < bytes_read; i++) {
      strcomp_write(&comp, read_buffer[i]);

      buffer b;
      while (true) {
        b = writer_consume_full_buffer(&w);
        if (b.length == 0) {
          break;
        }
        total_bytes_compressed += b.length;
      }
    }

    if (!is_success) {
      break;
    }
  }

  strcomp_flush(&comp);
  strcomp_deinit(&comp, test_context);
  acod_encoder_deinit(&encoder);
  buffer_or_error result = writer_consume_all_buffers(&w);
  TEST_ASSERT(!result.is_error);
  writer_deinit(&w, test_context);

  TIME_STRUCT end_time = get_current_time();
  double timediff_seconds = get_time_diff(start_time, end_time);

  printf("Compressed %s from %zu to %zu (%.02f%%) in %.03f seconds\n", path, total_bytes_read, total_bytes_compressed, ((double)total_bytes_compressed / (double)total_bytes_read) * 100.0,
         timediff_seconds);

  return NULL;
}

const char *test_string_compressor_perf() {
  const char *result;

  result = test_string_compression_on_a_file("./test/test_data/fda_recalls.json");
  if (result) {
    return result;
  }

  result = test_string_compression_on_a_file("./test/test_data/ru_news_feed.json");
  if (result) {
    return result;
  }

  return NULL;
}