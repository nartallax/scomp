#pragma once
#include "../src/json_tokenizer.c"
#include "test_utils.c"
#include <stdio.h>

bool test_json_tokenizer_push_string(json_tokenizer *t, const char *str) {
  for (int i = 0; str[i] != 0; i++) {
    if (!json_tokenizer_push(t, str[i])) {
      return false;
    }
  }
  return true;
}

bool test_json_tokenizer_reinit_nonempty(json_tokenizer *t) {
  if (json_tokenizer_is_empty(t)) {
    return false;
  }
  json_tokenizer_deinit(t);
  return json_tokenizer_init(t, test_context);
}

bool test_json_tokenizer_reinit(json_tokenizer *t) {
  if (!json_tokenizer_is_empty(t)) {
    return false;
  }
  json_tokenizer_deinit(t);
  return json_tokenizer_init(t, test_context);
}

const char *test_json_tokenizer_string() {
  json_tokenizer t;
  json_token *k;
  TEST_ASSERT(json_tokenizer_init(&t, test_context));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "\"abcde\""));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == 'a' && k->int_token.mod == 1);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == 'b' && k->int_token.mod == 1);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == 'c' && k->int_token.mod == 1);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == 'd' && k->int_token.mod == 1);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == 'e' && k->int_token.mod == 1);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  // correct escaped codepoint
  TEST_ASSERT(test_json_tokenizer_push_string(&t, "\"\\u12aB\""));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  k = json_tokenizer_consume(&t);
  uint64_t merged_code = 0;
  merged_code = (merged_code << 8) | 'B';
  merged_code = (merged_code << 8) | 'a';
  merged_code = (merged_code << 8) | '2';
  merged_code = (merged_code << 8) | '1';
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPED_CHARCODE && k->int_token.value == merged_code);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  // incorrect escaped codepoint
  TEST_ASSERT(test_json_tokenizer_push_string(&t, "\"\\uGg//\""));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  // incorrect escaped codepoints in all four positions
  const char *bad_escaped_codepoints[4] = {"\"\\uxfff", "\"\\ufxff", "\"\\uffxf", "\"\\ufffx"};
  for (size_t i = 0; i < 4; i++) {
    TEST_ASSERT(test_json_tokenizer_push_string(&t, bad_escaped_codepoints[i]));
    TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
    TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
    TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));
  }

  // other escapings
  TEST_ASSERT(test_json_tokenizer_push_string(&t, "\"\\r\\\\\\n\\t\\\"\\/\\b\\f\""));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPED_CHARACTER && k->char_token.character == 'r');
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPED_CHARACTER && k->char_token.character == '\\');
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPED_CHARACTER && k->char_token.character == 'n');
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPED_CHARACTER && k->char_token.character == 't');
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPED_CHARACTER && k->char_token.character == '"');
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPED_CHARACTER && k->char_token.character == '/');
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPED_CHARACTER && k->char_token.character == 'b');
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPED_CHARACTER && k->char_token.character == 'f');
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  // correct unicode
  TEST_ASSERT(test_json_tokenizer_push_string(&t, "\"Привет, мир!\""));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == (0xD0 | (0x9F << 8)));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == (0xD1 | (0x80 << 8)));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == (0xD0 | (0xB8 << 8)));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == (0xD0 | (0xB2 << 8)));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == (0xD0 | (0xB5 << 8)));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == (0xD1 | (0x82 << 8)));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == ',');
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == ' ');
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == (0xD0 | (0xBC << 8)));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == (0xD0 | (0xB8 << 8)));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == (0xD1 | (0x80 << 8)));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == '!');
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  // unicode with broken continuation bytes
  const byte broken_continuation_bytes_unicode_pair[6] = {0xD0, 0xFF, 0xD0, 0xFF, 0xD0, 0xFF};
  char *broken_continuation_bytes_unicode_str = malloc(64);
  TEST_ASSERT(sprintf(broken_continuation_bytes_unicode_str, "\"%.*s\"", 6, broken_continuation_bytes_unicode_pair) < 64);
  TEST_ASSERT(test_json_tokenizer_push_string(&t, broken_continuation_bytes_unicode_str));
  free(broken_continuation_bytes_unicode_str);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  // unicode with broken continuation bytes in escape sequence
  broken_continuation_bytes_unicode_str = malloc(64);
  TEST_ASSERT(sprintf(broken_continuation_bytes_unicode_str, "\"\\%.*s\"", 6, broken_continuation_bytes_unicode_pair) < 64);
  TEST_ASSERT(test_json_tokenizer_push_string(&t, broken_continuation_bytes_unicode_str));
  free(broken_continuation_bytes_unicode_str);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  json_tokenizer_deinit(&t);
  return NULL;
}

