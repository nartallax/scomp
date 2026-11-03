#pragma once
#include "../commons.c"
#include "../context.c"
#include "fixed_stack.c"

struct _fixed_array_list_entry {
  struct _fixed_array_list_entry *prev;
  struct _fixed_array_list_entry *next;
  int64 value;
};

typedef struct _fixed_array_list_entry fal_entry;

/** Double-linked list backed by an array of fixed size.
Acts like a queue; allows to remove values from any position. */
typedef struct {
  fal_entry *entries;
  fixed_stack free_slots;
  fal_entry *head;
  fal_entry *tail;
} fixed_array_list;

void fal_init(fixed_array_list *fal, context *context, int64 length) {
  *fal = (fixed_array_list){0};

  fal->head = NULL;
  fal->entries = context_arena_allocate(context, length, sizeof(fal_entry));
  fstack_init(&fal->free_slots, context, sizeof(fal_entry *), length);
  for (int64 i = length - 1; i >= 0; i--) {
    fal_entry **slot = fstack_push(&fal->free_slots);
    *slot = &fal->entries[i];
  }
}

void fal_deinit(fixed_array_list *fal) {
  fstack_deinit(&fal->free_slots);

  *fal = (fixed_array_list){0};
}

INLINE bool fal_has_free_space(fixed_array_list *fal) {
  return fstack_get_count(&fal->free_slots) > 0;
}

INLINE bool fal_is_empty(fixed_array_list *fal) {
  return fstack_get_count(&fal->free_slots) == fstack_get_capacity(&fal->free_slots);
}

INLINE fal_entry *fal_push_head(fixed_array_list *fal, int64 value) {
  assert(fal_has_free_space(fal) && "Must have free space on push");
  fal_entry **slot = fstack_pop(&fal->free_slots);
  fal_entry *new_entry = *slot;
  new_entry->next = NULL;
  new_entry->prev = fal->head;
  new_entry->value = value;

  if (fal->head) {
    fal->head->next = new_entry;
  } else {
    fal->tail = new_entry;
  }
  fal->head = new_entry;

  return new_entry;
}

INLINE void fal_remove_entry(fixed_array_list *fal, fal_entry *entry) {
  assert(!fal_is_empty(fal) && "Cannot remove entry from an empty list");

  fal_entry **slot = fstack_push(&fal->free_slots);
  *slot = entry;

  if (entry->prev) {
    entry->prev->next = entry->next;
  }
  if (entry->next) {
    entry->next->prev = entry->prev;
  }
  if (entry == fal->head) {
    fal->head = entry->prev;
  }
  if (entry == fal->tail) {
    fal->tail = entry->next;
  }
}

INLINE fal_entry *fal_pop_tail(fixed_array_list *fal) {
  assert(!fal_is_empty(fal) && "Cannot remove entry from an empty list");
  fal_entry *tail = fal->tail;
  fal_remove_entry(fal, tail);
  return tail;
}