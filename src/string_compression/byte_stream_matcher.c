#pragma once
#include "../commons.c"
#include "../context.c"
#include "../data_structures/ring_buffer.c"
#include <assert.h>
#include <limits.h>
#include <stdint.h>

constexpr int64 _BSM_LENGTH_SHIFT = 16; // 16 bits of length = 64kb of lookback. not easily tuneable
constexpr int64 _BSM_LENGTH = 1 << _BSM_LENGTH_SHIFT;
constexpr int64 _BSM_LENGTH_MASK = _BSM_LENGTH - 1;

// tuneable value
constexpr int64 _BSM_MAX_HYPOTHESIS = 20;     // TODO: settings?
constexpr int64 _BSM_MATCH_LENGTH_SHIFT = 12; // consider making it multiple of _STRCOMP_UINT_SYMBOL_BITS_COUNT

constexpr int64 _BSM_MATCH_LENGTH_LIMIT = 1 << _BSM_MATCH_LENGTH_SHIFT;
constexpr int64 _BSM_GOOD_ENOUGH_MATCH_LENGTH = _BSM_MATCH_LENGTH_LIMIT / 2;
constexpr int64 _BSM_HASH_LENGTH_BYTES = 4; // only tune with the algo

/** A structure that stores last N bytes of a stream and helps to search it */
typedef struct {
  // .content is a ring buffer that receives new bytes
  ring_buffer content;

  // hash chains array
  // .hash_head is hash -> position in .content
  // .hash_prev is position in content -> previous position in .content with the same hash
  uint16 *hash_head;
  uint16 *hash_prev;

  uint16 rolling_hash;
} byte_stream_matcher;

typedef struct {
  // TODO: smaller ints here? see comment about positions_by_hash
  int64 offset;
  int64 length;
} bsm_match;

constexpr bsm_match BSM_MATCH_EMPTY = {0};

void bsm_deinit(byte_stream_matcher *bsm, context *context) {
  context_free(context, bsm->hash_head);
  context_free(context, bsm->hash_prev);
  ring_buffer_deinit(&bsm->content, context);

  *bsm = (byte_stream_matcher){0};
}

NODISCARD bool bsm_init(byte_stream_matcher *bsm, context *context) {
  *bsm = (byte_stream_matcher){0};

  bsm->rolling_hash = 0;

  bsm->hash_head = context_allocate_zero_init(context, _BSM_LENGTH, sizeof(uint16));
  if (!bsm->hash_head) {
    bsm_deinit(bsm, context);
    return false;
  }

  bsm->hash_prev = context_allocate_zero_init(context, _BSM_LENGTH, sizeof(uint16));
  if (!bsm->hash_prev) {
    bsm_deinit(bsm, context);
    return false;
  }

  if (!ring_buffer_init(&bsm->content, context, _BSM_LENGTH_SHIFT)) {
    bsm_deinit(bsm, context);
    return false;
  }

  return true;
}

INLINE uint16 _bsm_update_rolling_hash(byte_stream_matcher *bsm, byte new_byte) {
  return bsm->rolling_hash = ((bsm->rolling_hash << 4) ^ new_byte);
}

// inline because I really expect the loop to be unrolled here
INLINE uint16 _bsm_calc_hash_at(ring_buffer *rb, int64 position) {
  uint16 hash = 0;
  for (int64 i = 0; i < _BSM_HASH_LENGTH_BYTES; i++) {
    hash = (hash << 4) ^ ring_buffer_get(rb, position + i);
  }
  return hash;
}

INLINE void _bsm_update_hash_chain(byte_stream_matcher *bsm, uint16 index, byte new_byte) {
  uint16 hash = _bsm_update_rolling_hash(bsm, new_byte);
  uint16 prev_pos = bsm->hash_head[hash];

  // the hash we just calculated is effective for strings starting _BSM_HASH_LENGTH_BYTES bytes back in the stream
  // which means we need to shift the position back
  // TODO: bit mask here?
  int64 target_index_intermediate = (int64)index - (int64)_BSM_HASH_LENGTH_BYTES + 1;
  uint16 target_index = (uint16)(target_index_intermediate < 0 ? target_index_intermediate + (int64)_BSM_LENGTH : target_index_intermediate);

  // printf("recording hash %hu at index %hu\n", hash, target_index);

  bsm->hash_head[hash] = target_index;
  bsm->hash_prev[target_index] = prev_pos;
}