const char *test_json_tokenizer_numbers() {
  json_tokenizer t;
  json_token *k;
  TEST_ASSERT(json_tokenizer_init(&t, test_context));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "12345"));
  TEST_ASSERT(json_tokenizer_finalize(&t));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !k->number_token.has_fraction_part && k->number_token.exponent_symbol == 0 && k->number_token.integer_part == 12345 && k->number_token.sign == 0);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "-12345"));
  TEST_ASSERT(json_tokenizer_finalize(&t));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !k->number_token.has_fraction_part && k->number_token.exponent_symbol == 0 && k->number_token.integer_part == 12345 && k->number_token.sign == '-');
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "123.456"));
  TEST_ASSERT(json_tokenizer_finalize(&t));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && k->number_token.has_fraction_part && k->number_token.fraction_part == 456 && k->number_token.exponent_symbol == 0 && k->number_token.integer_part == 123);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "123e456"));
  TEST_ASSERT(json_tokenizer_finalize(&t));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !k->number_token.has_fraction_part && k->number_token.exponent_symbol == 'e' && k->number_token.exponent_part == 456 &&
              k->number_token.integer_part == 123 && k->number_token.exponent_sign == 0);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "123e-456"));
  TEST_ASSERT(json_tokenizer_finalize(&t));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !k->number_token.has_fraction_part && k->number_token.exponent_symbol == 'e' && k->number_token.exponent_part == 456 &&
              k->number_token.integer_part == 123 && k->number_token.exponent_sign == '-');
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "123e+456"));
  TEST_ASSERT(json_tokenizer_finalize(&t));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !k->number_token.has_fraction_part && k->number_token.exponent_symbol == 'e' && k->number_token.exponent_part == 456 &&
              k->number_token.integer_part == 123 && k->number_token.exponent_sign == '+');
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "123E+456"));
  TEST_ASSERT(json_tokenizer_finalize(&t));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !k->number_token.has_fraction_part && k->number_token.exponent_symbol == 'E' && k->number_token.exponent_part == 456 &&
              k->number_token.integer_part == 123 && k->number_token.exponent_sign == '+');
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "123.456e+678"));
  TEST_ASSERT(json_tokenizer_finalize(&t));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && k->number_token.has_fraction_part && k->number_token.fraction_part == 456 && k->number_token.exponent_symbol == 'e' &&
              k->number_token.exponent_part == 678 && k->number_token.exponent_sign == '+' && k->number_token.integer_part == 123);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "123.456E+678"));
  TEST_ASSERT(json_tokenizer_finalize(&t));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && k->number_token.has_fraction_part && k->number_token.fraction_part == 456 && k->number_token.exponent_symbol == 'E' &&
              k->number_token.exponent_part == 678 && k->number_token.exponent_sign == '+' && k->number_token.integer_part == 123);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  // overflows
  TEST_ASSERT(test_json_tokenizer_push_string(&t, "1844674407370955200000"));
  TEST_ASSERT(json_tokenizer_finalize(&t));
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "1e1844674407370955200000"));
  TEST_ASSERT(json_tokenizer_finalize(&t));
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "1.1844674407370955200000"));
  TEST_ASSERT(json_tokenizer_finalize(&t));
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  char *too_long_number = string_repeat("1", 1, JTOK_MAX_CHARS_LENGTH);
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, too_long_number));
  free(too_long_number);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  // invalid number-like values
  const char *wrong_numbers[] = {"1.", ".1", "-.1", "e1", "-E1", "1.e5", "--", "1-", "1.-", "1e1+", "1ee", NULL};
  for (int i = 0;; i++) {
    const char *str = wrong_numbers[i];
    if (!str) {
      break;
    }
    TEST_ASSERT(test_json_tokenizer_push_string(&t, str));
    TEST_ASSERT(json_tokenizer_finalize(&t));
    TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
    TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));
  }

  json_tokenizer_deinit(&t);
  return NULL;
}

