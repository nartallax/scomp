#pragma once
#include "../../src/data_structures/arena.c"
#include "../test_utils.c"

const char *test_arena_simple() {
  arena a;
  TEST_ASSERT(arena_init(&a, test_context, 1024));

  byte *first = arena_allocate(&a, 5, sizeof(byte));
  byte *second = arena_allocate(&a, 5, sizeof(byte));
  byte *third = arena_allocate(&a, 5, sizeof(byte));
  TEST_ASSERT(first != second);
  TEST_ASSERT(second != third);
  TEST_ASSERT(first != third);

  for (int i = 0; i < 5; i++) {
    first[i] = 1;
  }

  for (int i = 0; i < 5; i++) {
    second[i] = 2;
  }

  for (int i = 0; i < 5; i++) {
    third[i] = 3;
  }

  for (int i = 0; i < 5; i++) {
    TEST_ASSERT(first[i] == 1);
    TEST_ASSERT(second[i] == 2);
    TEST_ASSERT(third[i] == 3);
  }

  arena_deinit(&a, test_context);
  return NULL;
}

const char *test_arena_allocation_failures() {
  arena a;
  setup_test_context(0);
  TEST_ASSERT(!arena_init(&a, test_context, 1024));
  TEST_ASSERT(context_is_errored(test_context));

  setup_test_context(1);
  TEST_ASSERT(arena_init(&a, test_context, 1024));
  TEST_ASSERT(!context_is_errored(test_context));

  arena_deinit(&a, test_context);

  return NULL;
}