/** Add a byte to the matcher.
This can lead to losing one of the oldest bytes */
INLINE void bsm_push(byte_stream_matcher *bsm, byte b) {
  uint16 position = ring_buffer_get_index(&bsm->content);
  ring_buffer_push(&bsm->content, b);
  _bsm_update_hash_chain(bsm, position, b);
}

int64 _bsm_check_match_starting_at(byte_stream_matcher *bsm, int64 position, ring_buffer *source, int64 source_position) {
  int64 src_end = ring_buffer_get_index(source);
  if (src_end < source_position) {
    src_end += ring_buffer_get_length(source);
  }
  int64 dist_to_src_end = src_end - source_position;

  int64 search_end = ring_buffer_get_index(&bsm->content);
  if (search_end < position) {
    search_end += ring_buffer_get_length(&bsm->content);
  }
  int64 dist_to_search_end = search_end - position;

  // we must not search past source buffer boundary, because bytes there are old and already were written on the previous buffer loop.
  // we must not search past bsm content buffer boundary, because this will generate a match with offset < length,
  // which is bad in two ways - it will break the (offset - length) logic on write, and it will cause search buffer overwrites on read
  // and also we must not read past max match limit, because we won't be able to write match longer than that;
  // but that last limitation is implied by length of source buffer - there's no reason to have source buffer longer than that
  int64 length_limit = dist_to_search_end < dist_to_src_end ? dist_to_search_end : dist_to_src_end;

  // printf("start: %zi; end %zi\n", source_position, src_end);
  for (int64 length = 0; length < length_limit; length++) {
    int64 src_current_position = source_position + length;
    byte buffer_byte = ring_buffer_get(&bsm->content, position + length);
    byte src_byte = ring_buffer_get(source, src_current_position);
    // printf("checking positions: src_pos=%zu match_pos=%zu: %i =?= %i\n", src_current_position, position + 1, src_byte, buffer_byte);
    if (buffer_byte != src_byte) {
      return length;
    }
  }
  return length_limit;
}

bsm_match bsm_find_match(byte_stream_matcher *bsm, ring_buffer *source, int64 source_position) {
  // TODO: think about detecting invalid positions, or clearing them on push
  // as it is right now, this function just assumes there will be no hash misses/invalid pointers
  // but there totally will be misses, and maybe it's cheaper to handle them rather than to have generic algo
  uint16 hash = _bsm_calc_hash_at(source, source_position);
  // printf("searching for hash %hu\n", hash);
  int64 buffer_end_position = ring_buffer_get_index(&bsm->content);
  int64 position = bsm->hash_head[hash];
  bsm_match result = {0};
  // printf("starting match search\n");
  for (int64 i = 0; i < _BSM_MAX_HYPOTHESIS; i++) {
    // printf("searching for hash %hu at %zu (attempt %zu)\n", hash, position, i);
    int64 length = _bsm_check_match_starting_at(bsm, position, source, source_position);
    int64 match_position = position;
    position = bsm->hash_prev[position];

    // it's tempting to test if `length < hash bytes length` here, and early-exit
    // but this means that sometimes, when hash collision occurs, a match is lost
    // as matches earlier in the stream may have the same hash as unrelated text later in the stream
    // and this will form chain of hashes in which first and last hashes point to a sequence with the start we are searching for,
    // but hash in the middle would point to unrelated sequence

    if (length > result.length) {
      // printf("len: %zi\n", length);
      // printf("abs pos: %zu\n", position);
      result.length = length;
      // TODO: bit mask here?
      int64 offset = (int64)buffer_end_position - (int64)match_position;
      if (offset < 0) {
        offset += (int64)_BSM_LENGTH;
      }
      result.offset = offset;

      if (result.length >= _BSM_GOOD_ENOUGH_MATCH_LENGTH) {
        // this condition is here mostly for synthetic data
        // on synthetic data, it's possible that we will have ridiculously long matches every time
        // and to break this pattern, we would accept first "long enough" match
        // (doesn't harm compression ratio on actual data btw)
        break;
      }
    }
  }
  return result;
}
