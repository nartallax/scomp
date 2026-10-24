#pragma once
#include <limits.h>
#include <stdint.h>
#if defined(_MSC_VER)

#include <intrin.h>

static inline int most_significant_bit_index(uint32 x) {
  uint64 index;

  return _BitScanReverse(&index, x) ? (int)index : -1;
}

#elif defined(__has_builtin)
#if __has_builtin(__builtin_clz)

static inline int most_significant_bit_index(uint32 x) {
  return x == 0 ? -1 : (int)((sizeof(x) * CHAR_BIT) - 1 - __builtin_clz(x));
}

#endif
#endif