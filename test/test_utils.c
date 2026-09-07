#pragma once
#include "../src/commons.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ASSERT(condition, comment)                                                                                                                                                                \
  do {                                                                                                                                                                                                 \
    if (!(condition)) {                                                                                                                                                                                \
      return (comment);                                                                                                                                                                                \
    }                                                                                                                                                                                                  \
  } while (0)

void merge_byte_arrays(byte **receiver, size_t *receiver_length, byte *b, size_t b_len) {
  byte *old_receiver = *receiver;
  byte *result = malloc(sizeof(byte) * (*receiver_length + b_len));
  memcpy(result, *receiver, *receiver_length);
  memcpy(result + *receiver_length, b, b_len);
  *receiver = result;
  *receiver_length += b_len;
  free(old_receiver);
}