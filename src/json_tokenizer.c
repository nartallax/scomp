#pragma once
#include "commons.c"
#include "context.c"
#include "queue.c"

/** Types of context in which JSON tokens may appear.
Different types of contexts may contain different types of tokens
("}" cannot appear in the middle of the array etc) */
typedef enum {
  JSON_CONTEXT_ROOT = 1,
  JSON_CONTEXT_OBJECT,
  JSON_CONTEXT_ARRAY,
  JSON_CONTEXT_STRING,
} json_context_type;

typedef enum {
  // simple tokens
  JSON_TOKEN_OBJECT_START = 1, // {
  JSON_TOKEN_OBJECT_END,       // }
  JSON_TOKEN_ARRAY_START,      // [
  JSON_TOKEN_ARRAY_END,        // ]
  JSON_TOKEN_TRUE,             // true
  JSON_TOKEN_FALSE,            // false
  JSON_TOKEN_NULL,             // null
  JSON_TOKEN_COMMA,            // element separator
  JSON_TOKEN_QUOTES,           // string start or end
  JSON_TOKEN_BOM,              // byte-order mark that may appear at the start of the JSON

  // character tokens
  JSON_TOKEN_WHITESPACE,        // ' ', '\n', '\r', '\t' between elements and commas
  JSON_TOKEN_CHARACTER,         // normal, non-escaped element of a string
  JSON_TOKEN_ESCAPED_CHARACTER, // \n, \r, \\ and other single-character escape sequences

  // integer tokens
  JSON_TOKEN_CHARCODE, // \u1234
  JSON_TOKEN_INTEGER,  // 12345, -12345, +12345. only 64-bit safe integers

  // string tokens
  JSON_TOKEN_NUMBER, // 12.345, 1e10. every number that is not 64-bit safe integer
} json_token_kind;

/** Tokens that store exactly 1 character of information with them */
typedef struct {
  byte character;
} json_character_token;

/** Tokens that store 64 bits of information with them */
typedef struct {
  uint64_t value;
  byte sign; // '+', '-' or 0
} json_integer_token;

/** Tokens that contain raw unparsed string with them that must be stored as such */
typedef struct {
  byte *value;
} json_unparsed_token;

/** Any of the tokens described above. */
typedef struct {
  json_token_kind kind;

  union {
    json_character_token char_token;
    json_integer_token int_token;
    json_unparsed_token raw_token;
  };
} json_token;

/** Json tokenizer is a state machine that converts bytes of encoded JSON into tokens that describe contents of the JSON */
typedef struct {
  context *context;
  /** Parsed tokens ready for consumption */
  queue *tokens;
  /** Unparsed characters */
  byte characters[16];
  /** Next free index in `characters` field */
  size_t char_pointer;
} json_tokenizer;

// json_tokenizer json_tokenizer_new(context *context) {
// }