const char *test_json_tokenizer_constants() {
  json_tokenizer t;
  TEST_ASSERT(json_tokenizer_init(&t, test_context));

  const char true_str[5] = "true";
  for (size_t i = 0; i < sizeof(true_str) - 1; i++) {
    for (size_t j = 0; j < sizeof(true_str) - 1; j++) {
      TEST_ASSERT(json_tokenizer_push(&t, j <= i ? true_str[j] : '0'));
    }
    if (i == sizeof(true_str) - 2) {
      TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_TRUE);
      TEST_ASSERT(test_json_tokenizer_reinit(&t));
    } else {
      TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
      TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));
    }
  }

  const char false_str[6] = "false";
  for (size_t i = 0; i < sizeof(false_str) - 1; i++) {
    for (size_t j = 0; j < sizeof(false_str) - 1; j++) {
      TEST_ASSERT(json_tokenizer_push(&t, j <= i ? false_str[j] : '0'));
    }
    if (i == sizeof(false_str) - 2) {
      TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_FALSE);
      TEST_ASSERT(test_json_tokenizer_reinit(&t));
    } else {
      TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
      TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));
    }
  }

  const char null_str[5] = "null";
  for (size_t i = 0; i < sizeof(null_str) - 1; i++) {
    for (size_t j = 0; j < sizeof(null_str) - 1; j++) {
      TEST_ASSERT(json_tokenizer_push(&t, j <= i ? null_str[j] : '0'));
    }
    if (i == sizeof(null_str) - 2) {
      TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_NULL);
      TEST_ASSERT(test_json_tokenizer_reinit(&t));
    } else {
      TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
      TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));
    }
  }

  json_tokenizer_deinit(&t);
  return NULL;
}

const char *test_json_tokenizer_array() {
  json_tokenizer t;
  json_token *k;
  TEST_ASSERT(json_tokenizer_init(&t, test_context));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "[1 , 2,3,4]"));
  TEST_ASSERT(json_tokenizer_is_empty(&t) == false);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_OPEN);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && k->number_token.integer_part == 1);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_COMMA);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && k->number_token.integer_part == 2);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_COMMA);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && k->number_token.integer_part == 3);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_COMMA);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && k->number_token.integer_part == 4);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "[]"));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "[\r \t\n]"));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "[true, false, null]"));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_TRUE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_COMMA);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_FALSE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_COMMA);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_NULL);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "[,]"));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "[1,,]"));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_NUMBER);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_COMMA);
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "[1 1]"));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_NUMBER);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  json_tokenizer_deinit(&t);
  return NULL;
}

const char *test_json_tokenizer_object() {
  json_tokenizer t;
  TEST_ASSERT(json_tokenizer_init(&t, test_context));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "{}"));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "{\r \t\n}"));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "{\"width\":15, \"is_good\":true}"));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(json_tokenizer_consume(&t)->int_token.value == 'w');
  TEST_ASSERT(json_tokenizer_consume(&t)->int_token.value == 'i');
  TEST_ASSERT(json_tokenizer_consume(&t)->int_token.value == 'd');
  TEST_ASSERT(json_tokenizer_consume(&t)->int_token.value == 't');
  TEST_ASSERT(json_tokenizer_consume(&t)->int_token.value == 'h');
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_COLON);
  TEST_ASSERT(json_tokenizer_consume(&t)->number_token.integer_part == 15);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_COMMA);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(json_tokenizer_consume(&t)->int_token.value == 'i');
  TEST_ASSERT(json_tokenizer_consume(&t)->int_token.value == 's');
  TEST_ASSERT(json_tokenizer_consume(&t)->int_token.value == '_');
  TEST_ASSERT(json_tokenizer_consume(&t)->int_token.value == 'g');
  TEST_ASSERT(json_tokenizer_consume(&t)->int_token.value == 'o');
  TEST_ASSERT(json_tokenizer_consume(&t)->int_token.value == 'o');
  TEST_ASSERT(json_tokenizer_consume(&t)->int_token.value == 'd');
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_COLON);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_TRUE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "{uwu"));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  json_tokenizer_deinit(&t);
  return NULL;
}

const char *test_json_tokenizer_whitespaces() {
  json_tokenizer t;
  json_token *k;
  TEST_ASSERT(json_tokenizer_init(&t, test_context));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "[1, \n\r\t2]"));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_NUMBER);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_COMMA);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_WHITESPACE && k->char_token.character == ' ');
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_WHITESPACE && k->char_token.character == '\n');
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_WHITESPACE && k->char_token.character == '\r');
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_WHITESPACE && k->char_token.character == '\t');
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_NUMBER);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "[owo]"));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  json_tokenizer_deinit(&t);
  return NULL;
}

const char *test_json_tokenizer_bom() {
  json_tokenizer t;
  TEST_ASSERT(json_tokenizer_init(&t, test_context));
  TEST_ASSERT(json_tokenizer_push(&t, utf8_bom[0]));
  TEST_ASSERT(json_tokenizer_push(&t, utf8_bom[1]));
  TEST_ASSERT(json_tokenizer_push(&t, utf8_bom[2]));
  TEST_ASSERT(json_tokenizer_push(&t, '{'));
  TEST_ASSERT(json_tokenizer_push(&t, '}'));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_BOM);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(json_tokenizer_push(&t, utf8_bom[0]));
  TEST_ASSERT(json_tokenizer_push(&t, utf8_bom[1]));
  TEST_ASSERT(json_tokenizer_push(&t, 0x0));
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL);

  json_tokenizer_deinit(&t);
  return NULL;
}

