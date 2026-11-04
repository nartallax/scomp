#pragma once
#include "../commons.c"
#include "../context.c"
#include "../data_structures/fixed_pool.c"
#include "../data_structures/fixed_stack.c"
#include "../data_structures/hash_chain.c"
#include "../data_structures/lm_queue.c"
#include "json_base.c"
#include <stdint.h>
#include <stdlib.h>

/*
have a token ring buffer that stores tokens before they are written out.
only start writing once buffer is full (like with strcomp).
as new tokens are encountered, maintain shapes repo - push open/close tokens, and object_key parts, and object_key starts.
add meta-info to object_open tokens (modify token structure) that holds some sort of reference to the shape of this object in the repo.

keep in mind that shapes can be incomplete, or invalidated, even when it's time to write the object; in this case, just write the tokens.
opening token holds reference to the shape it started, and its ID. shape holds reference to that reference. this way it can be updated/invalidated from both sides.
ID of the shape is only incremented every time an object is discovered. if it ever overflows - the cache resets, and the stack of the object starters resets too.
shapes can be invalidated for two reasons - because of the length overflow during building, and because of it being pushed out of cache after it's completed.
when repo invalidates the shape (because of its length) - repo sets reference to null, so the source object knows it's invalidated.
when repo invalidates the shape (because of the cache) - we clear the shape from the pool, and set its id to MAX_ID. if it's ever reused - its id will be larger than ids the opening tokens.
therefore, if an object is being written out, and starting token has shape ID < shape ID stored in the shape it references - then the shape was removed because of cache, and must not be used.
when object invalidates the shape (because we started writing it before it's competed) - object sets reference on the shape to null.
when repo competes the shape and merges it - repo updates the reference to reference the existing shape.
if shape is completed, but the merge didn't happen because no matching shape was found - repo must set reference to null;
this prevents referencing the shape, as it's the first of its kind and we must write it out instead of referencing.

this will also require maintaining a stack of opening tokens, to take from when a closing token is encountered.

repo: have a data structure that stores N values and allows to arbitrarily add/remove them, and access them by index (or do it with pointers?)
when starting a shape - put one in the repo; when ending a shape - "promote" it to completed.
completing a shape means putting it into a hash chain. if there is exactly matching shape - don't put new one in; instead, replace the shape completed object references with the old one.
on replace - make sure to update completed object's opening token so it points to the right shape.

when you need to put a new shape into the repo, but it's full:
find least recently used completed shape, and remove it. don't remove incomplete shapes.
this requires maintaining LRU queue, somehow.

shape references are written as offsets in the shapes repo.


repo interface:
object_shape* open_object() // returns shape reference to store; both reference and ID of that shape must be saved at that moment
void close_object()
void push_object_key_char(unicode_char_token)
void object_key_start()
object_shape get_shape_by_offset(offset) // for decoding

repo data:
pool<object_shape> shapes_storage // size must be at least twice the size of jsm stack
fixed_stack<object_shape*> shapes_stack // for updating top-most shape when there are updates

comp data:
queue<token> tokens_to_be_written

TESTS:
shape invalidated because it was too long
shape invalidated because it was pushed out of the queue while building
opening token is written out while the shape is still building (large object)
shape invalidated because of cache, but there's still a token referencing it
id overflow

*/

constexpr int64 JSON_SHAPE_SUM_KEY_LENGTH = 1024;
constexpr int64 JSON_SHAPES_FRESH_QUEUE_SIZE = 256;
constexpr int64 JSON_SHAPES_MATURE_QUEUE_SIZE = 256;
constexpr int64 JSON_SHAPES_TOTAL_POOL_SIZE = JSON_MAX_STATE_STACK_LENGTH + JSON_SHAPES_FRESH_QUEUE_SIZE + JSON_SHAPES_MATURE_QUEUE_SIZE;
constexpr int64 _JSR_HYPOTHESIS_DEPTH = 10;

