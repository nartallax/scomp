#pragma once
#include "../arithmetic_coding/encoder.c"

typedef struct {
  acod_encoder *encoder;

} json_compressor;