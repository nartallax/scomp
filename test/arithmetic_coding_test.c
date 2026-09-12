#pragma once
#include "../src/arithmetic_coding.c"
#include "../src/writer.c"
#include "./test_utils.c"
#include <limits.h>
#include <stdlib.h>

const size_t writer_buffer_size = 1024;

byte *get_random_bytes(size_t length) {
  byte *result = malloc(sizeof(byte) * length);
  for (size_t i = 0; i < length; i++) {
    result[i] = rand() & 0xff;
  }
  return result;
}

buffer encode_bytes(size_t length, byte *data) {
  writer *writer = writer_new(test_context, writer_buffer_size);
  acod_encoder *encoder = acod_encoder_new(test_context, writer);
  ftable *encoding_frequencies = ftable_new(test_context, 256, FTABLE_INCLUDE_EOF | FTABLE_INIT_ONE);

  for (size_t i = 0; i < length; i++) {
    symbol symbol = data[i];
    acod_encoder_write(encoder, encoding_frequencies, symbol);
    ftable_increment(encoding_frequencies, symbol);
  }
  acod_encoder_write(encoder, encoding_frequencies, ftable_get_eof_symbol(encoding_frequencies));

  acod_encoder_delete(encoder);
  buffer result = writer_consume_all_buffers(writer);
  writer_delete(writer);
  ftable_delete(encoding_frequencies);

  return result;
}

const char *_test_acod(size_t length, byte *data) {
  buffer encoded_bytes = encode_bytes(length, data);

  ftable *decoding_frequencies = ftable_new(test_context, 256, FTABLE_INCLUDE_EOF | FTABLE_INIT_ONE);
  acod_decoder *decoder = acod_decoder_new(test_context);

  size_t symbol_index = 0;
  symbol last_symbol = 0;
  symbol eof = ftable_get_eof_symbol(decoding_frequencies);
  size_t bit_index = 0;
  while (last_symbol != eof) {
    size_t current_byte_index = bit_index >> 3;
    byte current_byte = current_byte_index >= encoded_bytes.length ? 0 : encoded_bytes.data[bit_index >> 3];
    byte current_bit = (current_byte & (1 << (bit_index & 7))) ? 1 : 0;
    bit_index++;
    acod_decoder_update(decoder, current_bit);

    while (acod_decoder_has_symbol(decoder)) {
      last_symbol = acod_decoder_read(decoder, decoding_frequencies);
      if (last_symbol == eof) {
        break;
      }

      ftable_increment(decoding_frequencies, last_symbol);
      TEST_ASSERT(last_symbol == data[symbol_index], "Decoded symbol must be equal to source symbol");
      TEST_ASSERT(symbol_index < length, "Decoder must read exactly as much symbols as was written");
      symbol_index++;
    }
  }

  acod_decoder_delete(decoder);
  ftable_delete(decoding_frequencies);
  free(encoded_bytes.data);

  return NULL;
}

