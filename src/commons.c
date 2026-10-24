#pragma once

#include <stddef.h>
#include <stdint.h>

#define NODISCARD [[nodiscard]]

// TODO: INLINE macro, and use it everywhere it needs to be used

typedef uint8_t byte;
/** "symbol", in this repo, means a minimal writeable unit on a level of entropy-compressed stream.
i.e. entropy compression algo accepts a symbol and emits bits. */
typedef int16_t symbol;
/** A relative frequency of a symbol.
Usually equal to number of appearances of the symbol in the stream, but can be adjusted in some cases. */
typedef uint64_t symbol_frequency;