#pragma once
#include "../../src/data_structures/writer.c"
#include "../../src/string_compression/string_compressor.c"
#include "../../src/string_compression/string_decompressor.c"
#include "../test_utils.c"
#include <limits.h>

const char *strcomp_perf_test_process_buffer(int64 *total_bytes_compressed, buffer b, string_decompressor *decomp) {
  *total_bytes_compressed += b.length;

  for (int64 i = 0; i < b.length; i++) {
    byte c = b.data[i];
    for (int j = 0; j < CHAR_BIT; j++) {
      acod_decoder_update(decomp->decoder, c & 1);
      TEST_ASSERT(!strdecomp_read(decomp));
      c = c >> 1;
    }
  }

  return NULL;
}

void strcomp_perf_test_write_zeroes_until_eof(string_decompressor *decomp) {
  while (true) {
    acod_decoder_update(decomp->decoder, 0);
    if (!strdecomp_read(decomp)) {
      return;
    }
  }
}

const char *strcomp_perf_test_compare_to_file(buffer b, const char *path) {
  // FILE *out_file = fopen("/tmp/out.json", "w");
  // TEST_ASSERT(fwrite(b.data, 1, b.length, out_file) == b.length);
  // fclose(out_file);

  FILE *src_file = fopen(path, "r");
  TEST_ASSERT(src_file);

  bool is_success = true;
  int64 bytes_read;
  char read_buffer[4096];
  int64 buffer_pos = 0;
  while (is_success && (bytes_read = fread(read_buffer, 1, sizeof(read_buffer), src_file)) > 0) {
    if (b.length < buffer_pos + bytes_read) {
      is_success = false;
      break;
    }
    for (int64 i = 0; i < bytes_read; i++) {
      if (b.data[buffer_pos + i] != (byte)read_buffer[i]) {
        printf("At %zu, expected %c (%i), but got %c (%i)\n", buffer_pos + i, read_buffer[i], read_buffer[i], b.data[buffer_pos + i], b.data[buffer_pos + i]);
        is_success = false;
        break;
      }
    }
    buffer_pos += bytes_read;
  }
  fclose(src_file);

  TEST_ASSERT(is_success);
  return NULL;
}

const char *test_string_compression_on_a_file(const char *path) {
  TIME_STRUCT start_time = get_current_time();
  writer comp_writer;
  TEST_ASSERT(writer_init(&comp_writer, test_context, 16 * 1024, 4));
  writer decomp_writer;
  TEST_ASSERT(writer_init(&decomp_writer, test_context, 16 * 1024, 4));
  acod_encoder encoder;
  acod_encoder_init(&encoder, &comp_writer);
  acod_decoder decoder;
  acod_decoder_init(&decoder);
  string_compressor comp;
  TEST_ASSERT(strcomp_init(&comp, test_context, &encoder));
  string_decompressor decomp;
  TEST_ASSERT(strdecomp_init(&decomp, test_context, &decomp_writer, &decoder));

  FILE *src_file = fopen(path, "r");
  TEST_ASSERT(src_file);

  bool is_success = true;
  int64 bytes_read;
  int64 total_bytes_compressed = 0;
  int64 total_bytes_read = 0;
  char read_buffer[4096];
  while (is_success && (bytes_read = fread(read_buffer, 1, sizeof(read_buffer), src_file)) > 0) {
    total_bytes_read += bytes_read;
    for (int64 i = 0; i < bytes_read; i++) {
      strcomp_write(&comp, read_buffer[i]);

      buffer b;
      while (true) {
        b = writer_consume_full_buffer(&comp_writer);
        if (b.length == 0) {
          break;
        }

        TEST_ASSERT(!strcomp_perf_test_process_buffer(&total_bytes_compressed, b, &decomp));
      }
    }

    if (!is_success) {
      break;
    }
  }
  fclose(src_file);

  strcomp_flush(&comp);
  strcomp_deinit(&comp, test_context);
  acod_encoder_deinit(&encoder);
  buffer_or_error result = writer_consume_all_buffers(&comp_writer);
  TEST_ASSERT(!result.is_error);
  TEST_ASSERT(!strcomp_perf_test_process_buffer(&total_bytes_compressed, result.buffer, &decomp));
  strcomp_perf_test_write_zeroes_until_eof(&decomp);
  writer_deinit(&comp_writer, test_context);

  strdecomp_deinit(&decomp, test_context);
  buffer_or_error decomp_result = writer_consume_all_buffers(&decomp_writer);
  TEST_ASSERT(!decomp_result.is_error);
  TEST_ASSERT(!strcomp_perf_test_compare_to_file(decomp_result.buffer, path));

  TIME_STRUCT end_time = get_current_time();
  double timediff_seconds = get_time_diff(start_time, end_time);

  double compression_ratio = ((double)total_bytes_compressed / (double)total_bytes_read);
  printf("Compressed %s from %zu to %zu (%.02f%%) in %.03f seconds\n", path, total_bytes_read, total_bytes_compressed, compression_ratio * 100.0, timediff_seconds);

  TEST_ASSERT(compression_ratio < 0.25);

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