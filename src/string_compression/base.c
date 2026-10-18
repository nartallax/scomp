#pragma once
#include "../arithmetic_coding/frequency_table.c"
#include "../commons.c"
#include "../context.c"
#include "../data_structures/ring_buffer.c"
#include "byte_stream_matcher.c"

constexpr symbol _STRCOMP_BACKREFERENCE_SYMBOL = 256;
constexpr symbol _STRCOMP_EOF_SYMBOL = 257;           // TODO: use real eof?
constexpr size_t _STRCOMP_UINT_SYMBOL_BITS_COUNT = 3; // one symbol, which is part of uint, contains this much bits of information
constexpr size_t _STRCOMP_UINT_SYMBOL_MASK = (1 << _STRCOMP_UINT_SYMBOL_BITS_COUNT) - 1;
constexpr size_t _STRCOMP_UINT_TERMINATOR_SYMBOL = 1 << _STRCOMP_UINT_SYMBOL_BITS_COUNT;
constexpr size_t _STRCOMP_OFFSET_FTABLE_COUNT = (_BSM_LENGTH_SHIFT + (_STRCOMP_UINT_SYMBOL_BITS_COUNT - 1)) / _STRCOMP_UINT_SYMBOL_BITS_COUNT;
constexpr size_t _STRCOMP_LENGTH_FTABLE_COUNT = (_BSM_MATCH_LENGTH_SHIFT + (_STRCOMP_UINT_SYMBOL_BITS_COUNT - 1)) / _STRCOMP_UINT_SYMBOL_BITS_COUNT;

// TODO: tune this
constexpr size_t _STRCOMP_MIN_BACKREFERENCE_LENGTH = 6;
constexpr size_t _STRCOMP_ALLOWED_OFFSET_PER_BACKREFERENCE_BYTE = 7500;

typedef struct {
  ftable main_symbol_table;
  ring_buffer buffer;
  ftable backref_offset_ftables[_STRCOMP_OFFSET_FTABLE_COUNT];
  ftable backref_length_ftables[_STRCOMP_LENGTH_FTABLE_COUNT];
} string_compression_base;

void _strcomp_deinit_base(string_compression_base *base, context *context) {
  ftable_deinit(&base->main_symbol_table, context);

  for (size_t i = 0; i < _STRCOMP_OFFSET_FTABLE_COUNT; i++) {
    ftable_deinit(&base->backref_offset_ftables[i], context);
  }

  for (size_t i = 0; i < _STRCOMP_LENGTH_FTABLE_COUNT; i++) {
    ftable_deinit(&base->backref_length_ftables[i], context);
  }

  ring_buffer_deinit(&base->buffer, context);
}

NODISCARD bool _strcomp_init_base(string_compression_base *base, context *context) {
  *base = (string_compression_base){0};

  if (!ftable_init(&base->main_symbol_table, context, _STRCOMP_EOF_SYMBOL + 1, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
    _strcomp_deinit_base(base, context);
    return false;
  }

  if (!ring_buffer_init(&base->buffer, context, _BSM_MATCH_LENGTH_SHIFT)) {
    _strcomp_deinit_base(base, context);
    return false;
  }

  for (size_t i = 0; i < _STRCOMP_OFFSET_FTABLE_COUNT; i++) {
    if (!ftable_init(&base->backref_offset_ftables[i], context, (1 << _STRCOMP_UINT_SYMBOL_BITS_COUNT) + 1, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
      _strcomp_deinit_base(base, context);
      return false;
    }
  }

  for (size_t i = 0; i < _STRCOMP_LENGTH_FTABLE_COUNT; i++) {
    if (!ftable_init(&base->backref_length_ftables[i], context, (1 << _STRCOMP_UINT_SYMBOL_BITS_COUNT) + 1, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
      _strcomp_deinit_base(base, context);
      return false;
    }
  }

  return true;
}
