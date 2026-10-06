#pragma once
#include "../arithmetic_coding/frequency_table.c"
#include "../commons.c"
#include "../context.c"
#include "../data_structures/ring_buffer.c"
#include "byte_stream_matcher.c"

constexpr symbol _STRCOMP_BACKREFERENCE_SYMBOL = 256;
constexpr symbol _STRCOMP_EOF_SYMBOL = 257;
constexpr symbol _STRCOMP_BIT_TERMINATOR_SYMBOL = 2;
// TODO: tune this
constexpr size_t _STRCOMP_MIN_BACKREFERENCE_LENGTH = 5;

typedef struct {
  ftable main_symbol_table;
  ftable raw_bit_with_terminator_table;
  ring_buffer buffer;
} string_compression_base;

void _strcomp_deinit_base(string_compression_base *base, context *context) {
  ftable_deinit(&base->main_symbol_table, context);
  ftable_deinit(&base->raw_bit_with_terminator_table, context);
  ring_buffer_deinit(&base->buffer, context);
}

NODISCARD bool _strcomp_init_base(string_compression_base *base, context *context) {
  *base = (string_compression_base){0};

  if (!ftable_init(&base->main_symbol_table, context, 258, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
    // TODO: go over inits everywhere and call deinit instead of repeating all the fields
    _strcomp_deinit_base(base, context);
    return false;
  }

  if (!ftable_init(&base->raw_bit_with_terminator_table, context, 3, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF)) {
    _strcomp_deinit_base(base, context);
    return false;
  }

  if (!ring_buffer_init(&base->buffer, context, _BSM_MATCH_LENGTH_SHIFT)) {
    _strcomp_deinit_base(base, context);
    return false;
  }

  return true;
}
