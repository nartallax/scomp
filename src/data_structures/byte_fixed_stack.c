#pragma once
#include "../commons.c"
#include "../context.c"
#include <assert.h>
#include <stdbool.h>

/** A stack of fixed size that stores bytes. */
typedef struct {
  byte *values;
  size_t byte_length;
  size_t index;
} byte_fixed_stack;

NODISCARD bool bfstack_init(byte_fixed_stack *stack, context *context, size_t item_capacity) {
  *stack = (byte_fixed_stack){0};

  stack->byte_length = item_capacity;
  stack->index = 0;
  stack->values = context_allocate(context, stack->byte_length, sizeof(byte));
  return !!stack->values;
}

void bfstack_reset(byte_fixed_stack *stack) {
  stack->index = 0;
}

void bfstack_deinit(byte_fixed_stack *stack, context *context) {
  context_free(context, stack->values);
}

void bfstack_push(byte_fixed_stack *stack, byte value) {
  assert(stack->index < stack->byte_length && "Byte fixed stack overflow on push()");
  stack->values[stack->index++] = value;
}

// TODO: try adding inlining modifiers here and in other small functions
// while measuring performance
byte bfstack_peek(byte_fixed_stack *stack) {
  assert(stack->index > 0 && "Byte fixed stack underflow on peek()");
  return stack->values[stack->index - 1];
}

byte bfstack_pop(byte_fixed_stack *stack) {
  assert(stack->index > 0 && "Byte fixed stack underflow on pop()");
  return stack->values[--stack->index];
}

void bfstack_replace(byte_fixed_stack *stack, byte new_value) {
  assert(stack->index > 0 && "Byte fixed stack underflow on replace()");
  stack->values[stack->index - 1] = new_value;
}

/** Returns amount of items on the stack. */
size_t bfstack_get_count(byte_fixed_stack *stack) {
  return stack->index;
}

/** Returns amount of items the stack can hold. */
size_t bfstack_get_capacity(byte_fixed_stack *stack) {
  return stack->byte_length;
}