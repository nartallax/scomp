#pragma once
#include "../../src/arithmetic_coding/decoder.c"
#include "../../src/arithmetic_coding/encoder.c"
#include "../../src/data_structures/writer.c"
#include "../test_utils.c"
#include <limits.h>
#include <stdlib.h>

constexpr int64 ACOD_TEST_WRITER_BUFFER_SIZE = 1024;

byte *get_random_bytes(int64 length) {
  byte *result = malloc(sizeof(byte) * length);
  for (int64 i = 0; i < length; i++) {
    result[i] = rand() & 0xff;
  }
  return result;
}

buffer encode_bytes(int64 length, byte *data) {
  writer *w = malloc(sizeof(writer));
  if (!writer_init(w, test_context, ACOD_TEST_WRITER_BUFFER_SIZE, 4)) {
    return EMPTY_BUFFER;
  }

  acod_encoder *encoder = malloc(sizeof(acod_encoder));
  acod_encoder_init(encoder, w);
  ftable encoding_frequencies;
  if (!ftable_init(&encoding_frequencies, test_context, 256, FTABLE_INCLUDE_EOF | FTABLE_INIT_ONE)) {
    return EMPTY_BUFFER;
  }

  for (int64 i = 0; i < length; i++) {
    symbol symbol = data[i];
    acod_encoder_write(encoder, &encoding_frequencies, symbol);
    ftable_increment(&encoding_frequencies, symbol);
  }
  acod_encoder_write(encoder, &encoding_frequencies, ftable_get_eof_symbol(&encoding_frequencies));

  acod_encoder_deinit(encoder);
  buffer_or_error result = writer_consume_all_buffers(w);
  writer_deinit(w, test_context);
  free(w);
  ftable_deinit(&encoding_frequencies, test_context);
  free(encoder);

  return result.buffer;
}

const char *_test_acod(int64 length, byte *data) {
  buffer encoded_bytes = encode_bytes(length, data);
  reader r;
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(reader_push_buffer(&r, encoded_bytes));

  ftable decoding_frequencies;
  TEST_ASSERT(ftable_init(&decoding_frequencies, test_context, 256, FTABLE_INCLUDE_EOF | FTABLE_INIT_ONE));
  acod_decoder *decoder = malloc(sizeof(acod_decoder));
  acod_decoder_init(decoder, &r);

  int64 symbol_index = 0;
  symbol last_symbol = 0;
  symbol eof = ftable_get_eof_symbol(&decoding_frequencies);
  while (last_symbol != eof) {
    last_symbol = acod_decoder_read(decoder, &decoding_frequencies);
    if (last_symbol == eof) {
      break;
    }

    ftable_increment(&decoding_frequencies, last_symbol);
    TEST_ASSERT(symbol_index < length); // for case when we were supposed to read EOF but didn't
    TEST_ASSERT(last_symbol == data[symbol_index]);
    TEST_ASSERT(symbol_index < length);
    symbol_index++;
  }

  ftable_deinit(&decoding_frequencies, test_context);
  free(encoded_bytes.data);
  free(decoder);

  return NULL;
}

byte *get_nonuniform_random_bytes(int64 length) {
  symbol symbol_count = (rand() & 0xff) + 1;

  // create distribution
  symbol_frequency *freqs = calloc(symbol_count, sizeof(symbol_frequency));
  symbol_frequency sum = 0;
  for (symbol i = 0; i < symbol_count; i++) {
    freqs[i] = (rand() & 0xffff) + 1;
    sum += freqs[i];
  }
  symbol_frequency total = sum;

  // Rescale frequencies
  sum = 0;
  symbol index = 0;
  for (symbol i = 0; i < symbol_count; i++) {
    sum += freqs[i];
    symbol new_index = (length - symbol_count) * sum / total + i + 1;
    freqs[i] = new_index - index;
    index = new_index;
  }
  assert(length == index && "Failed to properly rescale frequencies");

  // create message
  byte *message = malloc(sizeof(byte) * length);
  int64 j = 0;
  for (symbol i = 0; i < symbol_count; i++) {
    for (symbol_frequency freq = 0; freq < freqs[i]; freq++) {
      message[j] = (byte)i;
      j++;
    }
  }

  for (int64 i = 0; i < length; i++) {
    int64 k = (rand() % (length - i)) + i;
    byte tmp = message[i];
    message[i] = message[k];
    message[k] = tmp;
  }

  free(freqs);

  return message;
}

const char *test_acod_simple() {
  srand(0xdeadbeef);
  const char *result;

  byte *zero_bytes = malloc(0);
  result = _test_acod(0, zero_bytes);
  if (result) {
    return result;
  }
  free(zero_bytes);

  byte one_byte[] = {53};
  result = _test_acod(1, one_byte);
  if (result) {
    return result;
  }

  byte two_bytes[] = {50, 22};
  result = _test_acod(2, two_bytes);
  if (result) {
    return result;
  }

  // underflow
  byte underflow_bytes[] = {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2};
  result = _test_acod(sizeof(underflow_bytes), underflow_bytes);
  if (result) {
    return result;
  }

  // every byte
  byte *every_byte = malloc(sizeof(byte) * 256);
  for (int64 i = 0; i < 256; i++) {
    every_byte[i] = (byte)i;
  }
  result = _test_acod(sizeof(underflow_bytes), underflow_bytes);
  if (result) {
    return result;
  }
  free(every_byte);

  // uniform random
  for (int i = 0; i < 10; i++) {
    int64 length = 10000 + (rand() / (INT_MAX / 10000));
    byte *input = get_random_bytes(length);
    const char *result = _test_acod(length, input);
    if (result) {
      return result;
    }
    free(input);
  }

  // non-uniform random
  for (int i = 0; i < 10; i++) {
    int64 length = 10000 + (rand() / (INT_MAX / 10000));
    byte *input = get_nonuniform_random_bytes(length);
    const char *result = _test_acod(length, input);
    if (result) {
      return result;
    }
    free(input);
  }

  return NULL;
}

NODISCARD bool _acod_test_fill_writer_bits(writer *w, int64 bit_count) {
  for (int64 i = 0; i < bit_count; i++) {
    writer_write_bit(w, 0);
  }
  return true;
}
