#pragma once
#include "arithmetic_coding.c"
#include "commons.c"

typedef struct {
  acod_encoder *encoder;
  ftable *table;
} string_compressor;