struct _json_object_shape {
  byte keys[JSON_SHAPE_SUM_KEY_LENGTH];
  /** Reference to this shape from the opening token for the object that is the source of this shape.
  It is here for two reasons:
    1. to be able to update the reference if after completion of shape, if the shape is merged with other shape
    2. it's null when the shape is completed, so it's also used for shape completeness check when object referencing this shape is being written
  It would also be null if the object start was written before the shape is complete. */
  struct _json_object_shape **source_object_shape_reference;
  /** Shape ID is used to check if the shape the object will be written with is good.
  Object opening token stores ID of a shape, and a reference to a shape; that's the ID that was given on creation.
  If ID on the object token is smaller than ID of the shape - then the shape can be used. If it's bigger - then the shape was replaced and must not be used. */
  uint32 id;
  uint16 keys_hash;
  int16 keys_length;
  fal_entry *cache_entry;
};
typedef struct _json_object_shape json_object_shape;

// TODO: recognize mono-type arrays? arrays that have all the same value types (and maybe nulls)
// TODO: recognize array-tuples?
// TODO: recognize map-types? objects with arbitrary keys that have all the same value types (and maybe nulls)

typedef struct {
  fixed_pool shapes;
  hash_chain key_hashes;
  lm_queue cache_controller;
  fixed_stack shapes_in_progress;
  uint64 id_counter;
} json_shapes_repository;

void jsr_deinit(json_shapes_repository *repo) {
  fpool_deinit(&repo->shapes);
  hash_chain_deinit(&repo->key_hashes);
  lm_queue_deinit(&repo->cache_controller);
  fstack_deinit(&repo->shapes_in_progress);

  *repo = (json_shapes_repository){0};
}

void jsr_init(json_shapes_repository *repo, context *context) {
  *repo = (json_shapes_repository){0};

  repo->id_counter = 0;
  fpool_init(&repo->shapes, context, sizeof(json_object_shape), JSON_SHAPES_TOTAL_POOL_SIZE);
  hash_chain_init(&repo->key_hashes, context, JSON_SHAPES_TOTAL_POOL_SIZE);
  lm_queue_init(&repo->cache_controller, context, JSON_SHAPES_FRESH_QUEUE_SIZE, JSON_SHAPES_MATURE_QUEUE_SIZE);
  fstack_init(&repo->shapes_in_progress, context, sizeof(json_object_shape *), JSON_MAX_STATE_STACK_LENGTH);
}

void _jsr_setup_shape(json_shapes_repository *repo, json_object_shape *shape, json_token *source_token) {
  shape->keys_hash = 0;
  shape->cache_entry = NULL;

  shape->id = repo->id_counter;
  source_token->shape_reference.shape_id = shape->id;
  repo->id_counter = repo->id_counter == UINT64_MAX ? 0 : repo->id_counter++;

  shape->source_object_shape_reference = (json_object_shape **)&source_token->shape_reference.shape;
}

uint16 _jsr_update_hash(uint16 base_hash, uint16 b, int16 index) {
  switch (index & 0x3) {
  case 0:
    return base_hash ^ b;
  case 1:
    return base_hash ^ (b << 8);
  case 2:
    return base_hash ^ (b << 4);
  default:
    return base_hash ^ ((b & 0x0f) << 12) ^ (b >> 4);
  }
}

json_object_shape *_jsr_get_current_shape(json_shapes_repository *repo) {
  return *(json_object_shape **)fstack_peek(&repo->shapes_in_progress);
}

