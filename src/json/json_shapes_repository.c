#pragma once
#include "../commons.c"

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

cache controller:
// cache entry: bool is_mature; list_ref to fresh_entries or mature_entries
cache_entry[range(max_offset)] all_entries;
fstack<offset> unoccupied_entries;
linked_list<offset> fresh_entries; // when it starts to overflow - remove oldest one
lru_cache mature_entries // lru cache that holds mature values; value is promoted to mature once its been bumped at least once. promoting value can kick out some other one
offset push_value() // returns a slot for a new value. founds a fresh entry in fresh_entries, sets it up and returns the offset. may return offset of existing value - then overwrite is expected
void bump_value(offset value); // updates position in cache. if the entry is fresh - it is promoted and removed from fresh_entries; if it's mature - it's bumped

lru cache:
linked_list<offset> values
list_ref push_value(offset)
void bump_value(list_ref)

linked list:
stack free_slots; // list of "holes" in entries
list_entry[] entries; // entry has reference to prev, back and some value
list_ref append(value) // creates a new value at the end of the list. returns a reference to that value.
void remove(list_ref) // removes the value from the list




to implement:
pool
redo fixed stack to accept not only bytes, but arbitrary values
LRU queue or smth
refactor out hash chain

*/

struct _json_object_shape {
  uint64 keyset_hash;
  /** Reference to this shape from the opening token for the object that is the source of this shape.
  It is here for two reasons:
    1. to be able to update the reference if after completion of shape, if the shape is merged with other shape
    2. it's null when the shape is completed, so it's also used for shape completeness check when object referencing this shape is being written
  It would also be null if the object start was written before the shape is complete. */
  struct _json_object_shape **source_object_shape_reference;
  // TODO: keys field?
};
typedef struct _json_object_shape json_object_shape;

// TODO: recognize mono-type arrays? arrays that have all the same value types (and maybe nulls)
// TODO: recognize array-tuples?
// TODO: recognize map-types? objects with arbitrary keys that have all the same value types (and maybe nulls)

typedef struct {
  // TODO: stack with top N objects/arrays
  // TODO: hash chain with known shapes
} json_shapes_repository;