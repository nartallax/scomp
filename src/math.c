#pragma once
#include "commons.c"
#include <limits.h>
#include <stdint.h>
#if defined(_MSC_VER)

#include <intrin.h>

INLINE int32 most_significant_bit_index(uint64 x) {
  uint64 index;

  if (x & ((uint64)1 << 63)) {
    return 63; // _BitScanReverse64 is supposed to work on ints, not uints... for some reason
  }
  return _BitScanReverse64(&index, (int64)x) ? (int32)index : -1;
}

#elif defined(__has_builtin)
#if __has_builtin(__builtin_clzg)

INLINE int32 most_significant_bit_index(uint64 x) {
  return x == 0 ? -1 : (int32)((sizeof(x) * CHAR_BIT) - 1 - __builtin_clzg(x));
}

#endif
#endif