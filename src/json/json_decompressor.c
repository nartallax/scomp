#pragma once
#import "../arithmetic_coding/decoder.c"
#import "../arithmetic_coding/uint_reader.c"
#import "../context.c"
#import "../string_compression/string_decompressor.c"
#import "json_compression_base.c"
#import "json_tokenizer.c"

typedef struct {
  string_decompressor *strdecomp;
  acod_decoder *decoder;
  json_compression_base base;
  uint_reader_state uint_reader;
  // TODO: think of a better way to maintain state?
  // we only have tokenizer here to backfeed the tokens we just read into it
  json_tokenizer tokenizer;
} json_decompressor;

void json_decompressor_deinit(json_decompressor *decomp, context *context) {
  _json_compression_base_deinit(&decomp->base, context);
  json_tokenizer_deinit(&decomp->tokenizer, context);

  *decomp = (json_decompressor){0};
}

NODISCARD bool json_decompressor_init(json_decompressor *decomp, context *context, acod_decoder *decoder, string_decompressor *strdecomp) {
  *decomp = (json_decompressor){0};

  decomp->strdecomp = strdecomp;
  decomp->decoder = decoder;
  uint_reader_reset(&decomp->uint_reader);

  if (!_json_compression_base_init(&decomp->base, context)) {
    json_decompressor_deinit(decomp, context);
    return false;
  }

  if (!json_tokenizer_init(&decomp->tokenizer, context)) {
    json_decompressor_deinit(decomp, context);
    return false;
  }

  return true;
}
