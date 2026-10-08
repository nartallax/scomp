#pragma once
#import "../commons.c"

// TODO: test if nesting below the limit actually works
/** Max length of state stack. Limits nesting.
Exists to prevent memory overflow in case when the input is infinite number of '[' */
constexpr size_t JSON_MAX_STATE_STACK_LENGTH__new = 512;

/** Max amount of bytes that may be assembled into one token.

It's that many mostly for various numbers;
non-number tokens will be 6 bytes at most (\u1234 being the second longest) */
constexpr size_t JSON_MAX_SINGLE_TOKEN_LENGTH__new = 128;

/** Max amount of leading zeroes a JSON number can have in its fractional part, or in its exponent.
It's possible to have in theory many more leading zeroes, especially in fractions, but it's just about never happens in real life. */
constexpr size_t JSON_MAX_LEADING_ZEROES__new = 32;

constexpr byte _JTOK_TRUE_BYTES[] = {'t', 'r', 'u', 'e'};
constexpr byte _JTOK_FALSE_BYTES[] = {'f', 'a', 'l', 's', 'e'};
constexpr byte _JTOK_NULL_BYTES[] = {'n', 'u', 'l', 'l'};

// TODO: consider making this single-byte long and check the performance
// this may require moving around parts of some structures, to jiggle alignment rules a bit
typedef enum {
  // simple tokens
  JSON_TOKEN_OBJECT_OPEN = 1, // {
  JSON_TOKEN_OBJECT_CLOSE,    // }
  JSON_TOKEN_ARRAY_OPEN,      // [
  JSON_TOKEN_ARRAY_CLOSE,     // ]
  JSON_TOKEN_TRUE,            // true
  JSON_TOKEN_FALSE,           // false
  JSON_TOKEN_NULL,            // null
  JSON_TOKEN_COMMA,           // element separator // TODO: rename to value_separator
  JSON_TOKEN_QUOTES,          // string start or end // TODO: rename to string_delimiter
  JSON_TOKEN_COLON,           // : separator between key and value // TODO: rename to key_value_separator
  JSON_TOKEN_BOM,             // byte-order mark that may appear at the start of the JSON

  // character tokens
  JSON_TOKEN_WHITESPACE,        // ' ', '\n', '\r', '\t' between elements and commas
  JSON_TOKEN_ESCAPED_CHARACTER, // \n, \r, \\ and other single-character escape sequences within a string

  // unicode tokens
  JSON_TOKEN_CHARACTER,        // normal, non-escaped element of a string. one unicode codepoint; `value` contains sequence of utf-8 bytes merged into one uint64, not decoded codepoint
  JSON_TOKEN_ESCAPED_CHARCODE, // \u1234

  // number tokens
  JSON_TOKEN_NUMBER, // 12.345, 1e10. every number that is not 64-bit safe integer
} json_token_kind__new;

typedef enum {
  JSON_STATE_ROOT = 0,            // if this state is ever reached - it means JSON reading is completed
  JSON_STATE_START,               // expecting BOM or value
  JSON_STATE_VALUE,               // expecting some sort of value
  JSON_STATE_OBJECT_START,        // expecting key or closing token
  JSON_STATE_OBJECT_CONTINUE,     // expecting comma or closing token
  JSON_STATE_OBJECT_KV_SEPARATOR, // expecting ":"
  JSON_STATE_OBJECT_KEY_START,    // expecting key
  JSON_STATE_ARRAY_START,         // expecting value or closing token
  JSON_STATE_ARRAY_CONTINUE,      // expecting comma or closing token
  JSON_STATE_STRING,              // expecting characters
  JSON_STATE_OBJECT_KEY,          // exactly like string, but hints about further state changes
  JSON_STATE_ESCAPED_CHARACTER,   // expecting n, r, or other escapeable character
  JSON_STATE_ESCAPED_CHARCODE,    // expecting four hexadecimals
  JSON_STATE_UNICODE_CHARACTER,   // expecting unicode continuation
  JSON_STATE_NUMBER,              // expecting numeric components
  JSON_STATE_TRUE,                // expecting letters of `true`
  JSON_STATE_FALSE,               // expecting letters of `false`
  JSON_STATE_NULL,                // expecting letters of `null`
  JSON_STATE_BOM                  // expecting bytes of utf-8 byte order mark
} json_state__new;

/** Tokens that store exactly 1 character of information with them */
typedef struct {
  byte character;
} json_character_token__new;

/** Tokens that are describing a unicode character, or a part of it, depeding on kind */
typedef struct {
  uint64_t value;
  byte length;
} json_unicode_token__new;

// TODO: make it be byte-long
/** Various booleans about JSON numbers packed into a bitmap */
typedef enum {
  JSON_NUMBER_HAS_FRACTION = 1 << 0,         // 1.1
  JSON_NUMBER_HAS_EXPONENT = 1 << 1,         // 1e1
  JSON_NUMBER_EXPONENT_UPPERCASE = 1 << 2,   // 1E1
  JSON_NUMBER_EXPONENT_IS_NEGATIVE = 1 << 3, // 1e-1
  JSON_NUMBER_EXPONENT_HAS_PLUS = 1 << 4,    // 1e+1
  JSON_NUMBER_IS_NEGATIVE = 1 << 5,          // -1
} json_number_flags__new;

/** Tokens that contains a number in JSON sense. */
typedef struct {
  uint64_t integer_part;
  uint64_t fraction_part;
  uint16_t exponent_part; // exponents over 65535 are way too large for anything practical anyway
  json_number_flags__new flags;
  byte fraction_leading_zeroes;
  byte exponent_leading_zeroes;
} json_number_token__new;

/** Any of the tokens described above. */
typedef struct {
  json_token_kind__new kind;

  union {
    json_character_token__new character;
    json_unicode_token__new unicode_character;
    json_number_token__new number;
  };
} json_token__new;