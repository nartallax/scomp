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

void ftree_delete(ftree tree) {
  free(tree.data);
}

void ftree_add(ftree tree, symbol symbol, symbol_frequency delta) {
  symbol += 1; // 0-based outside -> 1-based internal
  for (size_t i = symbol; i < tree.size; i += i & -i) {
    tree.data[i] += delta;
  }
}

/** Get cumulative frequency of a symbol (including frequencies of symbols before this one) */
symbol_frequency ftree_sum(ftree tree, symbol symbol) {
  symbol += 1; // 0-based outside -> 1-based internal
  symbol_frequency result = 0;
  for (size_t i = symbol; i > 0; i -= i & -i) {
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
  for (size_t i = 0; i < tree.size - 1; i++) {
    result[i] = ftree_sum(tree, i) - ftree_sum(tree, i - 1);
  }
  return result;
}

symbol_frequency ftree_range_sum(ftree tree, size_t from_index, size_t to_index) {
  if (to_index < from_index) {
    return 0;
  }
  return ftree_sum(tree, to_index) - ftree_sum(tree, from_index - 1);
}

/** Get frequency of a symbol (only of this symbol) */
symbol_frequency ftree_get(ftree tree, symbol symbol) {
  return ftree_range_sum(tree, symbol, symbol);
}

size_t ftree_length(ftree tree) {
  return tree.size - 1;
}