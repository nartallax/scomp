#pragma once
#include "commons.c"
#include "context.c"
#include <assert.h>
#include <limits.h>
#include <stdint.h>

const size_t BSM_LENGTH = 0x10000;
const size_t BSM_LENGTH_MASK = BSM_LENGTH - 1;

/** A structure that stores last N bytes of a stream and helps to search it */
typedef struct {
  context *context;

  // .content is a ring buffer that receives new bytes
  byte *content;
  uint16_t content_position;

  // hash chains array
  // .hash_head is hash -> position in .content
  // .hash_prev is position in content -> previous position in .content with the same hash
  uint16_t *hash_head;
  uint16_t *hash_prev;

  uint16_t rolling_hash;
} byte_stream_matcher;

typedef struct {
  // TODO: smaller ints here? see comment about positions_by_hash
  uint64_t offset;
  uint64_t length;
} bsm_match;

bool bsm_init(byte_stream_matcher *bsm, context *context) {
  bsm->context = context;
  bsm->content_position = 0;
  bsm->rolling_hash = 0;

  bsm->content = context_allocate_zero_init(context, BSM_LENGTH, sizeof(byte));
  if (!bsm->content) {
    return false;
  }

  bsm->hash_head = context_allocate_zero_init(context, BSM_LENGTH, sizeof(uint16_t));
  if (!bsm->hash_head) {
    context_free(context, bsm->content);
    return false;
  }

  bsm->hash_prev = context_allocate_zero_init(context, BSM_LENGTH, sizeof(uint16_t));
  if (!bsm->hash_prev) {
    context_free(context, bsm->content);
    context_free(context, bsm->hash_head);
    return false;
  }

  return true;
}

void bsm_deinit(byte_stream_matcher *bsm) {
  context_free(bsm->context, bsm->content);
  context_free(bsm->context, bsm->hash_head);
  context_free(bsm->context, bsm->hash_prev);
}

// TODO: experiment with different hash functions here
// for example, some implementations of DEFLATE use only first 3 bytes, which may make hash more useful. or not.
// in either case, if we go with hash over last 4 bytes - with current implementation, the first byte only appears as 4 bits in it
// which may cause more collisions. or not.
void _bsm_update_hash(byte_stream_matcher *bsm, byte new_byte) {
  bsm->rolling_hash = ((uint64_t)bsm->rolling_hash << 4) ^ new_byte;
}

// size_t _bsm_calculate_hash_at(byte_stream_matcher *bsm, size_t index) {
//   uint64_t hash = 0;
//   for (int i = 1; i <= BSM_HASH_SOURCE_LENGTH; i++) {
//     // TODO: try different constants here
//     size_t shift = (i * 7) & _bsm_hash_bit_length;
//     hash ^= _bsm_get_hash_part(bsm->content[index], shift);
//     index = (index + 1) & bsm->length_mask;
//   }

//   // cut it down to length
//   // overflow bytes go to the start
//   size_t result = hash & bsm->length_mask;
//   result |= (hash ^ result) >> bsm->length_shift;

//   return hash;
// }

// // TODO: rolling hash, hash chains, actually limit length
// void _bsm_rebuild_hash_at(byte_stream_matcher *bsm, size_t index) {
//   size_t start_index = index < BSM_HASH_SOURCE_LENGTH ? (index + bsm->length) - BSM_HASH_SOURCE_LENGTH : index - BSM_HASH_SOURCE_LENGTH;
//   size_t hash = _bsm_calculate_hash_at(bsm, start_index);
//   bsm->hash_head[hash] = index;
// }

// /** Add a byte to the matcher.
// This can lead to losing one of the oldest bytes */
// void bsm_push(byte_stream_matcher *bsm, byte b) {
//   size_t index = bsm->content_position++;
//   bsm->content[index] = b;
//   _bsm_rebuild_hash_at(bsm, index);
// }

// bsm_match bsm_find_match(byte_stream_matcher *bsm, byte b, const byte *source, size_t source_length) {
//   // TODO: impl
//   return (bsm_match){0};
// }