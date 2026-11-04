#pragma once
#include "../commons.c"
#include "../context.c"
#include <stdint.h>

// 16 bits of length = 64kb of addressable lookback in string compression. not easily tuneable
constexpr int64 HASH_CHAIN_SHIFT = 16;
constexpr int64 HASH_CHAIN_LENGTH = 1 << HASH_CHAIN_SHIFT;

typedef struct {
  // hash -> position in
  uint16 *heads;
  // position -> previous position with the same hash
  uint16 *prevs;
} hash_chain;

void hash_chain_deinit(hash_chain *chain) {
  *chain = (hash_chain){0};
}

void hash_chain_reset(hash_chain *chain) {
  for (int64 i = 0; i < HASH_CHAIN_LENGTH; i++) {
    chain->heads[i] = 0;
    chain->prevs[i] = 0;
  }
}

void hash_chain_init(hash_chain *chain, context *context) {
  *chain = (hash_chain){0};

  chain->heads = context_arena_allocate_zero_init(context, HASH_CHAIN_LENGTH, sizeof(uint16));
  chain->prevs = context_arena_allocate_zero_init(context, HASH_CHAIN_LENGTH, sizeof(uint16));
}

INLINE void hash_chain_push(hash_chain *chain, uint16 hash, uint16 position) {
  uint16 prev_position = chain->heads[hash];
  chain->heads[hash] = position;
  chain->prevs[position] = prev_position;
}

INLINE uint16 hash_chain_get_first_position_by_hash(hash_chain *chain, uint16 hash) {
  return chain->heads[hash];
}

INLINE uint16 hash_chain_get_previous_position_by_position(hash_chain *chain, uint16 position) {
  return chain->prevs[position];
}