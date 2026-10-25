#pragma once
#include "../arithmetic_coding/frequency_table.c"
#include "../arithmetic_coding/uint_freq_tables.c"
#include "../commons.c"
#include "../context.c"
#include "byte_stream_matcher.c"
#include <limits.h>

// tuneables
constexpr int64 _STRCOMP_UINT_SYMBOL_BITS_COUNT = 4; // one symbol, which is part of uint, contains this much bits of information
constexpr int64 _STRCOMP_UINT_SYMBOL_MASK = (1 << _STRCOMP_UINT_SYMBOL_BITS_COUNT) - 1;

constexpr int64 _STRCOMP_MIN_BACKREFERENCE_LENGTH = 5;

constexpr symbol _STRCOMP_BACKREFERENCE_SYMBOL = 256;

typedef struct {
  // frequency table for just characters, start-of-backreference symbol, or EOF
  ftable main_symbol_table;
  uint_freq_tables offset_tables;
  uint_freq_tables length_tables;
} string_compression_base;

void _strcomp_deinit_base(string_compression_base *base, context *context) {
  ftable_deinit(&base->main_symbol_table, context);
  uint_freq_tables_deinit(&base->offset_tables, context);
  uint_freq_tables_deinit(&base->length_tables, context);

  *base = (string_compression_base){0};
}

NODISCARD bool _strcomp_init_base(string_compression_base *base, context *context) {
  *base = (string_compression_base){0};

  if (!ftable_init(&base->main_symbol_table, context, _STRCOMP_BACKREFERENCE_SYMBOL + 1, FTABLE_INIT_ONE | FTABLE_INCLUDE_EOF)) {
    _strcomp_deinit_base(base, context);
    return false;
  }

  if (!uint_freq_tables_init(&base->offset_tables, context, _BSM_LENGTH)) {
    _strcomp_deinit_base(base, context);
    return false;
  }

  if (!uint_freq_tables_init(&base->length_tables, context, _BSM_MATCH_LENGTH_LIMIT)) {
    _strcomp_deinit_base(base, context);
    return false;
  }

  return true;
}
