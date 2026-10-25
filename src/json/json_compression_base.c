#pragma once
#include "../arithmetic_coding/frequency_table.c"
#include "../arithmetic_coding/uint_freq_tables.c"
#include "../commons.c"
#include "../context.c"
#include "json_base.c"
#include <stdint.h>

typedef struct {
  // TODO: this is too generic.
  // states for some tables won't ever be used; see true/false/number etc
  // also we may want to group some tables, like object_start + object_cont, object_key + string
  // I don't have a solid idea how this can be easily improved, but this feels cringe
  ftable tables[JSON_STATE_MAX + 1];
  ftable whitespaces;
  ftable escape_sequences;
  json_state current_state;
  uint_freq_tables number_int_tables;
  uint_freq_tables number_frac_zero_tables;
  uint_freq_tables number_frac_tables;
  uint_freq_tables number_expo_zero_tables;
  uint_freq_tables number_expo_tables;
  // TODO: think about better ways to write numbers' flags
  // right now flags are rearranged in a way that more common numbers will have numerically small flags
  // and yet, it's possible that if we write flags as separate two-symbol tables - they will be better overall
  // I just don't want to think about it right now
  uint_freq_tables number_flag_tables;
} json_compression_base;

void _json_compression_base_deinit(json_compression_base *base, context *context) {
  for (int32 i = 0; i < JSON_STATE_MAX + 1; i++) {
    ftable_deinit(&base->tables[i], context);
  }

  ftable_deinit(&base->whitespaces, context);
  ftable_deinit(&base->escape_sequences, context);

  uint_freq_tables_deinit(&base->number_int_tables, context);
  uint_freq_tables_deinit(&base->number_frac_tables, context);
  uint_freq_tables_deinit(&base->number_frac_zero_tables, context);
  uint_freq_tables_deinit(&base->number_expo_tables, context);
  uint_freq_tables_deinit(&base->number_expo_zero_tables, context);
  uint_freq_tables_deinit(&base->number_flag_tables, context);

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

  // TODO: test off-by-one here?
  if (!ftable_init(&base->whitespaces, context, sizeof(_JTOK_WHITESPACE_CHARS), FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
    _json_compression_base_deinit(base, context);
    return false;
  }

  // TODO: test off-by-one here?
  if (!ftable_init(&base->escape_sequences, context, sizeof(_JTOK_ESCAPE_SEQ_CHARS), FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
    _json_compression_base_deinit(base, context);
    return false;
  }

  if (!uint_freq_tables_init(&base->number_int_tables, context, UINT64_MAX)) {
    _json_compression_base_deinit(base, context);
    return false;
  }

  if (!uint_freq_tables_init(&base->number_frac_tables, context, UINT16_MAX)) {
    _json_compression_base_deinit(base, context);
    return false;
  }

  if (!uint_freq_tables_init(&base->number_frac_zero_tables, context, JSON_MAX_NUMBER_LEADING_ZEROES)) {
    _json_compression_base_deinit(base, context);
    return false;
  }

  if (!uint_freq_tables_init(&base->number_expo_tables, context, UINT16_MAX)) {
    _json_compression_base_deinit(base, context);
    return false;
  }

  if (!uint_freq_tables_init(&base->number_expo_zero_tables, context, JSON_MAX_NUMBER_LEADING_ZEROES)) {
    _json_compression_base_deinit(base, context);
    return false;
  }

  if (!uint_freq_tables_init(&base->number_flag_tables, context, JSON_NUMBER_FLAGS_MAX)) {
    _json_compression_base_deinit(base, context);
    return false;
  }

  return true;
}

INLINE ftable *_json_compression_base_get_current_ftable(json_compression_base *base) {
  return &base->tables[base->current_state];
}

INLINE byte _json_compression_get_escape_seq_char_by_symbol(symbol s) {
  return _JTOK_ESCAPE_SEQ_CHARS[s];
}

INLINE symbol _json_compression_get_symbol_by_escape_seq_char(byte b) {
  for (symbol i = 0; i < sizeof(_JTOK_ESCAPE_SEQ_CHARS); i++) {
    if (b == _JTOK_ESCAPE_SEQ_CHARS[i]) {
      return i;
    }
  }
  assert(false && "Unreachable");
}

INLINE byte _json_compression_get_whitespace_char_by_symbol(symbol s) {
  return _JTOK_WHITESPACE_CHARS[s];
}

INLINE symbol _json_compression_get_symbol_by_whitespace_char(byte b) {
  for (symbol i = 0; i < sizeof(_JTOK_WHITESPACE_CHARS); i++) {
    if (b == _JTOK_WHITESPACE_CHARS[i]) {
      return i;
    }
  }
  assert(false && "Unreachable");
}

// TODO: handle uint-in-a-string? if it fits in a uint64
// TODO: diffencode numbers?