#pragma once
#include "../src/fenwick_tree.c"
#include "./test_utils.c"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

const char *test_fenwick_tree_simple() {
  ftree tree;
  ftree_init(&tree, test_context, 256 + 1);
  ftree_add(tree, 15, 1);
  ftree_add(tree, 37, 1);
  ftree_add(tree, 64, 1);
  ftree_add(tree, 21, 1);
  ftree_add(tree, 256, 1);
  ftree_add(tree, 15, 1);
  ftree_add(tree, 0, 1);

  uint64_t *source_array = malloc(sizeof(uint64_t) * (256 + 1));
  ftree_to_source_array(tree, source_array);
  ftree tree_from_source;
  ftree_init(&tree_from_source, test_context, 256 + 1);
  ftree_fill_from_source_frequencies(tree_from_source, source_array, 0);

  int expected_cumulatives[][2] = {{0, 1}, {15, 3}, {21, 4}, {37, 5}, {64, 6}, {256, 7}};
  size_t expected_cumulatives_count = sizeof expected_cumulatives / sizeof expected_cumulatives[0];
  for (size_t i = 0; i < expected_cumulatives_count; i++) {
    int *pair = expected_cumulatives[i];
    size_t index = (size_t)pair[0];
    uint64_t expected_sum = (uint64_t)pair[1];
    uint64_t sum = ftree_sum(tree, index);
    TEST_ASSERT(sum == expected_sum, "Cumulative sum should be equal to expected");
    TEST_ASSERT(ftree_sum(tree_from_source, index) == expected_sum, "Cumulative sum from reparsed tree should be equal to expected");
  }

  int expected_src[][2] = {{15, 2}, {37, 1}, {64, 1}, {21, 1}, {256, 1}, {0, 1}};
  size_t expected_src_count = sizeof expected_src / sizeof expected_src[0];
  for (size_t i = 0; i < expected_src_count; i++) {
    int *pair = expected_src[i];
    size_t index = (size_t)pair[0];
    uint64_t expected_src_value = (uint64_t)pair[1];
    uint64_t src = source_array[index];
    TEST_ASSERT(src == expected_src_value, "Source value should be equal to expected");
    TEST_ASSERT(ftree_get(tree, index) == expected_src_value, "ftree_get should be equal to expected");
    TEST_ASSERT(ftree_get(tree_from_source, index) == expected_src_value, "ftree_get from reparsed tree should be equal to expected");
  }

  free(source_array);
  ftree_deinit(tree, test_context);
  ftree_deinit(tree_from_source, test_context);

  return NULL;
}

const char *test_fenwick_tree_max_range() {
  for (int64_t size = 1; size < 1024; size++) {
    ftree tree;
    ftree_init(&tree, test_context, size);
    TEST_ASSERT(ftree_length(tree) == size, "ftree_length must be equal to passed size");
    ftree_add(tree, 0, 1);
    ftree_add(tree, size - 1, 1);
    uint64_t sum = ftree_sum(tree, size - 1);
    TEST_ASSERT(sum == 2, "Sum of just two increments must be 2");
    ftree_deinit(tree, test_context);
  }
  return NULL;
}

const char *test_fenwick_tree_range_sum_cornercase() {
  ftree tree;
  ftree_init(&tree, test_context, 5);
  ftree_add(tree, 1, 1);
  TEST_ASSERT(ftree_range_sum(tree, 2, 1) == 0, "Range sums with negative range lengths must be zero");
  ftree_deinit(tree, test_context);
  return NULL;
}

const char *test_fenwick_tree_allocation_failure() {
  setup_test_context(0);
  TEST_ASSERT(context_is_errored(test_context) == false, "Error must not be set at the test start");

  ftree tree;
  TEST_ASSERT(!ftree_init(&tree, test_context, 5), "Init must return false on init failure");
  TEST_ASSERT(context_is_errored(test_context) == true, "Error must be present when allocation fails");
  error *last_error = context_get_error(test_context);
  TEST_ASSERT(strncmp(last_error->message, "Failed to allocate memory: calloc(6, 8)", last_error->message_length) == 0, "Error message must be the one we expect");
  context_clear_error(test_context);
  TEST_ASSERT(context_is_errored(test_context) == false, "Error must not be set after it was cleared");

  return NULL;
}