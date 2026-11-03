#pragma once
#include "../commons.c"
#include "../context.c"
#include <assert.h>
#include <stdbool.h>

/** A stack of fixed size. */
typedef struct {
  byte *values;
  // index of first unoccupied space in the queue
  int64 index;
  int64 item_size;
  int64 item_capacity;
} fixed_stack;

void fstack_deinit(fixed_stack *stack) {
  *stack = (fixed_stack){0};
}

void fstack_init(fixed_stack *stack, context *context, int64 item_size, int64 item_capacity) {
  *stack = (fixed_stack){0};

  stack->item_capacity = item_capacity;
  stack->item_size = item_size;
  stack->index = 0;
  stack->values = context_arena_allocate(context, stack->item_capacity, item_size);
}

void fstack_reset(fixed_stack *stack) {
  stack->index = 0;
}

INLINE void *fstack_push(fixed_stack *stack) {
  assert(stack->index < stack->item_capacity && "Fixed stack overflow on push()");
  return &stack->values[(stack->index++) * stack->item_size];
}

INLINE void *fstack_peek(fixed_stack *stack) {
  assert(stack->index > 0 && "Byte fixed stack underflow on peek()");
  return &stack->values[(stack->index - 1) * stack->item_size];
}

INLINE void *fstack_pop(fixed_stack *stack) {
  assert(stack->index > 0 && "Byte fixed stack underflow on pop()");
  return &stack->values[(--stack->index) * stack->item_size];
}

/** Returns amount of items on the stack. */
INLINE int64 fstack_get_count(fixed_stack *stack) {
  return stack->index;
}

/** Returns amount of items the stack can hold. */
INLINE int64 fstack_get_capacity(fixed_stack *stack) {
  return stack->item_capacity;
}