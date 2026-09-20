#pragma once
#include "../src/json_tokenizer.c"
#include "test_utils.c"

bool test_json_tokenizer_push_string(json_tokenizer *t, const char *str) {
  for (int i = 0; str[i] != 0; i++) {
    if (!json_tokenizer_push(t, str[i])) {
      return false;
    }
  }
  return true;
}

const char *test_json_tokenizer_string() {
  json_tokenizer t;
  TEST_ASSERT(json_tokenizer_init(&t, test_context));
  TEST_ASSERT(json_tokenizer_is_empty(&t));
  TEST_ASSERT(test_json_tokenizer_push_string(&t, "\"abcde\""));
  TEST_ASSERT(json_tokenizer_is_empty(&t) == false);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);

  json_token *k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == 'a' && k->int_token.mod == 1);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == 'b' && k->int_token.mod == 1);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == 'c' && k->int_token.mod == 1);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == 'd' && k->int_token.mod == 1);
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == 'e' && k->int_token.mod == 1);
  TEST_ASSERT(json_tokenizer_is_empty(&t) == false);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
  TEST_ASSERT(json_tokenizer_is_empty(&t));

  json_tokenizer_deinit(&t);

  return NULL;
}

const char *test_json_tokenizer_numbers() {
  json_tokenizer t;
  json_token *k;
  TEST_ASSERT(json_tokenizer_init(&t, test_context));

  // space is here to terminate the number, otherwise tokenizer will wait for more
  // wonder if it will bite me later
  TEST_ASSERT(test_json_tokenizer_push_string(&t, "12345 "));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !k->number_token.has_fraction_part && k->number_token.exponent_symbol == 0 && k->number_token.integer_part == 12345 && k->number_token.sign == 0);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_is_empty(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "-12345 "));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !k->number_token.has_fraction_part && k->number_token.exponent_symbol == 0 && k->number_token.integer_part == 12345 && k->number_token.sign == '-');
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_is_empty(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "123.456 "));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && k->number_token.has_fraction_part && k->number_token.fraction_part == 456 && k->number_token.exponent_symbol == 0 && k->number_token.integer_part == 123);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_is_empty(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "123e456 "));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !k->number_token.has_fraction_part && k->number_token.exponent_symbol == 'e' && k->number_token.exponent_part == 456 &&
              k->number_token.integer_part == 123 && k->number_token.exponent_sign == 0);
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_is_empty(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "123e-456 "));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !k->number_token.has_fraction_part && k->number_token.exponent_symbol == 'e' && k->number_token.exponent_part == 456 &&
              k->number_token.integer_part == 123 && k->number_token.exponent_sign == '-');
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_is_empty(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "123e+456 "));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !k->number_token.has_fraction_part && k->number_token.exponent_symbol == 'e' && k->number_token.exponent_part == 456 &&
              k->number_token.integer_part == 123 && k->number_token.exponent_sign == '+');
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_is_empty(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, "123E+456 "));
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !k->number_token.has_fraction_part && k->number_token.exponent_symbol == 'E' && k->number_token.exponent_part == 456 &&
              k->number_token.integer_part == 123 && k->number_token.exponent_sign == '+');
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(json_tokenizer_is_empty(&t));

  json_tokenizer_deinit(&t);
  return NULL;
}

const char *test_json_tokenizer_constants() {
  return NULL;
}

const char *test_json_tokenizer_array() {
  return NULL;
}

const char *test_json_tokenizer_object() {
  return NULL;
}

const char *test_json_tokenizer_allocation_failures() {
  return NULL;
}