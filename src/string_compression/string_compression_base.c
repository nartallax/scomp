#pragma once
#include "../arithmetic_coding/frequency_table.c"
#include "../commons.c"
#include "../context.c"
#include "byte_stream_matcher.c"

// tuneables
constexpr int64 _STRCOMP_UINT_SYMBOL_BITS_COUNT = 4; // one symbol, which is part of uint, contains this much bits of information
constexpr int64 _STRCOMP_MIN_BACKREFERENCE_LENGTH = 5;

constexpr symbol _STRCOMP_BACKREFERENCE_SYMBOL = 256;
constexpr symbol _STRCOMP_EOF_SYMBOL = 257; // TODO: use real eof?
constexpr int64 _STRCOMP_UINT_SYMBOL_MASK = (1 << _STRCOMP_UINT_SYMBOL_BITS_COUNT) - 1;
constexpr int64 _STRCOMP_UINT_TERMINATOR_SYMBOL = 1 << _STRCOMP_UINT_SYMBOL_BITS_COUNT;

typedef struct {
  // frequency table for just characters, start-of-backreference symbol, or EOF
  ftable main_symbol_table;

  // ..._length frequency tables are used when writing amount of symbols part of backreference (offset or length) will use
  // uint (offset or length) is broken into groups of bits; each group is used as a symbol
  ftable backref_offset_length;
  ftable backref_length_length;
  // this table is used when writing all groups of bits of offset or length but last
  // it's always stays at 1; when we are using this table, probability of each symbol is equal,
  // so there's no reason to increment it.
  ftable uint_parts;
  // ..._last_part tables are used when writing last group of bits of offset or length.
  // those tables are incremented after use;
  // last group of symbols is the most significant, and is much more likely to be skewed
  // (because offsets and lengths are not equally distributed - smaller offsets are much more likely)
  ftable backref_offset_last_part;
  ftable backref_length_last_part;
} string_compression_base;

void _strcomp_deinit_base(string_compression_base *base, context *context) {
  ftable_deinit(&base->main_symbol_table, context);
  ftable_deinit(&base->uint_parts, context);
  ftable_deinit(&base->backref_offset_last_part, context);
  ftable_deinit(&base->backref_length_last_part, context);
  ftable_deinit(&base->backref_length_length, context);
  ftable_deinit(&base->backref_offset_length, context);
}

NODISCARD bool _strcomp_init_base(string_compression_base *base, context *context) {
  *base = (string_compression_base){0};

  if (!ftable_init(&base->main_symbol_table, context, _STRCOMP_EOF_SYMBOL + 1, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
    _strcomp_deinit_base(base, context);
    return false;
  }

  if (!ftable_init(&base->uint_parts, context, (1 << _STRCOMP_UINT_SYMBOL_BITS_COUNT), FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
    _strcomp_deinit_base(base, context);
    return false;
  }

  if (!ftable_init(&base->backref_offset_last_part, context, (1 << _STRCOMP_UINT_SYMBOL_BITS_COUNT), FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
    _strcomp_deinit_base(base, context);
    return false;
  }

  if (!ftable_init(&base->backref_length_last_part, context, (1 << _STRCOMP_UINT_SYMBOL_BITS_COUNT), FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
    _strcomp_deinit_base(base, context);
    return false;
  }

  if (!ftable_init(&base->backref_length_length, context, ((_BSM_MATCH_LENGTH_SHIFT + _STRCOMP_UINT_SYMBOL_BITS_COUNT - 1) / _STRCOMP_UINT_SYMBOL_BITS_COUNT) + 1,
                   FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
    _strcomp_deinit_base(base, context);
    return false;
  }

  if (!ftable_init(&base->backref_offset_length, context, ((_BSM_LENGTH_SHIFT + _STRCOMP_UINT_SYMBOL_BITS_COUNT - 1) / _STRCOMP_UINT_SYMBOL_BITS_COUNT) + 1, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
    _strcomp_deinit_base(base, context);
    return false;
  }

  return true;
}