json_object_shape *_jsr_find_matching_shape(json_shapes_repository *repo, json_object_shape *new_shape) {
  int64 pos = hash_chain_get_first_position_by_hash(&repo->key_hashes, new_shape->keys_hash);
outer:
  for (int64 i = 0; i < _JSR_HYPOTHESIS_DEPTH; i++) {
    json_object_shape *match_shape = fpool_offset_to_value(&repo->shapes, pos);
    pos = hash_chain_get_previous_position_by_position(&repo->key_hashes, pos);
    if (match_shape->cache_entry == NULL) {
      // this means that this shape is incomplete
      // which in turn means one of two things - either we found actual shape-in-building,
      // or we have found shape that was invalidated
      // in either case, we should not process it
      continue;
    }

    if (match_shape->keys_length != new_shape->keys_length || match_shape->keys_hash != new_shape->keys_hash) {
      continue;
    }

    bool is_good = true;
    for (int64 i = 0; i < match_shape->keys_length; i++) {
      if (match_shape->keys[i] != new_shape->keys[i]) {
        is_good = false;
        break;
      }
    }

    if (is_good) {
      return match_shape;
    }
  }
  return NULL;
}

void jsr_push_key_byte(json_shapes_repository *repo, byte b) {
  json_object_shape *shape = _jsr_get_current_shape(repo);
  if (shape == NULL) {
    return;
  }

  if (shape->keys_length >= JSON_SHAPE_SUM_KEY_LENGTH) {
    if (shape->source_object_shape_reference != NULL) {
      *shape->source_object_shape_reference = NULL;
    }
    shape->source_object_shape_reference = NULL;
    json_object_shape **slot = fstack_peek(&repo->shapes_in_progress);
    *slot = NULL;
    fpool_release(&repo->shapes, shape);
    return;
  }

  shape->keys_hash = _jsr_update_hash(shape->keys_hash, b, shape->keys_length);
  shape->keys[shape->keys_length] = b;
  shape->keys_length++;
}

void _jsr_drain_cache_overflow(json_shapes_repository *repo) {
  if (lm_queue_is_overflowing(&repo->cache_controller)) {
    json_object_shape *removed_shape = lm_queue_pop(&repo->cache_controller);
    removed_shape->cache_entry = NULL;
    if (removed_shape->source_object_shape_reference != NULL) {
      *removed_shape->source_object_shape_reference = NULL;
    }
    removed_shape->source_object_shape_reference = NULL;
    fpool_release(&repo->shapes, removed_shape);
  }
}

void _jsr_complete_current_shape(json_shapes_repository *repo) {
  json_object_shape **top_slot = fstack_pop(&repo->shapes_in_progress);
  json_object_shape *shape = *top_slot;
  if (shape == NULL) {
    return;
  }

  json_object_shape *older_shape = _jsr_find_matching_shape(repo, shape);
  if (older_shape) {
    older_shape->cache_entry = lm_queue_bump(&repo->cache_controller, older_shape->cache_entry);
    _jsr_drain_cache_overflow(repo);
    if (shape->source_object_shape_reference != NULL) {
      *shape->source_object_shape_reference = older_shape;
    }
    return;
  }

  if (shape->source_object_shape_reference != NULL) {
    // source token must not use this shape, as it's the first of its kind
    // because in this case, decoder won't know which shape is reference points to
    *shape->source_object_shape_reference = NULL;
  }
  shape->source_object_shape_reference = NULL;
  hash_chain_push(&repo->key_hashes, shape->keys_hash, fpool_get_offset(&repo->shapes, shape));

  shape->cache_entry = lm_queue_push(&repo->cache_controller);
  _jsr_drain_cache_overflow(repo);
}

void jsr_process_token(json_shapes_repository *repo, json_token *token, json_state state) {
  if (state == JSON_STATE_OBJECT_KEY && token->kind == JSON_TOKEN_STRING_DELIMITER) {
    // JSON disallows using bytes from 0 to 0x1f directly in the string
    // so us using such byte as a key separator is completely fine
    jsr_push_key_byte(repo, 1);
  } else if (token->kind == JSON_TOKEN_OBJECT_OPEN) {
    json_object_shape *shape = fpool_acquire(&repo->shapes);
    json_object_shape **slot = fstack_push(&repo->shapes_in_progress);
    *slot = shape;
    _jsr_setup_shape(repo, shape, token);
  } else if (token->kind == JSON_TOKEN_OBJECT_CLOSE) {
    _jsr_complete_current_shape(repo);
  }
}