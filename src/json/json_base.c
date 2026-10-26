#pragma once
#include "../commons.c"

/** Max length of state stack. Limits nesting.
Exists to prevent memory overflow in case when the input is infinite number of '[' */
constexpr int64 JSON_MAX_STATE_STACK_LENGTH = 512;

/** Max amount of bytes that may be assembled into one token.

It's that many mostly for various numbers;
non-number tokens will be 6 bytes at most (\u1234 being the second longest) */
constexpr int64 JSON_MAX_SINGLE_TOKEN_LENGTH = 128;

/** Max amount of leading zeroes a JSON number can have in its fractional part, or in its exponent.
It's possible to have in theory many more leading zeroes, especially in fractions, but it's just about never happens in real life. */
constexpr int64 JSON_MAX_LEADING_ZEROES = 32;

// TODO: is it faster to hardcode it everywhere?
constexpr byte _JTOK_TRUE_BYTES[] = {'t', 'r', 'u', 'e'};
constexpr byte _JTOK_FALSE_BYTES[] = {'f', 'a', 'l', 's', 'e'};
constexpr byte _JTOK_NULL_BYTES[] = {'n', 'u', 'l', 'l'};
constexpr byte _JTOK_ESCAPE_SEQ_CHARS[] = {'\\', '"', '/', 'b', 'f', 'n', 'r', 't'};
constexpr byte _JTOK_WHITESPACE_CHARS[] = {' ', '\n', '\r', '\t'};

typedef byte json_token_kind;
// simple tokens
constexpr json_token_kind JSON_TOKEN_OBJECT_OPEN = 1;      // {
constexpr json_token_kind JSON_TOKEN_OBJECT_CLOSE = 2;     // }
constexpr json_token_kind JSON_TOKEN_ARRAY_OPEN = 3;       // [
constexpr json_token_kind JSON_TOKEN_ARRAY_CLOSE = 4;      // ]
constexpr json_token_kind JSON_TOKEN_TRUE = 5;             // true
constexpr json_token_kind JSON_TOKEN_FALSE = 6;            // false
constexpr json_token_kind JSON_TOKEN_NULL = 7;             // null
constexpr json_token_kind JSON_TOKEN_VALUE_SEPARATOR = 8;  // ',' - element separator
constexpr json_token_kind JSON_TOKEN_STRING_DELIMITER = 9; // '"' - string start or end
constexpr json_token_kind JSON_TOKEN_KV_SEPARATOR = 10;    // ':' - separator between key and value
constexpr json_token_kind JSON_TOKEN_BOM = 11;             // byte-order mark that may appear at the start of the JSON
// character tokens
constexpr json_token_kind JSON_TOKEN_WHITESPACE = 12; // ' ', '\n', '\r', '\t' between elements and commas
// TODO: get rid of this token type
constexpr json_token_kind JSON_TOKEN_ESCAPE_SEQUENCE = 13; // \n, \r, \\ and other single-character escape sequences within a string
// unicode tokens
constexpr json_token_kind JSON_TOKEN_UNICODE_CHARACTER =
    14; // normal, non-escaped element of a string. one unicode codepoint; `value` contains sequence of utf-8 bytes merged into one uint64, not decoded codepoint
        // TODO: get rid of this token type
constexpr json_token_kind JSON_TOKEN_ESCAPED_UNICODE_CHARCODE = 15; // \u1234
// number tokens
constexpr json_token_kind JSON_TOKEN_NUMBER = 16; // 12.345, 1e10. every number that is not 64-bit safe integer
constexpr json_token_kind JSON_TOKEN_MAX = JSON_TOKEN_NUMBER;

