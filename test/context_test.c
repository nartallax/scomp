#pragma once
#include "test_utils.c"

// there's not much to test
// as in, everything is there to test is covered by other tests

const char *test_context_double_error() {
  context_set_error(test_context, "123");
  context_set_error(test_context, "123456");
  TEST_ASSERT(context_get_error(test_context)->message_length == 3);
  return NULL;
}