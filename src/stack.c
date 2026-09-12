#pragma once
#include "commons.c"
#include "context.c"

#define STACK_DEFAULT_LENGTH 16

/** Stack data structure.
Just like queue, may contain arbitrary sized values, and because of that operates with pointers. */
typedef struct {
  context *context;
  size_t value_size;
  byte *values;
  // length of `values`, in elements.
  size_t length;
  // amount of elements in `values`
  size_t count;
} stack;

// TODO: consider reducing indirection by making small data structures like this (stack, queue) to be inlineable
// to achieve this, we must convert this allocator into init function
stack *stack_new(context *context, size_t value_size) {
  stack *result = context_allocate(context, 1, sizeof(stack));
  if (!result) {
    return NULL;
  }

  result->context = context;
  result->value_size = value_size;
  result->length = STACK_DEFAULT_LENGTH;
  result->count = 0;
  result->values = context_allocate(context, result->length, value_size);
  if (!result->values) {
    context_free(context, result);
    return NULL;
  }

  return result;
}

/** If values are heap-allocated, callers must take care of them, as they will not be deleted in this function */
void stack_delete(stack *stack) {
  context_free(stack->context, stack->values);
  context_free(stack->context, stack);
}

bool _stack_maybe_grow(stack *stack) {
  if (stack->count < stack->length) {
    return true;
  }

  size_t new_length = stack->length * 2;
  byte *new_values = context_reallocate(stack->context, stack->values, new_length, stack->value_size);
  if (!new_values) {
    return false;
  }
  stack->values = new_values;
  stack->length = new_length;
  return true;
}

void *stack_peek(stack *stack) {
  return stack->values + ((stack->count - 1) * stack->value_size);
}

void *stack_push(stack *stack) {
  stack->count++;
  if (!_stack_maybe_grow(stack)) {
    stack->count--;
    return NULL;
  }

  // get the pointer AFTER underlying array has grown
  // otherwise there's a chance to get pointer in an array that will be moved right after that
  return stack_peek(stack);
}

void *stack_pop(stack *stack) {
  stack->count--;
  return stack->values + (stack->count * stack->value_size);
}

size_t stack_get_count(stack *stack) {
  return stack->count;
}