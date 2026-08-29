#pragma once
#include "./commons.c"
#include <stddef.h>
#include <stdlib.h>

/** Fenwick tree */
typedef struct {
  /** Length of `data` */
  size_t size;
  symbol_frequency *data;
} ftree;

ftree ftree_new(size_t size) {
  ftree tree;
  // +1 because fenwick trees' indices are innately 1-based
  tree.size = size + 1;
  tree.data = calloc(tree.size, sizeof(symbol_frequency));
  return tree;
}

void ftree_add(ftree tree, size_t index, symbol_frequency delta) {
  index += 1; // 0-based outside -> 1-based internal
  for (size_t i = index; i < tree.size; i += i & -i) {
    tree.data[i] += delta;
  }
}

symbol_frequency ftree_sum(ftree tree, size_t index) {
  index += 1; // 0-based outside -> 1-based internal
  symbol_frequency result = 0;
  for (size_t i = index; i > 0; i -= i & -i) {
    result += tree.data[i];
  }
  return result;
}

ftree ftree_from_values(size_t size, symbol_frequency *values, size_t end_offset) {
  ftree tree = ftree_new(size + end_offset);
  for (size_t i = 0; i < size; i++) {
    ftree_add(tree, i, values[i]);
  }
  return tree;
}

symbol_frequency *ftree_to_source_array(ftree tree) {
  symbol_frequency *result = malloc(sizeof(symbol_frequency) * (tree.size - 1));
  for (size_t i = 0; i < tree.size; i++) {
    result[i] = ftree_sum(tree, i) - ftree_sum(tree, i - 1);
  }
  return result;
}

void ftree_set(ftree tree, size_t index, symbol_frequency new_value) {
  symbol_frequency current = ftree_sum(tree, index) - ftree_sum(tree, index - 1);
  ftree_add(tree, index, new_value - current);
}

symbol_frequency ftree_range_sum(ftree tree, size_t from_index, size_t to_index) {
  if (to_index < from_index) {
    return 0;
  }
  return ftree_sum(tree, to_index) - ftree_sum(from_index - 1);
}

symbol_frequency ftree_get(ftree tree, size_t index) {
  return ftree_range_sum(tree, index, index);
}

size_t ftree_length(ftree tree) {
  return tree.size - 1;
}