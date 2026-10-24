#pragma once
#include "../arithmetic_coding/frequency_table.c"
#include "../commons.c"
#include "../context.c"
#include "json_base.c"

typedef struct {
  ftable tables[JSON_STATE_MAX + 1];
  ftable whitespaces;
  ftable escape_sequences;
  json_state current_state;
} json_compression_base;

void _json_compression_base_deinit(json_compression_base *base, context *context) {
  for (int32 i = 0; i < JSON_STATE_MAX + 1; i++) {
    ftable_deinit(&base->tables[i], context);
  }

  *base = (json_compression_base){0};
}

void _json_compression_base_reset(json_compression_base *base) {
  base->current_state = JSON_STATE_START;
}

NODISCARD bool _json_compression_base_init(json_compression_base *base, context *context) {
  *base = (json_compression_base){0};

  _json_compression_base_reset(base);

  for (int32 i = 0; i < JSON_STATE_MAX + 1; i++) {
    // in theory we could make tables smaller than that
    // as in, there's no way that value starts with comma token for example, so we don't need to include comma for value table
    // but this would require additional step of mapping token ids to symbols in the table, which complicates things
    if (!ftable_init(&base->tables[i], context, JSON_TOKEN_MAX + 1, FTABLE_INIT_ONE | FTABLE_INCLUDE_EOF)) {
      _json_compression_base_deinit(base, context);
      return false;
    }
  }

  return true;
}

INLINE ftable *_json_compression_base_get_current_ftable(json_compression_base *base) {
  return &base->tables[base->current_state];
}

// TODO: handle uint-in-a-string? if it fits in a uint64
// TODO: diffencode numbers?