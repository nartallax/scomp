#pragma once
#include "../../src/json/json_detokenizer.c"
#include "../../src/json/json_tokenizer.c"
#include "../../src/writer.c"
#include "../test_utils.c"
#include <string.h>

json_token test_string_to_single_json_token(const char *str) {
  json_tokenizer t;
  if (!json_tokenizer_init(&t, test_context)) {
    return (json_token){0};
  }
  for (int i = 0; str[i] != 0; i++) {
    if (!json_tokenizer_push(&t, str[i])) {
      return (json_token){0};
    }
  }
  if (!json_tokenizer_push(&t, ' ')) {
    return (json_token){0};
  }
  json_token token = *json_tokenizer_consume(&t);
  json_tokenizer_deinit(&t, test_context);
  return token;
}

bool test_json_detokenizer(json_token *token, const char *expected_result) {
  writer w;
  TEST_ASSERT(writer_init(&w, test_context, 1024, 4));
  json_detokenizer_write(&w, token);
  buffer result = writer_consume_all_buffers(&w).buffer;
  char *result_str = malloc(sizeof(char) * (result.length + 1));
  strncpy(result_str, (const char *)result.data, result.length);
  free(result.data);
  result_str[result.length] = 0;
  size_t i = 0;
  for (; expected_result[i] != 0; i++) {
    if (i >= result.length) {
      printf("Result shorter than expected: %s != %s\n", result_str, expected_result);
      return false;
    }
    if (result_str[i] != expected_result[i]) {
      printf("Unexpected result: %s != %s\n", result_str, expected_result);
      return false;
    }
  }
  if (i != result.length) {
    printf("Result longer than expected: %s != %s\n", result_str, expected_result);
    return false;
  }
  writer_deinit(&w, test_context);
  free(result_str);
  return true;
}

const char *test_json_detokenizer_simple() {
  TEST_ASSERT(test_json_detokenizer(&((json_token){.kind = JSON_TOKEN_NUMBER, .number = (json_number_token){.integer_part = 12345}}), "12345"));

  const char *number_with_everything_str = "-12345.00123E-0065";
  json_token number_with_everything = test_string_to_single_json_token(number_with_everything_str);
  TEST_ASSERT(test_json_detokenizer(&number_with_everything, number_with_everything_str));

  json_token number_with_zero_exp = test_string_to_single_json_token("1e0");
  TEST_ASSERT(test_json_detokenizer(&number_with_zero_exp, "1e0"));

  const char utf8_bom_zeroterminated[4] = {UTF8_BOM[0], UTF8_BOM[1], UTF8_BOM[2], 0};
  TEST_ASSERT(test_json_detokenizer(&((json_token){.kind = JSON_TOKEN_BOM}), utf8_bom_zeroterminated));
  TEST_ASSERT(test_json_detokenizer(&((json_token){.kind = JSON_TOKEN_TRUE}), "true"));
  TEST_ASSERT(test_json_detokenizer(&((json_token){.kind = JSON_TOKEN_FALSE}), "false"));
  TEST_ASSERT(test_json_detokenizer(&((json_token){.kind = JSON_TOKEN_NULL}), "null"));
  TEST_ASSERT(test_json_detokenizer(&((json_token){.kind = JSON_TOKEN_ESCAPE_SEQUENCE, .character = (json_character_token){.character = 'n'}}), "\\n"));

  uint64_t utf8_bytes = (0xE2 << 0) | (0x82 << 8) | (0xAC << 16);
  TEST_ASSERT(test_json_detokenizer(&((json_token){.kind = JSON_TOKEN_UNICODE_CHARACTER, .unicode_character = (json_unicode_token){.value = utf8_bytes, .length = 3}}), "€"));

  uint64_t charcode = ('1' << 0) | ('2' << 8) | ('3' << 16) | ('4' << 24);
  TEST_ASSERT(test_json_detokenizer(&((json_token){.kind = JSON_TOKEN_ESCAPED_UNICODE_CHARCODE, .unicode_character = (json_unicode_token){.value = charcode}}), "\\u1234"));

  return NULL;
}