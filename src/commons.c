#pragma once

#include <stddef.h>
#include <stdint.h>

#define NODISCARD [[nodiscard]]
// it may feel weird to use static here
// but all of our code is one big translation unit anyway
// so there's no difference between static and non-static in our case
#define INLINE static inline

typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
typedef size_t memsize;

typedef uint8_t byte;
/** "symbol", in this repo, means a minimal writeable unit on a level of entropy-compressed stream.
i.e. entropy compression algo accepts a symbol and emits bits. */
typedef int16 symbol;
/** A relative frequency of a symbol.
Usually equal to number of appearances of the symbol in the stream, but can be adjusted in some cases. */
typedef uint64 symbol_frequency;