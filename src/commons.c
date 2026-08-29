#pragma once

#include <stdint.h>

typedef uint8_t byte;
/** "symbol", in this repo, means a minimal writeable unit on a level of entropy-compressed stream.
i.e. entropy compression algo accepts a symbol and emits bits. */
typedef uint64_t symbol;
/** A relative frequency of a symbol.
Usually equal to number of appearances of the symbol in the stream, but can be adjusted in some cases. */
typedef uint64_t symbol_frequency;