typedef byte json_state;
constexpr json_state JSON_STATE_ROOT = 0;                // if this state is ever reached - it means JSON reading is completed
constexpr json_state JSON_STATE_START = 1;               // expecting BOM or value
constexpr json_state JSON_STATE_VALUE = 2;               // expecting some sort of value
constexpr json_state JSON_STATE_OBJECT_START = 3;        // expecting key or closing token
constexpr json_state JSON_STATE_OBJECT_CONTINUE = 4;     // expecting comma or closing token
constexpr json_state JSON_STATE_OBJECT_KV_SEPARATOR = 5; // expecting ":"
constexpr json_state JSON_STATE_OBJECT_KEY_START = 6;    // expecting key
constexpr json_state JSON_STATE_ARRAY_START = 7;         // expecting value or closing token
constexpr json_state JSON_STATE_ARRAY_CONTINUE = 8;      // expecting comma or closing token
constexpr json_state JSON_STATE_STRING = 9;              // expecting characters
constexpr json_state JSON_STATE_OBJECT_KEY = 10;         // exactly like string = 0; but hints about further state changes
constexpr json_state JSON_STATE_ESCAPED_CHARACTER = 11;  // expecting n = 0; r = 0; or other escapeable character
constexpr json_state JSON_STATE_ESCAPED_CHARCODE = 12;   // expecting four hexadecimals
constexpr json_state JSON_STATE_UNICODE_CHARACTER = 13;  // expecting unicode continuation
constexpr json_state JSON_STATE_NUMBER = 14;             // expecting numeric components
constexpr json_state JSON_STATE_TRUE = 15;               // expecting letters of `true`
constexpr json_state JSON_STATE_FALSE = 16;              // expecting letters of `false`
constexpr json_state JSON_STATE_NULL = 17;               // expecting letters of `null`
constexpr json_state JSON_STATE_BOM = 18;                // expecting bytes of utf-8 byte order mark
constexpr json_state JSON_STATE_MAX = JSON_STATE_BOM;

/** Tokens that store exactly 1 character of information with them */
typedef struct {
  byte character;
} json_character_token;

/** Tokens that are describing a unicode character, or a part of it, depeding on kind */
typedef struct {
  uint64 value;
  byte length;
} json_unicode_token;

/** Various booleans about JSON numbers packed into a bitmap */
typedef byte json_number_flags;
constexpr json_number_flags JSON_NUMBER_INTEGER_PART_IS_ZERO = 1 << 0; // 0, -0
constexpr json_number_flags JSON_NUMBER_IS_NEGATIVE = 1 << 1;          // -1
constexpr json_number_flags JSON_NUMBER_HAS_FRACTION = 1 << 2;         // 1.1
constexpr json_number_flags JSON_NUMBER_HAS_EXPONENT = 1 << 3;         // 1e1
constexpr json_number_flags JSON_NUMBER_EXPONENT_UPPERCASE = 1 << 4;   // 1E1
constexpr json_number_flags JSON_NUMBER_EXPONENT_IS_NEGATIVE = 1 << 5; // 1e-1
constexpr json_number_flags JSON_NUMBER_EXPONENT_HAS_PLUS = 1 << 6;    // 1e+1
constexpr json_number_flags JSON_NUMBER_FLAGS_MAX = (1 << 7) - 1;

/** Max leading zeroes there can be in exponent or fraction part of a number.
Any more - and the number is considered invalid.

Reason to have this value is to keep string representation of a number low; this helps with re-stringification in case of broken JSON */
constexpr int32 JSON_MAX_NUMBER_LEADING_ZEROES = 80;

/** Tokens that contains a number in JSON sense. */
typedef struct {
  uint64 integer_part;
  uint64 fraction_part;
  uint16 exponent_part; // exponents over 65535 are way too large for anything practical anyway
  json_number_flags flags;
  byte fraction_leading_zeroes;
  byte exponent_leading_zeroes;
} json_number_token;

/** Any of the tokens described above. */
typedef struct {
  union {
    json_character_token character;
    json_unicode_token unicode_character;
    json_number_token number;
  };
  json_token_kind kind;
} json_token;