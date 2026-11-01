#pragma once
#include "../../src/asymmetrical_numeric_system/ans_encoder.c"
#include "../test_utils.c"
#include <limits.h>

INLINE byte test_ans_get_nth_bit(byte b, int64 i) {
  return b & (1 << i) ? 1 : 0;
}

INLINE byte test_ans_set_nth_bit(byte b, int64 i) {
  return b | (1 << i);
}

buffer test_ans_bit_reverse(writer *w) {
  int64 bits_written = (int64)w->current_bit_index;
  buffer src = writer_consume_all_buffers(w).buffer;

  uint64 byte_length = (bits_written + CHAR_BIT - 1) >> 3;
  byte *result = calloc(byte_length, sizeof(byte));
  for (int64 i = 0; i < bits_written; i++) {
    int64 rev_i = bits_written - i - 1;
    byte src_byte = src.data[rev_i >> 3];
    byte bit = test_ans_get_nth_bit(src_byte, rev_i & 7);
    if (bit) {
      byte res_byte = result[i >> 3];
      res_byte = test_ans_set_nth_bit(res_byte, i & 7);
      result[i >> 3] = res_byte;
    }
  }

  buffer_free(src, test_context);
  return buffer_make(result, byte_length, byte_length);
}

const char *test_ans_simple() {
  ans_encoder encoder;
  ans_decoder decoder;
  reader r;
  writer w;
  ftable enc_table;
  ftable dec_table;
  TEST_ASSERT(writer_init(&w, test_context, 1024, 4));
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(ftable_init(&enc_table, test_context, 256, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF));
  TEST_ASSERT(ftable_init(&dec_table, test_context, 256, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF));
  ans_encoder_init(&encoder, &w);
  ans_decoder_init(&decoder, &r);

  const char *msg = "Hello, world!";
  int64 msg_len = strlen(msg);
  symbol *symbols = malloc(sizeof(symbol) * msg_len);
  for (int64 i = 0; i < msg_len; i++) {
    symbols[i] = msg[i];
  }

  decoder.state = ans_encoder_write_block(&encoder, symbols, msg_len, &enc_table, false);
  // buffer encoded = writer_consume_all_buffers(&w).buffer;
  buffer encoded = test_ans_bit_reverse(&w);
  TEST_ASSERT(reader_push_buffer(&r, encoded));

  for (int64 i = 0; i < msg_len; i++) {
    symbol s = ans_decoder_read(&decoder, &dec_table);
    if (s != (symbol)msg[i]) {
      printf("At %zi, expected '%c' (%zi), but got '%c' (%u)\n", i, msg[i], (int64)msg[i], (char)s, s);
      TEST_ASSERT("wrong decoding result" && false);
    }
    // ftable_increment(&dec_table, s);
  }
  TEST_ASSERT(!context_is_errored(test_context));

  writer_deinit(&w, test_context);
  reader_deinit(&r, test_context);
  ftable_deinit(&enc_table, test_context);
  ftable_deinit(&dec_table, test_context);
  return NULL;
}