#pragma once
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define ERROR_MESSAGE_LENGTH 512

typedef struct {
  char message[ERROR_MESSAGE_LENGTH];
  size_t message_length;
  /** False when there's no error */
  bool is_present;
} error;

error error_last = {0};

bool error_is_present() {
  return error_last.is_present;
}

error error_get_last() {
  return error_last;
}

void error_clear_last() {
  error_last.is_present = false;
  error_last.message_length = 0;
}

void error_set(const char *format, ...) {
  error_clear_last();

  va_list args;
  va_start(args, format);
  error_last.message_length = vsnprintf(error_last.message, ERROR_MESSAGE_LENGTH, format, args);
  error_last.is_present = true;
  va_end(args);
}
