#pragma once
#include "../../src/arithmetic_coding/uint_reader.c"
#include "../../src/arithmetic_coding/uint_writer.c"
#include "../test_utils.c"
#include <stdint.h>

const char *test_uint_writer_case(uint64 src_value) {
  uint_freq_tables tables;
  uint_freq_tables_init(&tables, test_context, UINT64_MAX);
  writer w;
  TEST_ASSERT(writer_init(&w, test_context, 1024, 4));
  acod_encoder encoder;
  acod_encoder_init(&encoder, &w);
  uint_writer_write(&tables, &encoder, src_value);
  acod_encoder_deinit(&encoder);
  buffer b = writer_consume_all_buffers(&w).buffer;
  uint_freq_tables_deinit(&tables);
  writer_deinit(&w, test_context);

  reader r;
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(reader_push_buffer(&r, b));
  uint_freq_tables_init(&tables, test_context, UINT64_MAX);
  acod_decoder decoder;
  acod_decoder_init(&decoder, &r);
  uint64 read_result = uint_reader_read(&tables, &decoder);
  uint_freq_tables_deinit(&tables);
  reader_deinit(&r, test_context);
  acod_decoder_deinit(&decoder);

  TEST_ASSERT(read_result == src_value);
  return NULL;
}

const char *test_uint_writer_simple() {
  TEST_ASSERT(!test_uint_writer_case(5067680002));
  return NULL;
}