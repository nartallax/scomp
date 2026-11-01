#pragma once
#include "../test_constants.c"
#include "../test_utils.c"
#include "arithmetic_coding_test.c"

// TODO: let's avoid repeating this code almost exactly in every perf test and have a helper function instead
const char *test_acod_on_a_file(const char *src_path) {
  buffer file = test_read_file_to_buffer(src_path);

  TIME_STRUCT comp_start_time = get_current_time();
  buffer compressed = test_acod_encode_bytes(file.length, file.data);
  TIME_STRUCT comp_end_time = get_current_time();
  double compression_ratio = ((double)compressed.length / (double)file.length);
  double comp_seconds = get_time_diff(comp_start_time, comp_end_time);
  printf("Acod: compressed %s from %zu to %zu (%.02f%%) in %.03f seconds\n", src_path, file.length, compressed.length, compression_ratio * 100.0, comp_seconds);

  TIME_STRUCT decomp_start_time = get_current_time();
  buffer decompressed = test_acod_decode_bytes(compressed);
  TIME_STRUCT decomp_end_time = get_current_time();
  double decomp_seconds = get_time_diff(decomp_start_time, decomp_end_time);
  printf("Acod: decompressed %s in %.03f seconds\n", src_path, decomp_seconds);

  TEST_ASSERT(test_compare_buffers(src_path, file, decompressed));

  buffer_free(file, test_context);
  buffer_free(decompressed, test_context);
  return NULL;
}

const char *test_acod_perf() {
  // TODO: raw jcomp, without any object/array shape fixes, is worse than raw strcomp
  // (probably because we aren't able to include json's control symbols in strcomp, and are constantly interrupting strcomp)
  // so what we might want to do - continue compressing incoming json with strcomp, until we have a structure we can substitute
  // (this would require maintaining JSON state in the background, without it affecting anything in the output)
  // not well though-out idea, but feels like it maaay just work
  for (memsize i = 0; i < sizeof(perf_test_files) / sizeof(perf_test_files[0]); i++) {
    const char *src_file = perf_test_files[i];
    const char *result = test_acod_on_a_file(src_file);
    if (result) {
      return result;
    }
  }

  return NULL;
}