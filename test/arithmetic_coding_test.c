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

void maybe_rotate_buffers(writer *writer, byte **result, size_t *result_length) {
  if (writer_is_current_buffer_exhausted(writer)) {
    byte *full_buffer = writer_rotate_buffers(writer, calloc(sizeof(byte), writer_buffer_size));
    merge_byte_arrays(result, result_length, full_buffer, writer_buffer_size);
    free(full_buffer);
  }
}

byte *encode_bytes(size_t length, byte *data, size_t *result_length) {
  writer *writer = writer_new(writer_buffer_size, calloc(writer_buffer_size, sizeof(byte)), calloc(writer_buffer_size, sizeof(byte)));
  acod_encoder *encoder = acod_encoder_new(writer);
  ftable *encoding_frequencies = ftable_new(256, FTABLE_INCLUDE_EOF | FTABLE_INIT_ONE);

  *result_length = 0;
  byte *result = malloc(sizeof(byte) * 0);

  for (size_t i = 0; i < length; i++) {
    symbol symbol = data[i];
    acod_encoder_write(encoder, encoding_frequencies, symbol);
    ftable_increment(encoding_frequencies, symbol);
    maybe_rotate_buffers(writer, &result, result_length);
  }
  acod_encoder_write(encoder, encoding_frequencies, ftable_get_eof_symbol(encoding_frequencies));
  maybe_rotate_buffers(writer, &result, result_length);

  acod_encoder_delete(encoder);
  writer_deletion_result last_buffer = writer_delete(writer);
  merge_byte_arrays(&result, result_length, last_buffer.current_buffer, last_buffer.length);
  free(last_buffer.current_buffer);
  free(last_buffer.next_buffer);
  ftable_delete(encoding_frequencies);

  return result;
}

const char *_test_acod(size_t length, byte *data) {
  size_t result_length = 0;
  byte *encoded_bytes = encode_bytes(length, data, &result_length);

  ftable *decoding_frequencies = ftable_new(256, FTABLE_INCLUDE_EOF | FTABLE_INIT_ONE);
  acod_decoder *decoder = acod_decoder_new();

  size_t symbol_index = 0;
  symbol last_symbol = 0;
  symbol eof = ftable_get_eof_symbol(decoding_frequencies);
  size_t bit_index = 0;
  while (last_symbol != eof) {
    size_t current_byte_index = bit_index >> 3;
    byte current_byte = current_byte_index >= result_length ? 0 : encoded_bytes[bit_index >> 3];
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
  free(encoded_bytes);

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

const char *test_acod_allocation_failures() {
  TEST_ASSERT(error_is_present() == false, "Error must not be set at the test start");

  set_malloc_to_fail_after(0);
  acod_encoder *encoder = acod_encoder_new(NULL);
  TEST_ASSERT(error_is_present() == true, "Error must be set after allocation fails");
  TEST_ASSERT(encoder == NULL, "Encoder must be null after allocation fails");
  error_clear_last();

  set_malloc_to_fail_after(0);
  acod_decoder *decoder = acod_decoder_new();
  TEST_ASSERT(error_is_present() == true, "Error must be set after allocation fails");
  TEST_ASSERT(decoder == NULL, "Decoder must be null after allocation fails");
  error_clear_last();

  return NULL;
}