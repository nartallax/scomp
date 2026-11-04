#pragma once
#include "../commons.c"
#include "../context.c"
#include <limits.h>
#include <stdint.h>

constexpr int64 HASH_CHAIN_BIT_WIDTH = sizeof(uint16) * CHAR_BIT; // hash is uint16
constexpr int64 HASH_CHAIN_HASH_ARRAY_LENGTH = 1 << HASH_CHAIN_BIT_WIDTH;

/** A hash-map of hash->int that allows to search previous matches for that hash */
typedef struct {
  int64 length;
  // hash -> position in
  // TODO: reduce both heads and prevs to uint16, we probably don't need that many bits here
  int64 *heads;
  // position -> previous position with the same hash
  int64 *prevs;
} hash_chain;

void hash_chain_deinit(hash_chain *chain) {
  *chain = (hash_chain){0};
}

void hash_chain_reset(hash_chain *chain) {
  for (int64 i = 0; i < HASH_CHAIN_HASH_ARRAY_LENGTH; i++) {
    chain->heads[i] = 0;
  }

  for (int64 i = 0; i < chain->length; i++) {
    chain->prevs[i] = 0;
  }
}

void hash_chain_init(hash_chain *chain, context *context, int64 length) {
  *chain = (hash_chain){0};

  chain->length = length;
  chain->heads = context_arena_allocate_zero_init(context, HASH_CHAIN_HASH_ARRAY_LENGTH, sizeof(int64));
  chain->prevs = context_arena_allocate_zero_init(context, length, sizeof(int64));
}

INLINE void hash_chain_push(hash_chain *chain, uint16 hash, int64 position) {
  int64 prev_position = chain->heads[hash];
  chain->heads[hash] = position;
  chain->prevs[position] = prev_position;
}

INLINE int64 hash_chain_get_first_position_by_hash(hash_chain *chain, uint16 hash) {
  return chain->heads[hash];
}

INLINE int64 hash_chain_get_previous_position_by_position(hash_chain *chain, int64 position) {
  return chain->prevs[position];
}