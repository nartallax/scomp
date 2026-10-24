#pragma once
#include "../commons.c"
#include "../context.c"

/** Stack data structure.
Just like queue, may contain arbitrary sized values, and because of that operates with pointers. */
typedef struct {
  context *context;
  int64_t value_size;
  byte *values;
  // length of `values`, in elements.
  int64_t length;
  // amount of elements in `values`
  int64_t count;
} stack;

NODISCARD bool stack_init(stack *s, context *context, int64_t value_size) {
  *s = (stack){0};

  s->context = context;
  s->value_size = value_size;
  s->length = 16;
  s->count = 0;
  s->values = context_allocate(context, s->length, value_size);

  return s->values != NULL;
}

/** If values are heap-allocated, callers must take care of them, as they will not be deleted in this function */
void stack_deinit(stack *s, context *context) {
  context_free(context, s->values);

  *s = (stack){0};
}

NODISCARD bool _stack_maybe_grow(stack *stack) {
  if (stack->count < stack->length) {
    return true;
  }

  int64_t new_length = stack->length * 2;
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

int64_t stack_get_count(stack *stack) {
  return stack->count;
}