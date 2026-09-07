#pragma once
#include "../src/arithmetic_coding.c"
#include "../src/writer.c"
#include "./test_utils.c"
#include <limits.h>
#include <stdlib.h>

const size_t writer_buffer_size = 1024 * 1024;

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
    // ftable_increment(encoding_frequencies, symbol);
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
    // printf("Byte at %zu is %hhu; bit index is %zu\n", current_byte_index, current_byte, bit_index);
    bit_index++;
    acod_decoder_update(decoder, current_bit);

    while (acod_decoder_has_symbol(decoder)) {
      last_symbol = acod_decoder_read(decoder, decoding_frequencies);
      if (last_symbol == eof) {
        break;
      }
      // TODO: increment your tables
      // ftable_increment(decoding_frequencies, last_symbol);
      printf("%zu, %hhu\n", last_symbol, data[symbol_index]);
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

const char *test_acod_simple() {
  srand(0xdeadbeef);
  const char *result;

  // byte one_byte[] = {53};
  // result = _test_acod(1, one_byte);
  // if (result) {
  //   return result;
  // }

  byte two_bytes[] = {50, 22};
  result = _test_acod(2, two_bytes);
  if (result) {
    return result;
  }

  // for (int i = 0; i < 10; i++) {
  //   size_t length = 1000 + (rand() / (INT_MAX / 1000));
  //   byte *input = get_random_bytes(length);
  //   const char *result = _test_acod(length, input);
  //   if (result) {
  //     return result;
  //   }
  //   free(input);
  // }
  return NULL;
}