byte *get_nonuniform_random_bytes(size_t length) {
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
  size_t j = 0;
  for (symbol i = 0; i < symbol_count; i++) {
    for (symbol_frequency freq = 0; freq < freqs[i]; freq++) {
      message[j] = (byte)i;
      j++;
    }
  }

  for (size_t i = 0; i < length; i++) {
    size_t k = (rand() % (length - i)) + i;
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
  for (size_t i = 0; i < 256; i++) {
    every_byte[i] = (byte)i;
  }
  result = _test_acod(sizeof(underflow_bytes), underflow_bytes);
  if (result) {
    return result;
  }
  free(every_byte);

  // uniform random
  for (int i = 0; i < 10; i++) {
    size_t length = 10000 + (rand() / (INT_MAX / 10000));
    byte *input = get_random_bytes(length);
    const char *result = _test_acod(length, input);
    if (result) {
      return result;
    }
    free(input);
  }

  // non-uniform random
  for (int i = 0; i < 10; i++) {
    size_t length = 10000 + (rand() / (INT_MAX / 10000));
    byte *input = get_nonuniform_random_bytes(length);
    const char *result = _test_acod(length, input);
    if (result) {
      return result;
    }
    free(input);
  }

  return NULL;
}

void _acod_test_fill_writer_bits(writer *w, size_t bit_count) {
  for (size_t i = 0; i < bit_count; i++) {
    writer_write_bit(w, 0);
  }
}

const char *test_acod_allocation_failures() {
  TEST_ASSERT(context_is_errored(test_context) == false, "Error must not be set at the test start");

  setup_test_context(0);
  acod_decoder *decoder = acod_decoder_new(test_context);
  TEST_ASSERT(context_is_errored(test_context) == true, "Error must be set after allocation fails");
  TEST_ASSERT(decoder == NULL, "Decoder must be null after allocation fails");

  setup_test_context(0);
  acod_encoder *encoder = acod_encoder_new(test_context, NULL);
  TEST_ASSERT(context_is_errored(test_context) == true, "Error must be set after allocation fails");
  TEST_ASSERT(encoder == NULL, "Encoder must be null after allocation fails");

  // this tests for error on shift bit write
  {
    setup_test_context_default();
    ftable *bit_table = ftable_new(test_context, 2, FTABLE_EXCLUDE_EOF | FTABLE_INIT_ONE);
    writer *w = writer_new(test_context, 2);
    encoder = acod_encoder_new(test_context, w);
    TEST_ASSERT(context_is_errored(test_context) == false, "Initial allocation successful");

    _acod_test_fill_writer_bits(w, 15);
    update_test_context_for_alloc_failure(0);
    TEST_ASSERT(!acod_encoder_write(encoder, bit_table, 0), "Write should return false on allocation fail");
    TEST_ASSERT(context_is_errored(test_context) == true, "Errored after write fail");

    ftable_delete(bit_table);
    writer_delete(w);
    acod_encoder_delete(encoder);
  }

  // this tests for error on underflow bit write
  {
    setup_test_context_default();
    ftable *bit_table = ftable_new(test_context, 2, FTABLE_EXCLUDE_EOF | FTABLE_INIT_ONE);
    writer *w = writer_new(test_context, 2);
    encoder = acod_encoder_new(test_context, w);
    TEST_ASSERT(context_is_errored(test_context) == false, "Not errored yet");

    // this feels slightly like cheating, because I couldn't pick the right input data to make it underflow
    // but I won't compromise 100% code coverage just because of that
    encoder->underflows++;
    _acod_test_fill_writer_bits(w, 14);
    update_test_context_for_alloc_failure(0);
    TEST_ASSERT(!acod_encoder_write(encoder, bit_table, 0), "Write should return false on allocation fail");
    TEST_ASSERT(context_is_errored(test_context) == true, "Errored after write fail");

    ftable_delete(bit_table);
    writer_delete(w);
    acod_encoder_delete(encoder);
  }

  // this tests for error on closing bit writes
  {
    setup_test_context_default();
    ftable *bit_table = ftable_new(test_context, 2, FTABLE_EXCLUDE_EOF | FTABLE_INIT_ONE);
    writer *w = writer_new(test_context, 2);
    encoder = acod_encoder_new(test_context, w);
    TEST_ASSERT(context_is_errored(test_context) == false, "Not errored yet");

    // this feels slightly like cheating, because I couldn't pick the right input data to make it underflow
    // but I won't compromise 100% code coverage just because of that
    encoder->underflows++;
    _acod_test_fill_writer_bits(w, 13);
    update_test_context_for_alloc_failure(0);
    TEST_ASSERT(acod_encoder_write(encoder, bit_table, 0), "Normal write should succeed");
    TEST_ASSERT(context_is_errored(test_context) == false, "No error just yet");

    ftable_delete(bit_table);
    writer_delete(w);
    TEST_ASSERT(!acod_encoder_delete(encoder), "Final writes should fail");
  }

  return NULL;
}