#pragma once
#include "../src/queue.c"
#include "test_utils.c"
#include <string.h>

const char *test_queue_simple() {
  queue *q = queue_new(test_context);
  TEST_ASSERT(queue_get_count(q) == 0, "Empty queue should have length of zero");

  queue_push(q, "first");
  TEST_ASSERT(queue_get_count(q) == 1, "Single-value queue should have length of 1");

  queue_push(q, "second");
  queue_push(q, "third");
  queue_push(q, "forth");
  TEST_ASSERT(queue_get_count(q) == 4, "Queue length should have expected value");
  TEST_ASSERT(strcmp(queue_peek(q), "first") == 0, "Peek should return expected value");
  TEST_ASSERT(strcmp(queue_peek(q), "first") == 0, "Peek should not remove values");
  TEST_ASSERT(strcmp(queue_pop(q), "first") == 0, "Pop should return expected value");
  TEST_ASSERT(strcmp(queue_pop(q), "second") == 0, "Pop should return expected value");
  TEST_ASSERT(queue_get_count(q) == 2, "Queue length should have expected value after pops");
  TEST_ASSERT(strcmp(queue_pop(q), "third") == 0, "Pop should return expected value");
  TEST_ASSERT(strcmp(queue_pop(q), "forth") == 0, "Pop should return expected value");
  TEST_ASSERT(queue_get_count(q) == 0, "Empty queue should have length of zero");

  queue_delete(q);

  return NULL;
}

const char *test_queue_overflow_while_wrapping() {
  queue *q = queue_new(test_context);

  for (size_t i = 0; i < QUEUE_DEFAULT_SIZE - 1; i++) {
    queue_push(q, "a");
  }

  TEST_ASSERT(queue_get_count(q) == QUEUE_DEFAULT_SIZE - 1, "Count should have expected value");

  queue_pop(q);
  queue_pop(q);
  queue_pop(q);

  TEST_ASSERT(queue_get_count(q) == QUEUE_DEFAULT_SIZE - 4, "Count should have expected value");
  TEST_ASSERT(q->length == QUEUE_DEFAULT_SIZE, "Length should not have grown yet");

  for (size_t i = 0; i < 7; i++) {
    queue_push(q, "b");
  }

  TEST_ASSERT(queue_get_count(q) == QUEUE_DEFAULT_SIZE + 3, "Count should have expected value");
  TEST_ASSERT(q->length == QUEUE_DEFAULT_SIZE * 2, "Length should have grown");
  for (size_t i = 0; i < QUEUE_DEFAULT_SIZE - 4; i++) {
    TEST_ASSERT(strcmp(queue_pop(q), "a") == 0, "This pop should return 'a'");
  }
  for (size_t i = 0; i < 7; i++) {
    TEST_ASSERT(strcmp(queue_pop(q), "b") == 0, "This pop should return 'b'");
  }
  TEST_ASSERT(queue_get_count(q) == 0, "Count should have expected value");

  for (size_t i = 0; i < QUEUE_DEFAULT_SIZE * 2 + 3; i++) {
    queue_push(q, "c");
  }
  TEST_ASSERT(queue_get_count(q) == (QUEUE_DEFAULT_SIZE * 2) + 3, "Count should have expected value");
  TEST_ASSERT(q->length == QUEUE_DEFAULT_SIZE * 4, "Length should have grown again");
  for (size_t i = 0; i < QUEUE_DEFAULT_SIZE * 2 + 3; i++) {
    TEST_ASSERT(strcmp(queue_pop(q), "c") == 0, "This pop should return 'c'");
  }

  queue_delete(q);
  return NULL;
}

const char *test_queue_overflow_while_not_wrapping() {
  queue *q = queue_new(test_context);

  TEST_ASSERT(q->length == QUEUE_DEFAULT_SIZE, "Length should not have grown yet");
  for (size_t i = 0; i < QUEUE_DEFAULT_SIZE * 2 + 5; i++) {
    queue_push(q, "a");
  }

  TEST_ASSERT(queue_get_count(q) == QUEUE_DEFAULT_SIZE * 2 + 5, "Count should have expected value");
  TEST_ASSERT(q->length == QUEUE_DEFAULT_SIZE * 4, "Length should have grown");
  for (size_t i = 0; i < QUEUE_DEFAULT_SIZE * 2 + 5; i++) {
    TEST_ASSERT(strcmp(queue_pop(q), "a") == 0, "This pop should return 'a'");
  }

  queue_delete(q);
  return NULL;
}

const char *test_queue_underflow() {
  queue *q = queue_new(test_context);
  TEST_ASSERT(context_is_errored(test_context) == false, "There should be no error on start");

  queue_push(q, "a");
  queue_pop(q);
  TEST_ASSERT(queue_pop(q) == NULL, "Pop on empty queue should return null");
  TEST_ASSERT(context_is_errored(test_context) == true, "Queue underflow should error");
  context_clear_error(test_context);

  TEST_ASSERT(queue_peek(q) == NULL, "Peek on empty queue should return null");
  TEST_ASSERT(context_is_errored(test_context) == true, "Queue underflow on peek should error");

  queue_delete(q);
  return NULL;
}

const char *test_queue_allocation_failures() {
  setup_test_context(0);
  queue *q;

  q = queue_new(test_context);
  TEST_ASSERT(q == NULL, "Queue should be null on first allocation fail");

  setup_test_context(1);
  q = queue_new(test_context);
  TEST_ASSERT(q == NULL, "Queue should be null on second allocation fail");

  setup_test_context(2);
  q = queue_new(test_context);
  TEST_ASSERT(context_is_errored(test_context) == false, "There should be no error just yet");
  for (size_t i = 0; i < QUEUE_DEFAULT_SIZE + 1; i++) {
    queue_push(q, "a");
  }
  TEST_ASSERT(context_is_errored(test_context) == true, "There should be an error on queue overflow allocation fail");

  queue_delete(q);

  return NULL;
}