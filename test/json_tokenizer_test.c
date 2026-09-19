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
  TEST_ASSERT(json_tokenizer_init(&t, test_context), "Init should be successful");
  TEST_ASSERT(json_tokenizer_is_empty(&t), "Empty tokenizer should return true");
  TEST_ASSERT(test_json_tokenizer_push_string(&t, "\"abcde\""), "Push should be successful");
  TEST_ASSERT(json_tokenizer_is_empty(&t) == false, "Non-empty tokenizer should return false");
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES, "Kind should be expected one");

  json_token *k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == 'a' && k->int_token.mod == 1, "Expected 1-byte character");
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == 'b' && k->int_token.mod == 1, "Expected 1-byte character");
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == 'c' && k->int_token.mod == 1, "Expected 1-byte character");
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == 'd' && k->int_token.mod == 1, "Expected 1-byte character");
  k = json_tokenizer_consume(&t);
  TEST_ASSERT(k->kind == JSON_TOKEN_CHARACTER && k->int_token.value == 'e' && k->int_token.mod == 1, "Expected 1-byte character");
  TEST_ASSERT(json_tokenizer_is_empty(&t) == false, "Non-empty tokenizer should return false");
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_QUOTES, "Kind should be expected one");
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL, "Null on empty tokenizer");
  TEST_ASSERT(json_tokenizer_is_empty(&t), "Empty tokenizer should return true");

  json_tokenizer_deinit(&t);

  return NULL;
}

const char *test_json_tokenizer_allocation_failures() {
  return NULL;
}