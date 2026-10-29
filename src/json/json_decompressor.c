#pragma once
#import "../arithmetic_coding/decoder.c"
#import "../arithmetic_coding/uint_reader.c"
#import "../context.c"
#import "../string_compression/string_decompressor.c"
#import "json_compression_base.c"
#import "json_state_machine.c"

typedef struct {
  string_decompressor *strdecomp;
  acod_decoder *decoder;
  json_compression_base base;
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
}

NODISCARD bool json_decompressor_init(json_decompressor *decomp, context *context, acod_decoder *decoder, string_decompressor *strdecomp) {
  *decomp = (json_decompressor){0};

  decomp->strdecomp = strdecomp;
  decomp->decoder = decoder;

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

void json_decompressor_push_bit(json_decompressor *decomp, byte bit) {
}