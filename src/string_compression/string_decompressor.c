// #pragma once
// #include "arithmetic_coding.c"
// #include "commons.c"
// #include "writer.c"
// #include <stdbool.h>

// typedef struct {
//   writer *writer;
//   acod_decoder *decoder;
//   ftable *main_symbol_table;
//   ftable *raw_bit_with_terminator_table;
// } string_decompressor;

// bool strdecomp_init(string_decompressor *decomp) {
//  *decomp = (string_decompressor){0};
// }

// void strdecomp_deinit(string_decompressor *decomp) {
// }

// bool strdecomp_push_bit(string_decompressor *decomp, byte bit) {
//   acod_decoder_update(decomp->decoder, bit);
//   while (acod_decoder_has_symbol(decomp->decoder)) {
//   }
// }
