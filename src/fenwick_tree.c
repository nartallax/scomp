#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/** Fenwick tree */
typedef struct {
  /** Length of `data` */
  size_t size;
  uint64_t *data;
} ftree;

ftree ftree_new(size_t size) {
  ftree tree;
  // +1 because fenwick trees' indices are innately 1-based
  tree.size = size + 1;
  tree.data = calloc(tree.size, sizeof(uint64_t));
  return tree;
}

void ftree_delete(ftree tree) {
  free(tree.data);
}

void ftree_add(ftree tree, size_t symbol, uint64_t delta) {
  symbol += 1; // 0-based outside -> 1-based internal
  for (size_t i = symbol; i < tree.size; i += i & -i) {
    tree.data[i] += delta;
  }
}

/** Get cumulative frequency of a symbol (including frequencies of symbols before this one) */
uint64_t ftree_sum(ftree tree, size_t symbol) {
  symbol += 1; // 0-based outside -> 1-based internal
  uint64_t result = 0;
  for (size_t i = symbol; i > 0; i -= i & -i) {
    result += tree.data[i];
  }
  return result;
}

ftree ftree_from_values(size_t size, uint64_t *values, size_t end_offset) {
  ftree tree = ftree_new(size + end_offset);
  for (size_t i = 0; i < size; i++) {
    ftree_add(tree, i, values[i]);
  }
  return tree;
}

/** Returns an array of length `ftree_length(tree)` which holds source non-cumulative frequencies of symbols.
Reverse operation for this one is `ftree_from_values()` */
uint64_t *ftree_to_source_array(ftree tree) {
  uint64_t *result = malloc(sizeof(uint64_t) * (tree.size - 1));
  for (size_t i = 0; i < tree.size - 1; i++) {
    result[i] = ftree_sum(tree, i) - ftree_sum(tree, i - 1);
  }
  return result;
}

uint64_t ftree_range_sum(ftree tree, size_t from_index, size_t to_index) {
  if (to_index < from_index) {
    return 0;
  }
  return ftree_sum(tree, to_index) - ftree_sum(tree, from_index - 1);
}

/** Get frequency of a symbol (only of this symbol) */
uint64_t ftree_get(ftree tree, size_t symbol) {
  return ftree_range_sum(tree, symbol, symbol);
}

size_t ftree_length(ftree tree) {
  return tree.size - 1;
}