#pragma once
#include "../commons.c"
#include "../context.c"
#include <assert.h>
#include <stdbool.h>

/** A stack of fixed size that stores values up to 4 bits. */
typedef struct {
  byte *values;
  size_t byte_length;
  size_t item_capacity;
  size_t index;
} halfbyte_fixed_stack;

NODISCARD bool hbfstack_init(halfbyte_fixed_stack *stack, context *context, size_t item_capacity) {
  *stack = (halfbyte_fixed_stack){0};
  assert((item_capacity & 1) == 0 && "Half-byte stack expects its item capacity to be even");

  stack->byte_length = item_capacity / 2;
  stack->item_capacity = item_capacity;
  stack->index = 0;
  stack->values = context_allocate(context, stack->byte_length, sizeof(byte));
  return !!stack->values;
}

void hbfstack_deinit(halfbyte_fixed_stack *stack, context *context) {
  context_free(context, stack->values);
}

void hbfstack_push(halfbyte_fixed_stack *stack, byte value) {
  assert(value < 0x0f && "Half-byte stack only accepts values from 0 to 15");
  assert(stack->index < stack->item_capacity && "Half-byte fixed stack overflow on push()");
  size_t byte_index = stack->index >> 1;
  byte result_value = stack->index & 1 ? stack->values[byte_index] | (value << 4) : value;
  stack->values[byte_index] = result_value;
  stack->index++;
}

byte hbfstack_pop(halfbyte_fixed_stack *stack) {
  assert(stack->index > 0 && "Half-byte fixed stack underflow on pop()");
  stack->index--;
  byte value = stack->values[stack->index >> 1];
  byte result = stack->index & 1 ? value >> 4 : value & 0x0f;
  return result;
}

/** Returns amount of items (half-bytes) on the stack. */
size_t hbfstack_get_count(halfbyte_fixed_stack *stack) {
  return stack->index;
}

/** Returns amount of items (half-bytes) the stack can hold. */
size_t hbfstack_get_capacity(halfbyte_fixed_stack *stack) {
  return stack->item_capacity;
}