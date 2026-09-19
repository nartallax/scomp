#pragma once
#include "../src/arena.c"
#include "test_utils.c"

const char *test_arena_simple() {
  arena a;
  arena_init(&a, test_context);

  size_t first_offset = arena_allocate(&a, sizeof(byte) * 5);
  size_t second_offset = arena_allocate(&a, sizeof(byte) * 5);
  size_t third_offset = arena_allocate(&a, sizeof(byte) * 5);
  TEST_ASSERT(first_offset != second_offset);
  TEST_ASSERT(second_offset != third_offset);
  TEST_ASSERT(first_offset != third_offset);

  byte *first = arena_offset_to_pointer(&a, first_offset);
  for (int i = 0; i < 5; i++) {
    first[i] = 1;
  }

  byte *second = arena_offset_to_pointer(&a, second_offset);
  for (int i = 0; i < 5; i++) {
    second[i] = 2;
  }

  byte *third = arena_offset_to_pointer(&a, third_offset);
  for (int i = 0; i < 5; i++) {
    third[i] = 3;
  }

  for (int i = 0; i < 5; i++) {
    TEST_ASSERT(first[i] == 1);
    TEST_ASSERT(second[i] == 2);
    TEST_ASSERT(third[i] == 3);
  }

  arena_free(&a);
  arena_free(&a);
  arena_free(&a);

  byte *forth = arena_offset_to_pointer(&a, arena_allocate(&a, 10));
  byte *fifth = arena_offset_to_pointer(&a, arena_allocate(&a, 10));
  TEST_ASSERT(forth == first);
  TEST_ASSERT(fifth == third);

  size_t big_buffer_offset = arena_allocate(&a, ARENA_DEFAULT_LENGTH);
  size_t even_bigger_buffer_offset = arena_allocate(&a, ARENA_DEFAULT_LENGTH * 2);
  byte *big_buffer = arena_offset_to_pointer(&a, big_buffer_offset);
  byte *even_bigger_buffer = arena_offset_to_pointer(&a, even_bigger_buffer_offset);
  big_buffer[ARENA_DEFAULT_LENGTH - 1] = 0xff;
  even_bigger_buffer[ARENA_DEFAULT_LENGTH * 2 - 1] = 0xff; // test write to trigger valgrind

  arena_deinit(&a);
  return NULL;
}

const char *test_arena_one_big_allocation() {
  arena a;
  arena_init(&a, test_context);

  size_t really_big_buffer_offset = arena_allocate(&a, ARENA_DEFAULT_LENGTH * 10);
  byte *buffer = arena_offset_to_pointer(&a, really_big_buffer_offset);
  buffer[0] = 0;
  buffer[ARENA_DEFAULT_LENGTH * 10 - 1] = 0xff; // test writes to trigger valgrind

  arena_deinit(&a);
  return NULL;
}

const char *test_arena_allocation_failures() {
  arena a;
  setup_test_context(0);
  TEST_ASSERT(!arena_init(&a, test_context));
  TEST_ASSERT(context_is_errored(test_context));

  setup_test_context(1);
  arena_init(&a, test_context);
  size_t first_offset = arena_allocate(&a, 15);
  TEST_ASSERT(first_offset != 0);
  TEST_ASSERT(arena_allocate(&a, ARENA_DEFAULT_LENGTH) == 0);
  TEST_ASSERT(context_is_errored(test_context));

  arena_deinit(&a);

  return NULL;
}