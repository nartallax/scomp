#pragma once
#include "../src/stack.c"
#include "test_utils.c"
#include <stdbool.h>
#include <string.h>

void _test_stack_push(stack *s, const char *value) {
  const char **slot = stack_push(s);
  *slot = value;
}

bool _test_stack_strcmp(const char **slot, const char *value) {
  return strcmp(*slot, value) == 0;
}

const char *test_stack_simple() {
  stack *s = malloc(sizeof(stack));
  stack_init(s, test_context, sizeof(const char *));
  TEST_ASSERT(stack_get_count(s) == 0);

  _test_stack_push(s, "first");
  TEST_ASSERT(stack_get_count(s) == 1);

  _test_stack_push(s, "second");
  _test_stack_push(s, "third");
  TEST_ASSERT(stack_get_count(s) == 3);
  TEST_ASSERT(_test_stack_strcmp(stack_peek(s), "third"));
  TEST_ASSERT(_test_stack_strcmp(stack_pop(s), "third"));
  TEST_ASSERT(stack_get_count(s) == 2);
  TEST_ASSERT(_test_stack_strcmp(stack_peek(s), "second"));
  TEST_ASSERT(_test_stack_strcmp(stack_peek(s), "second"));
  TEST_ASSERT(_test_stack_strcmp(stack_pop(s), "second"));
  TEST_ASSERT(_test_stack_strcmp(stack_pop(s), "first"));
  TEST_ASSERT(stack_get_count(s) == 0);

  for (size_t i = 0; i < STACK_DEFAULT_LENGTH * 2 + 5; i++) {
    _test_stack_push(s, "value");
  }
  TEST_ASSERT(stack_get_count(s) == STACK_DEFAULT_LENGTH * 2 + 5);

  for (size_t i = 0; i < STACK_DEFAULT_LENGTH * 2 + 3; i++) {
    TEST_ASSERT(_test_stack_strcmp(stack_peek(s), "value"));
    TEST_ASSERT(_test_stack_strcmp(stack_pop(s), "value"));
  }
  TEST_ASSERT(stack_get_count(s) == 2);

  stack_deinit(s);
  free(s);
  return NULL;
}

typedef struct {
  size_t x;
  size_t y;
} test_stack_obj;

const char *test_stack_non_pointer() {
  stack *s = malloc(sizeof(stack));
  stack_init(s, test_context, sizeof(test_stack_obj));
  test_stack_obj *o = stack_push(s);
  TEST_ASSERT(stack_get_count(s) == 1);
  o->x = 1;
  o->y = 2;

  o = stack_push(s);
  TEST_ASSERT(stack_get_count(s) == 2);
  o->x = 3;
  o->y = 4;

  o = stack_pop(s);
  TEST_ASSERT(stack_get_count(s) == 1);
  TEST_ASSERT(o->x == 3 && o->y == 4);

  o = stack_pop(s);
  TEST_ASSERT(stack_get_count(s) == 0);
  TEST_ASSERT(o->x == 1 && o->y == 2);

  for (size_t i = 0; i < STACK_DEFAULT_LENGTH * 2 + 5; i++) {
    o = stack_push(s);
    o->x = i;
    o->y = i * 2;
  }
  TEST_ASSERT(stack_get_count(s) == STACK_DEFAULT_LENGTH * 2 + 5);

  for (size_t i = STACK_DEFAULT_LENGTH * 2 + 4; i >= 2; i--) {
    o = stack_pop(s);
    TEST_ASSERT(o->x == i && o->y == i * 2);
  }
  TEST_ASSERT(stack_get_count(s) == 2);

  stack_deinit(s);
  free(s);
  return NULL;
}

const char *test_stack_allocation_failures() {
  stack *s = malloc(sizeof(stack));

  setup_test_context(0);
  TEST_ASSERT(!stack_init(s, test_context, sizeof(const char *)));
  TEST_ASSERT(context_is_errored(test_context));

  setup_test_context(1);
  stack_init(s, test_context, sizeof(const char *));
  for (size_t i = 0; i < STACK_DEFAULT_LENGTH - 1; i++) {
    stack_push(s);
  }
  TEST_ASSERT(stack_push(s) == NULL);
  TEST_ASSERT(context_is_errored(test_context));

  stack_deinit(s);
  free(s);
  return NULL;
}