const char *test_json_tokenizer_nesting() {
  json_tokenizer t;
  TEST_ASSERT(json_tokenizer_init(&t, test_context));
  // this also tests for whitespaces everywhere
  TEST_ASSERT(test_json_tokenizer_push_string(&t, "[ { \"a\" : { \"b\" : [ [ { \"c\" : null } ] ] } } ]"));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_CHARACTER);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_COLON);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_CHARACTER);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_COLON);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_CHARACTER);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_COLON);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_NULL);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  json_tokenizer_deinit(&t);
  return NULL;
}

const char *test_json_tokenizer_emptiness() {
  json_tokenizer t;
  TEST_ASSERT(json_tokenizer_init(&t, test_context));
  TEST_ASSERT(json_tokenizer_is_empty(&t));
  TEST_ASSERT(test_json_tokenizer_push_string(&t, "["));
  TEST_ASSERT(!json_tokenizer_is_empty(&t));
  TEST_ASSERT(test_json_tokenizer_push_string(&t, "]"));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(json_tokenizer_is_empty(&t));
  TEST_ASSERT(test_json_tokenizer_push_string(&t, "123"));
  TEST_ASSERT(!json_tokenizer_is_empty(&t));

  json_tokenizer_deinit(&t);
  return NULL;
}

bool test_json_tokenizer_setup_for_queue_failure(json_tokenizer *t, size_t offset, int allocations_before_failure) {
  setup_test_context_default();
  if (!json_tokenizer_init(t, test_context)) {
    return false;
  }
  char *lots_of_array_open = string_repeat("[", 1, QUEUE_DEFAULT_LENGTH - offset);
  if (!test_json_tokenizer_push_string(t, lots_of_array_open)) {
    return false;
  }
  free(lots_of_array_open);
  update_test_context_for_alloc_failure(allocations_before_failure);
  return true;
}

const char *test_json_tokenizer_allocation_failures() {
  json_tokenizer t;
  char *long_string_with_repeats;
  setup_test_context(0);
  TEST_ASSERT(!json_tokenizer_init(&t, test_context));

  // printf("queue size/length = %zu/%zu; stack size/length = %zu/%zu\n", queue_get_count(&t.token_queue), t.token_queue.length, t.state_stack.count, t.state_stack.length);

  setup_test_context(1);
  TEST_ASSERT(!json_tokenizer_init(&t, test_context));

  // overflow on context push
  setup_test_context_default();
  TEST_ASSERT(json_tokenizer_init(&t, test_context));
  long_string_with_repeats = string_repeat("[", 1, STACK_DEFAULT_LENGTH - 1);
  TEST_ASSERT(test_json_tokenizer_push_string(&t, long_string_with_repeats));
  free(long_string_with_repeats);
  update_test_context_for_alloc_failure(0);
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, "["));
  json_tokenizer_deinit(&t);

  // overflow on token queue push
  TEST_ASSERT(test_json_tokenizer_setup_for_queue_failure(&t, 1, 1));
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, "["));
  json_tokenizer_deinit(&t);

  TEST_ASSERT(test_json_tokenizer_setup_for_queue_failure(&t, 1, 1));
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, "123 "));
  json_tokenizer_deinit(&t);

  TEST_ASSERT(test_json_tokenizer_setup_for_queue_failure(&t, 2, 0));
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, "\"a\""));
  json_tokenizer_deinit(&t);

  TEST_ASSERT(test_json_tokenizer_setup_for_queue_failure(&t, 1, 0));
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, " "));
  json_tokenizer_deinit(&t);

  TEST_ASSERT(test_json_tokenizer_setup_for_queue_failure(&t, 5, 0));
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, "{\"a\":"));
  json_tokenizer_deinit(&t);

  TEST_ASSERT(test_json_tokenizer_setup_for_queue_failure(&t, 2, 0));
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, "null,"));
  json_tokenizer_deinit(&t);

  // some bullshit for code coverage
  // it's not normally possible for code to attempt memory allocation when in root state
  // because there's guaranteed to be some free slots in the state stack
  // but I also don't feel comfortable to not check something that must always be checked
  setup_test_context_default();
  TEST_ASSERT(json_tokenizer_init(&t, test_context));
  while (stack_get_count(&t.state_stack) < t.state_stack.length - 1) {
    json_state_type *state_slot = stack_push(&t.state_stack);
    *state_slot = JSON_STATE_ROOT;
  }
  update_test_context_for_alloc_failure(0);
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, "["));
  json_tokenizer_deinit(&t);

  return NULL;
}