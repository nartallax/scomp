#pragma once
#include "../src/fenwick_tree.c"
#include "./test_utils.c"
#include <stdio.h>

const char *test_fenwick_tree_simple() {
  ftree tree = ftree_new(256 + 1);
  ftree_add(tree, 15, 1);
  ftree_add(tree, 37, 1);
  ftree_add(tree, 64, 1);
  ftree_add(tree, 21, 1);
  ftree_add(tree, 256, 1);
  ftree_add(tree, 15, 1);
  ftree_add(tree, 0, 1);

  symbol_frequency *source_array = ftree_to_source_array(tree);
  ftree tree_from_source = ftree_from_values(256 + 1, source_array, 0);

  int expected_cumulatives[][2] = {{0, 1}, {15, 3}, {21, 4}, {37, 5}, {64, 6}, {256, 7}};
  size_t expected_cumulatives_count = sizeof expected_cumulatives / sizeof expected_cumulatives[0];
  for (size_t i = 0; i < expected_cumulatives_count; i++) {
    int *pair = expected_cumulatives[i];
    size_t index = (size_t)pair[0];
    symbol_frequency expected_sum = (symbol_frequency)pair[1];
    symbol_frequency sum = ftree_sum(tree, index);
    TEST_ASSERT(sum == expected_sum, "Cumulative sum should be equal to expected");
    TEST_ASSERT(ftree_sum(tree_from_source, index) == expected_sum, "Cumulative sum from reparsed tree should be equal to expected");
  }

  int expected_src[][2] = {{15, 2}, {37, 1}, {64, 1}, {21, 1}, {256, 1}, {0, 1}};
  size_t expected_src_count = sizeof expected_src / sizeof expected_src[0];
  for (size_t i = 0; i < expected_src_count; i++) {
    int *pair = expected_src[i];
    size_t index = (size_t)pair[0];
    symbol_frequency expected_src_value = (symbol_frequency)pair[1];
    symbol_frequency src = source_array[index];
    TEST_ASSERT(src == expected_src_value, "Source value should be equal to expected");
    TEST_ASSERT(ftree_get(tree, index) == expected_src_value, "ftree_get should be equal to expected");
    TEST_ASSERT(ftree_get(tree_from_source, index) == expected_src_value, "ftree_get from reparsed tree should be equal to expected");
  }

  free(source_array);
  ftree_delete(tree);
  ftree_delete(tree_from_source);

  return NULL;
}

const char *test_fenwick_tree_max_range() {
  for (size_t size = 1; size < 1024; size++) {
    ftree tree = ftree_new(size);
    TEST_ASSERT(ftree_length(tree) == size, "ftree_length must be equal to passed size");
    ftree_add(tree, 0, 1);
    ftree_add(tree, size - 1, 1);
    symbol_frequency sum = ftree_sum(tree, size - 1);
    TEST_ASSERT(sum == 2, "Sum of just two increments must be 2");
    ftree_delete(tree);
  }
  return NULL;
}

const char *test_fenwick_tree_range_sum_cornercase() {
  ftree tree = ftree_new(5);
  ftree_add(tree, 1, 1);
  TEST_ASSERT(ftree_range_sum(tree, 2, 1) == 0, "Range sums with negative range lengths must be zero");
  ftree_delete(tree);
  return NULL;
}