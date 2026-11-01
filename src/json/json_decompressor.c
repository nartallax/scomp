#pragma once
#include "../arithmetic_coding/decoder.c"
#include "../arithmetic_coding/uint_reader.c"
#include "../context.c"
#include "../data_structures/writer.c"
#include "../string_compression/string_decompressor.c"
#include "json_compression_base.c"
#include "json_detokenizer.c"
#include "json_state_machine.c"

typedef struct {
  string_decompressor *strdecomp;
  acod_decoder *decoder;
  writer *writer;
  context *context;
  json_compression_base base;
  json_state_machine jsm;
} json_decompressor;

void json_decompressor_deinit(json_decompressor *decomp, context *context) {
  _json_compression_base_deinit(&decomp->base, context);
  jsm_deinit(&decomp->jsm, context);

  *decomp = (json_decompressor){0};
}

void json_decompressor_reset(json_decompressor *decomp) {
  _json_compression_base_reset(&decomp->base);
  jsm_reset(&decomp->jsm);
}

NODISCARD bool json_decompressor_init(json_decompressor *decomp, context *context, acod_decoder *decoder, string_decompressor *strdecomp, writer *writer) {
  *decomp = (json_decompressor){0};

  decomp->strdecomp = strdecomp;
  decomp->decoder = decoder;
  decomp->context = context;
  decomp->writer = writer;

  if (!_json_compression_base_init(&decomp->base, context)) {
    json_decompressor_deinit(decomp, context);
    return false;
  }

  if (!jsm_init(&decomp->jsm, context)) {
    json_decompressor_deinit(decomp, context);
    return false;
  }

  json_decompressor_reset(decomp);

  return true;
}

NODISCARD INLINE bool _json_decompressor_update_jsm(json_decompressor *decomp, json_token_kind kind) {
  if (!jsm_process_token(&decomp->jsm, kind)) {
    context_set_error(decomp->context, "Illegal sequence of JSON tokens discovered when decompressing: unexpected %i", kind);
    return false;
  }
  decomp->base.current_state = jsm_get_current_state(&decomp->jsm);
  return true;
}

NODISCARD INLINE bool _json_decompressor_read_string(json_decompressor *decomp) {
  if (!strdecomp_read(decomp->strdecomp)) {
    ftable *table = _json_compression_base_get_current_ftable(&decomp->base);
    symbol s = acod_decoder_read(decomp->decoder, table);
    // printf("Reading token: %i (state: %i)\n", s, decomp->base.current_state);
    ftable_increment(table, s);
    if (s == ftable_get_eof_symbol(table)) {
      return false;
    }

    json_token_kind kind = s;
    if (kind != JSON_TOKEN_STRING_DELIMITER) {
      context_set_error(decomp->context, "Illegal sequence of JSON tokens discovered when decompressing: expected string delimiter, but got %i instead", kind);
      return false;
    }

    if (!_json_decompressor_update_jsm(decomp, JSON_TOKEN_STRING_DELIMITER)) {
      return false;
    }

    json_token token = {.kind = kind};
    json_detokenizer_write(decomp->writer, &token);
  }

  return true;
}

NODISCARD bool _json_decompressor_read_nonstring(json_decompressor *decomp) {
  ftable *table = _json_compression_base_get_current_ftable(&decomp->base);
  symbol s = acod_decoder_read(decomp->decoder, table);
  // printf("Reading token: %i (state: %i)\n", s, decomp->base.current_state);
  if (s == ftable_get_eof_symbol(table)) {
    return false;
  }
  ftable_increment(table, s);

  json_token_kind kind = s;
  if (!_json_decompressor_update_jsm(decomp, kind)) {
    return false;
  }

  switch (kind) {
  case JSON_TOKEN_WHITESPACE: {
    symbol whitespace = acod_decoder_read(decomp->decoder, &decomp->base.whitespaces);
    ftable_increment(&decomp->base.whitespaces, whitespace);
    json_token token = {.kind = kind, .character = {.character = _json_compression_get_whitespace_char_by_symbol(whitespace)}};
    // printf("Reading whitespace symbol: %i\n", whitespace);
    json_detokenizer_write(decomp->writer, &token);
    return true;
  }
  case JSON_TOKEN_NUMBER: {
    json_number_token number = {0};
    number.flags = uint_reader_read(&decomp->base.number_flag_tables, decomp->decoder);
    if (!(number.flags & JSON_NUMBER_INTEGER_PART_IS_ZERO)) {
      number.integer_part = uint_reader_read(&decomp->base.number_int_tables, decomp->decoder);
    }
    if (number.flags & JSON_NUMBER_HAS_FRACTION) {
      number.fraction_leading_zeroes = uint_reader_read(&decomp->base.number_frac_zero_tables, decomp->decoder);
      number.fraction_part = uint_reader_read(&decomp->base.number_frac_tables, decomp->decoder);
    }
    if (number.flags & JSON_NUMBER_HAS_EXPONENT) {
      number.exponent_leading_zeroes = uint_reader_read(&decomp->base.number_expo_zero_tables, decomp->decoder);
      number.exponent_part = uint_reader_read(&decomp->base.number_expo_tables, decomp->decoder);
    }
    json_token token = {.kind = kind, .number = number};
    json_detokenizer_write(decomp->writer, &token);
    return true;
  }
  default: {
    json_token token = {.kind = kind};
    json_detokenizer_write(decomp->writer, &token);
    return true;
  }
  }
}

/** Read a sequence of symbols from decoder, containingn one token.
Returns false on EOF. */
NODISCARD bool json_decompressor_process_next_token(json_decompressor *decomp) {
  if (json_is_string_state(decomp->base.current_state)) {
    return _json_decompressor_read_string(decomp);
  } else {
    return _json_decompressor_read_nonstring(decomp);
  }
}

// TODO: test for what happens if outer stream ends prematurely on decoding.
// we will have to feed zeroes to the decoder to bleed out all the symbols, but will we detect absolutely broken stream?