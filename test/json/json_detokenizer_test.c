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

bool test_json_detokenizer_with_token(json_token token, const char *expected_result) {
  writer w;
  int64_t expected_result_length = strlen(expected_result);
  TEST_ASSERT(writer_init(&w, test_context, 1024, 4));
  int64_t written_bytes_count = json_detokenizer_write(&w, &token);
  buffer result = writer_consume_all_buffers(&w).buffer;
  char *result_str = malloc(sizeof(char) * (result.length + 1));
  strncpy(result_str, (const char *)result.data, result.length);
  free(result.data);
  result_str[result.length] = 0;
  int64_t i = 0;
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
  if (written_bytes_count != expected_result_length) {
    printf("Wrote %zu bytes, but expected %zu\n", written_bytes_count, expected_result_length);
    return false;
  }
  writer_deinit(&w, test_context);
  free(result_str);
  return true;
}

bool test_json_detokenizer_with_str(const char *src) {
  json_token token = test_string_to_single_json_token(src);
  return test_json_detokenizer_with_token(token, src);
}
const char *test_json_detokenizer_simple() {
  TEST_ASSERT(test_json_detokenizer_with_str("12345"));
  TEST_ASSERT(test_json_detokenizer_with_str("-12345.00123E-0065"));
  TEST_ASSERT(test_json_detokenizer_with_str("1e0"));
  const char utf8_bom_zeroterminated[4] = {UTF8_BOM[0], UTF8_BOM[1], UTF8_BOM[2], 0};
  TEST_ASSERT(test_json_detokenizer_with_str(utf8_bom_zeroterminated));
  TEST_ASSERT(test_json_detokenizer_with_str("true"));
  TEST_ASSERT(test_json_detokenizer_with_str("false"));
  TEST_ASSERT(test_json_detokenizer_with_str("null"));

  TEST_ASSERT(test_json_detokenizer_with_token(((json_token){.kind = JSON_TOKEN_ESCAPE_SEQUENCE, .character = (json_character_token){.character = 'n'}}), "\\n"));

  uint64_t utf8_bytes = (0xE2 << 0) | (0x82 << 8) | (0xAC << 16);
  TEST_ASSERT(test_json_detokenizer_with_token(((json_token){.kind = JSON_TOKEN_UNICODE_CHARACTER, .unicode_character = (json_unicode_token){.value = utf8_bytes, .length = 3}}), "€"));

  uint64_t charcode = ('1' << 0) | ('2' << 8) | ('3' << 16) | ('4' << 24);
  TEST_ASSERT(test_json_detokenizer_with_token(((json_token){.kind = JSON_TOKEN_ESCAPED_UNICODE_CHARCODE, .unicode_character = (json_unicode_token){.value = charcode}}), "\\u1234"));

  return NULL;
}