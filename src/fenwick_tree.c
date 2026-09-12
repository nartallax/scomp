#pragma once
#include "./context.c"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Fenwick tree */
typedef struct {
  /** Length of `data`.
  Here (and in other places in this file), size and indices are not size_t, but int64_t instead.
  This is because Fenwick trees by design do weird stuff with indices, and they can go below 0, as temporary value. */
  int64_t size;
  uint64_t *data;
} ftree;

bool ftree_init(ftree *tree, context *context, int64_t size) {
  // +1 because fenwick trees' indices are inherently 1-based
  tree->size = size + 1;
  tree->data = context_allocate_zero_init(context, tree->size, sizeof(uint64_t));
  return tree->data != NULL;
}

void ftree_deinit(ftree tree, context *context) {
  context_free(context, tree.data);
}

// TODO: passing tree by value everywhere goes against other places in this repo
// after we have proper perf test, start passing it by reference and measure if it affects perf
// if it doesn't - include context in the tree, to avoid passing it in deinit
void ftree_add(ftree tree, int64_t symbol, uint64_t delta) {
  assert(symbol + 1 < tree.size);

  symbol += 1; // 0-based outside -> 1-based internal
  for (int64_t i = symbol; i < tree.size; i += i & -i) {
    tree.data[i] += delta;
  }
}

/** Get cumulative frequency of a symbol (including frequencies of symbols before this one) */
uint64_t ftree_sum(ftree tree, int64_t symbol) {
  assert(symbol + 1 < tree.size);

  symbol += 1; // 0-based outside -> 1-based internal
  uint64_t result = 0;
  for (int64_t i = symbol; i > 0; i -= i & -i) {
    result += tree.data[i];
  }
  return result;
}

/** Sets frequencies of the symbols in the tree from source values. Existing values are overwritten. */
void ftree_fill_from_source_frequencies(ftree tree, uint64_t *values, int64_t end_offset) {
  for (int64_t i = 0; i < tree.size; i++) {
    tree.data[i] = 0;
  }

  for (int64_t i = 0; i < tree.size - 1 - end_offset; i++) {
    ftree_add(tree, i, values[i]);
  }
}

/** Expects an array of `ftree_length(tree)` and fills it with source non-cumulative frequencies of symbols.
Reverse operation for this one is `ftree_fill_from_source_frequencies()` */
void ftree_to_source_array(ftree tree, uint64_t *buffer) {
  for (int64_t i = 0; i < tree.size - 1; i++) {
    buffer[i] = ftree_sum(tree, i) - ftree_sum(tree, i - 1);
  }
}

uint64_t ftree_range_sum(ftree tree, int64_t from_index, int64_t to_index) {
  assert(from_index + 1 < tree.size && to_index + 1 < tree.size);

  if (to_index < from_index) {
    return 0;
  }
  return ftree_sum(tree, to_index) - ftree_sum(tree, from_index - 1);
}

/** Get frequency of a symbol (only of this symbol) */
uint64_t ftree_get(ftree tree, int64_t symbol) {
  assert(symbol + 1 < tree.size);

  return ftree_range_sum(tree, symbol, symbol);
}

int64_t ftree_length(ftree tree) {
  return tree.size - 1;
}