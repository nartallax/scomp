#pragma once
#include "../commons.c"
#include "../context.c"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Fenwick tree */
typedef struct {
  /** Length of `data` */
  int64_t size;
  symbol_frequency *data;
} ftree;

NODISCARD bool ftree_init(ftree *tree, context *context, int64_t size) {
  *tree = (ftree){0};

  // +1 because fenwick trees' indices are inherently 1-based
  tree->size = size + 1;
  tree->data = context_allocate_zero_init(context, tree->size, sizeof(symbol_frequency));
  return tree->data != NULL;
}

void ftree_deinit(ftree *tree, context *context) {
  context_free(context, tree->data);

  *tree = (ftree){0};
}

// TODO: passing tree by value everywhere goes against other places in this repo
// after we have proper perf test, start passing it by reference and measure if it affects perf
// if it doesn't - pass by reference, to be in line with everything else
void ftree_add(ftree *tree, int64_t symbol, symbol_frequency delta) {
  assert(symbol + 1 < tree->size);

  symbol += 1; // 0-based outside -> 1-based internal
  for (int64_t i = symbol; i < tree->size; i += i & -i) {
    tree->data[i] += delta;
  }
}

/** Get cumulative frequency of a symbol (including frequencies of symbols before this one) */
symbol_frequency ftree_sum(ftree *tree, int64_t symbol) {
  assert(symbol + 1 < tree->size);

  symbol += 1; // 0-based outside -> 1-based internal
  symbol_frequency result = 0;
  for (int64_t i = symbol; i > 0; i -= i & -i) {
    result += tree->data[i];
  }
  return result;
}

/** Sets frequencies of the symbols in the tree from source values. Existing values are overwritten. */
void ftree_fill_from_source_frequencies(ftree *tree, symbol_frequency *values, int64_t end_offset) {
  for (int64_t i = 0; i < tree->size; i++) {
    tree->data[i] = 0;
  }

  for (int64_t i = 0; i < tree->size - 1 - end_offset; i++) {
    ftree_add(tree, i, values[i]);
  }
}

/** Expects an array of `ftree_length(tree)` and fills it with source non-cumulative frequencies of symbols.
Reverse operation for this one is `ftree_fill_from_source_frequencies()` */
void ftree_to_source_array(ftree *tree, symbol_frequency *buffer) {
  for (int64_t i = 0; i < tree->size - 1; i++) {
    buffer[i] = ftree_sum(tree, i) - ftree_sum(tree, i - 1);
  }
}

symbol_frequency ftree_range_sum(ftree *tree, int64_t from_index, int64_t to_index) {
  assert(from_index + 1 < tree->size && to_index + 1 < tree->size);

  if (to_index < from_index) {
    return 0;
  }
  return ftree_sum(tree, to_index) - ftree_sum(tree, from_index - 1);
}

/** Get frequency of a symbol (only of this symbol) */
symbol_frequency ftree_get(ftree *tree, int64_t symbol) {
  assert(symbol + 1 < tree->size);

  return ftree_range_sum(tree, symbol, symbol);
}

int64_t ftree_length(ftree *tree) {
  return tree->size - 1;
}