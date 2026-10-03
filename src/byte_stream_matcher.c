#pragma once
#include "commons.c"
#include "context.c"
#include "ring_buffer.c"
#include <assert.h>
#include <limits.h>
#include <stdint.h>

const size_t _BSM_LENGTH_SHIFT = 16; // 16 bits of length = 64kb of lookback
const size_t _BSM_LENGTH = 1 << _BSM_LENGTH_SHIFT;
const size_t _BSM_LENGTH_MASK = _BSM_LENGTH - 1;
const size_t _BSM_LOOKBACK_LENGTH = 3;
const size_t _BSM_MATCH_LENGTH_SHIFT = 9; // not based on anything
const size_t _BSM_MATCH_LENGTH_LIMIT = 1 << _BSM_MATCH_LENGTH_SHIFT;
const size_t _BSM_HASH_LENGTH_BYTES = 4;

/** A structure that stores last N bytes of a stream and helps to search it */
typedef struct {
  context *context;

  // .content is a ring buffer that receives new bytes
  ring_buffer content;

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

const bsm_match empty_bsm_match = {0};

NODISCARD bool bsm_init(byte_stream_matcher *bsm, context *context) {
  *bsm = (byte_stream_matcher){0};

  bsm->context = context;
  bsm->rolling_hash = 0;

  bsm->hash_head = context_allocate_zero_init(context, _BSM_LENGTH, sizeof(uint16_t));
  if (!bsm->hash_head) {
    return false;
  }

  bsm->hash_prev = context_allocate_zero_init(context, _BSM_LENGTH, sizeof(uint16_t));
  if (!bsm->hash_prev) {
    context_free(context, bsm->hash_head);
    return false;
  }

  if (!ring_buffer_init(&bsm->content, context, _BSM_LENGTH_SHIFT)) {
    context_free(context, bsm->hash_head);
    context_free(context, bsm->hash_prev);
    return false;
  }

  return true;
}

void bsm_deinit(byte_stream_matcher *bsm) {
  context_free(bsm->context, bsm->hash_head);
  context_free(bsm->context, bsm->hash_prev);
  ring_buffer_deinit(&bsm->content);
}

// TODO: experiment with different hash functions here
// for example, some implementations of DEFLATE use only first 3 bytes, which may make hash more useful. or not.
// in either case, if we go with hash over last 4 bytes - with current implementation, the first byte only appears as 4 bits in it
// which may cause more collisions. or not.
uint16_t _bsm_update_rolling_hash(byte_stream_matcher *bsm, byte new_byte) {
  return bsm->rolling_hash = (uint16_t)(((uint64_t)bsm->rolling_hash << 4) ^ new_byte);
}

uint16_t _bsm_calc_hash_at(ring_buffer *rb, size_t position) {
  // TODO: try manually unrolling the loop?
  // return (uint16_t)((uint64_t)ring_buffer_get(rb, position) << 12) ^ ((uint64_t)ring_buffer_get(rb, position + 1) << 8) ^ ((uint64_t)ring_buffer_get(rb, position + 2) << 4) ^
  //        ((uint64_t)ring_buffer_get(rb, position + 3) << 0);
  uint64_t hash = 0;
  for (size_t i = 0; i < _BSM_HASH_LENGTH_BYTES; i++) {
    hash = (hash << 4) ^ ring_buffer_get(rb, position + i);
  }
  return (uint16_t)hash;
}

void _bsm_update_hash_chain(byte_stream_matcher *bsm, uint16_t index, byte new_byte) {
  uint16_t hash = _bsm_update_rolling_hash(bsm, new_byte);
  uint16_t prev_pos = bsm->hash_prev[hash];

  // the hash we just calculated is effective for strings starting _BSM_HASH_LENGTH_BYTES bytes back in the stream
  // which means we need to shift the position back
  // TODO: consider converting constants to int64_t too?
  int64_t target_index_intermediate = (int64_t)index - (int64_t)_BSM_HASH_LENGTH_BYTES + 1;
  uint16_t target_index = (uint16_t)(target_index_intermediate < 0 ? target_index_intermediate + (int64_t)_BSM_LENGTH : target_index_intermediate);

  // printf("recording hash %hu at index %hu\n", hash, target_index);

  bsm->hash_head[hash] = target_index;
  bsm->hash_prev[target_index] = prev_pos;
}

/** Add a byte to the matcher.
This can lead to losing one of the oldest bytes */
void bsm_push(byte_stream_matcher *bsm, byte b) {
  uint16_t position = ring_buffer_get_index(&bsm->content);
  ring_buffer_push(&bsm->content, b);
  _bsm_update_hash_chain(bsm, position, b);
}

size_t _bsm_check_match_starting_at(byte_stream_matcher *bsm, size_t position, ring_buffer *source, size_t source_position) {
  size_t src_end = ring_buffer_get_index(source);
  for (size_t i = 0; i < _BSM_MATCH_LENGTH_LIMIT; i++) {
    size_t src_current_position = source_position + i;
    if (src_current_position == src_end) {
      // don't read past source buffer end
      // but it's fine to read past match buffer end, because it's zero-initialized and synchronized on decoder and encoder
      // which means even if we accidently read past the end marker - as long as the data at the end matches the source - it's fine
      return i;
    }
    byte buffer_byte = ring_buffer_get(&bsm->content, position + i);
    byte src_byte = ring_buffer_get(source, src_current_position);
    // printf("checking positions: src_pos=%zu match_pos=%zu: %i =?= %i\n", src_current_position, position + 1, src_byte, buffer_byte);
    if (buffer_byte != src_byte) {
      return i;
    }
  }
  return _BSM_MATCH_LENGTH_LIMIT;
}

// TODO: I wonder if size_t here instead of uint16_t is bad for performance
bsm_match bsm_find_match(byte_stream_matcher *bsm, ring_buffer *source, size_t source_position) {
  // TODO: think about detecting invalid positions, or clearing them on push
  // as it is right now, this function just assumes there will be no hash misses/invalid pointers
  // but there totally will be misses, and maybe it's cheaper to handle them rather than to have generic algo
  uint16_t hash = _bsm_calc_hash_at(source, source_position);
  // printf("searching for hash %hu\n", hash);
  size_t buffer_end_position = ring_buffer_get_index(&bsm->content);
  size_t position = bsm->hash_head[hash];
  bsm_match result = {0};
  for (size_t i = 0; i < _BSM_LOOKBACK_LENGTH; i++) {
    // printf("searching for hash %hu at %zu\n", hash, position);
    size_t length = _bsm_check_match_starting_at(bsm, position, source, source_position);
    if (length > result.length) {
      // printf("abs pos: %zu\n", position);
      result.length = length;
      int64_t offset = (int64_t)buffer_end_position - (int64_t)position;
      if (offset < 0) {
        offset += (int64_t)_BSM_LENGTH;
      }
      result.offset = offset;
    }
    position = bsm->hash_prev[position];
  }
  return result;
}
