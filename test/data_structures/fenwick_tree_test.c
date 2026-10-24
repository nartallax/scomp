#pragma once
#include "../../src/data_structures/fenwick_tree.c"
#include "../test_utils.c"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

const char *test_fenwick_tree_simple() {
  ftree tree;
  TEST_ASSERT(ftree_init(&tree, test_context, 256 + 1));
  ftree_add(&tree, 15, 1);
  ftree_add(&tree, 37, 1);
  ftree_add(&tree, 64, 1);
  ftree_add(&tree, 21, 1);
  ftree_add(&tree, 256, 1);
  ftree_add(&tree, 15, 1);
  ftree_add(&tree, 0, 1);

  symbol_frequency *source_array = malloc(sizeof(symbol_frequency) * (256 + 1));
  ftree_to_source_array(&tree, source_array);
  ftree tree_from_source;
  TEST_ASSERT(ftree_init(&tree_from_source, test_context, 256 + 1));
  ftree_fill_from_source_frequencies(&tree_from_source, source_array, 0);

  int64 expected_cumulatives[][2] = {{0, 1}, {15, 3}, {21, 4}, {37, 5}, {64, 6}, {256, 7}};
  memsize expected_cumulatives_count = sizeof expected_cumulatives / sizeof expected_cumulatives[0];
  for (memsize i = 0; i < expected_cumulatives_count; i++) {
    int64 *pair = expected_cumulatives[i];
    int64 index = pair[0];
    symbol_frequency expected_sum = (symbol_frequency)pair[1];
    symbol_frequency sum = ftree_sum(&tree, index);
    TEST_ASSERT(sum == expected_sum);
    TEST_ASSERT(ftree_sum(&tree_from_source, index) == expected_sum);
  }

  int64 expected_src[][2] = {{15, 2}, {37, 1}, {64, 1}, {21, 1}, {256, 1}, {0, 1}};
  memsize expected_src_count = sizeof expected_src / sizeof expected_src[0];
  for (memsize i = 0; i < expected_src_count; i++) {
    int64 *pair = expected_src[i];
    int64 index = pair[0];
    symbol_frequency expected_src_value = (symbol_frequency)pair[1];
    symbol_frequency src = source_array[index];
    TEST_ASSERT(src == expected_src_value);
    TEST_ASSERT(ftree_get(&tree, index) == expected_src_value);
    TEST_ASSERT(ftree_get(&tree_from_source, index) == expected_src_value);
  }

  free(source_array);
  ftree_deinit(&tree, test_context);
  ftree_deinit(&tree_from_source, test_context);

  return NULL;
}

const char *test_fenwick_tree_max_range() {
  for (int64 size = 1; size < 1024; size++) {
    ftree tree;
    TEST_ASSERT(ftree_init(&tree, test_context, size));
    TEST_ASSERT(ftree_length(&tree) == size);
    ftree_add(&tree, 0, 1);
    ftree_add(&tree, size - 1, 1);
    uint64 sum = ftree_sum(&tree, size - 1);
    TEST_ASSERT(sum == 2);
    ftree_deinit(&tree, test_context);
  }
  return NULL;
}

const char *test_fenwick_tree_range_sum_cornercase() {
  ftree tree;
  TEST_ASSERT(ftree_init(&tree, test_context, 5));
  ftree_add(&tree, 1, 1);
  TEST_ASSERT(ftree_range_sum(&tree, 2, 1) == 0);
  ftree_deinit(&tree, test_context);
  return NULL;
}

const char *test_fenwick_tree_allocation_failure() {
  setup_test_context(0);
  TEST_ASSERT(context_is_errored(test_context) == false);

  ftree tree;
  TEST_ASSERT(!ftree_init(&tree, test_context, 5));
  TEST_ASSERT(context_is_errored(test_context) == true);
  error *last_error = context_get_error(test_context);
  TEST_ASSERT(strncmp(last_error->message, "Failed to allocate memory: calloc(6, 8)", last_error->message_length) == 0);
  context_clear_error(test_context);
  TEST_ASSERT(context_is_errored(test_context) == false);

  return NULL;
}