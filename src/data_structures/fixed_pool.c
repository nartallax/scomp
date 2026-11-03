// #pragma once
// #include "../commons.c"
// #include "../context.c"
// #include "queue.c"

// /** A region of memory, of fixed size, that holds some amount of values of the same type.
// This data structure allows to add or remove elements in O(1) */
// typedef struct {
//   byte *data;
//   int64 item_size;
//   fqueue free_slots;
// } fixed_pool;

// void fpool_init(fixed_pool *pool, context *context, int64 item_size) {
// }