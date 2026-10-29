#pragma once
#import "../arithmetic_coding/decoder.c"
#import "../arithmetic_coding/uint_reader.c"
#import "../context.c"
#import "../string_compression/string_decompressor.c"
#import "json_compression_base.c"
#import "json_state_machine.c"

typedef byte json_decomp_state;

constexpr json_decomp_state JSON_DECOMP_DEFAULT = 0;
constexpr json_decomp_state JSON_DECOMP_WHITESPACE = 1;
constexpr json_decomp_state JSON_DECOMP_NUMBER_START = 2;
constexpr json_decomp_state JSON_DECOMP_NUMBER_INT = 3;
constexpr json_decomp_state JSON_DECOMP_NUMBER_FRAC = 4;
constexpr json_decomp_state JSON_DECOMP_NUMBER_EXPO = 5;

typedef struct {
  string_decompressor *strdecomp;
  acod_decoder *decoder;
  context *context;
  json_compression_base base;
  json_decomp_state state;
  bool expecting_string_delimiter;
  uint_reader_state uint_reader;
  json_state_machine jsm;
} json_decompressor;

void json_decompressor_deinit(json_decompressor *decomp, context *context) {
  _json_compression_base_deinit(&decomp->base, context);
  jsm_deinit(&decomp->jsm, context);

  *decomp = (json_decompressor){0};
}

void json_decompressor_reset(json_decompressor *decomp) {
  _json_compression_base_reset(&decomp->base);
  uint_reader_reset(&decomp->uint_reader);
  jsm_reset(&decomp->jsm);
  decomp->expecting_string_delimiter = false;
  decomp->state = JSON_DECOMP_DEFAULT;
}

NODISCARD bool json_decompressor_init(json_decompressor *decomp, context *context, acod_decoder *decoder, string_decompressor *strdecomp) {
  *decomp = (json_decompressor){0};

  decomp->strdecomp = strdecomp;
  decomp->decoder = decoder;
  decomp->context = context;

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
  if (decomp->expecting_string_delimiter) {
    if (kind != JSON_TOKEN_STRING_DELIMITER) {
      context_set_error(decomp->context, "Illegal sequence of JSON tokens discovered when decompressing: expected string delimiter, but got %i instead", kind);
      return false;
    }
    decomp->expecting_string_delimiter = false;
    return true;
  }

  if (!jsm_process_token(&decomp->jsm, kind)) {
    context_set_error(decomp->context, "Illegal sequence of JSON tokens discovered when decompressing: unexpected %i", kind);
    return false;
  }
  decomp->base.current_state = jsm_get_current_state(&decomp->jsm);

  switch (kind) {
  case JSON_TOKEN_NUMBER:
    decomp->state = JSON_DECOMP_NUMBER_START;
    break;
  case JSON_TOKEN_WHITESPACE:
    decomp->state = JSON_DECOMP_WHITESPACE;
    break;
  default:
    break;
  }

  return true;
}

NODISCARD INLINE bool _json_decompressor_read_string(json_decompressor *decomp) {
  if (!strdecomp_read(decomp->strdecomp)) {
    // we don't update JSM on each individual string character, nothing it can do anyway
    // but if strdecomp discovered EOF - this means we have found string delimiter
    if (!_json_decompressor_update_jsm(decomp, JSON_TOKEN_STRING_DELIMITER)) {
      return false;
    }

    // now, we have not truly found string delimiter yet. but it is expected to be the next symbol
    decomp->expecting_string_delimiter = true;
  }

  return true;
}

NODISCARD bool _json_decompressor_read_nonstring(json_decompressor *decomp) {
  switch (decomp->state) {
  case JSON_DECOMP_DEFAULT: {
    ftable *table = _json_compression_base_get_current_ftable(&decomp->base);
    json_token_kind kind = acod_decoder_read(decomp->decoder, table);
    ftable_increment(table, kind);
    if (!_json_decompressor_update_jsm(decomp, kind)) {
      return false;
    }

  } break;
  }

  return true;
}

/** Reads symbols from decoder.
Returns false when EOF is encountered, or true when decoder has no more symbols. */
NODISCARD bool json_decompressor_read(json_decompressor *decomp) {
  if (json_is_string_state(decomp->base.current_state)) {
    if (!_json_decompressor_read_string(decomp)) {
      return false;
    }
  }

  while (acod_decoder_has_symbol(decomp->decoder)) {
    if (!_json_decompressor_read_nonstring(decomp)) {
      return false;
    }
  }

  return true;
}