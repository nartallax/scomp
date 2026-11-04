#pragma once
#include "../../src/data_structures/hash_chain.c"
#include "../test_utils.c"

const char *test_hash_chain_simple() {
  hash_chain chain;
  hash_chain_init(&chain, test_context, 1024);

  hash_chain_push(&chain, 123, 123);
  hash_chain_push(&chain, 123, 321);
  hash_chain_push(&chain, 333, 444);
  TEST_ASSERT(hash_chain_get_first_position_by_hash(&chain, 123) == 321);
  TEST_ASSERT(hash_chain_get_previous_position_by_position(&chain, 321) == 123);
  TEST_ASSERT(hash_chain_get_previous_position_by_position(&chain, 123) == 0);
  TEST_ASSERT(hash_chain_get_previous_position_by_position(&chain, 0) == 0);
  TEST_ASSERT(hash_chain_get_first_position_by_hash(&chain, 333) == 444);
  TEST_ASSERT(hash_chain_get_previous_position_by_position(&chain, 444) == 0);

  hash_chain_reset(&chain);
  TEST_ASSERT(hash_chain_get_first_position_by_hash(&chain, 123) == 0);
  TEST_ASSERT(hash_chain_get_first_position_by_hash(&chain, 333) == 0);

  hash_chain_deinit(&chain);
  return NULL;
}