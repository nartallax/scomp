#pragma once
#include "../src/json_detokenizer.c"
#include "../src/writer.c"
#include "json_tokenizer_test.c"
#include "test_utils.c"
#include <string.h>

bool test_setup_detokeniser_writer_for_failure(writer **w, int fail_after_characters) {
  if (*w) {
    writer_delete(*w);
    *w = NULL;
  }

  setup_test_context_default();
  *w = writer_new(test_context, fail_after_characters);
  if (!*w) {
    return false;
  }

  update_test_context_for_alloc_failure(0);
  return true;
}

json_token test_string_to_single_json_token(const char *str) {
  json_tokenizer t;
  json_tokenizer_init(&t, test_context);
  test_json_tokenizer_push_string(&t, str);
  if (!json_tokenizer_finalize(&t)) {
    return (json_token){0};
  }
  json_token token = *json_tokenizer_consume(&t);
  json_tokenizer_deinit(&t);
  return token;
}

const char *test_json_detokenizer_allocation_failures() {
  writer *w = NULL;

  TEST_ASSERT(test_setup_detokeniser_writer_for_failure(&w, 2));
  TEST_ASSERT(!json_detokenizer_write(w, &((json_token){.kind = JSON_TOKEN_NUMBER, .number = (json_number_token){.integer_part = 12345}})));

  const char *number_with_everything_str = "-12345.00123E-0065";
  size_t number_with_everything_strlen = strlen(number_with_everything_str);
  json_token number_with_everything = test_string_to_single_json_token(number_with_everything_str);
  for (size_t i = 1; i < number_with_everything_strlen; i++) {
    TEST_ASSERT(test_setup_detokeniser_writer_for_failure(&w, (int)i));
    TEST_ASSERT(!json_detokenizer_write(w, &number_with_everything));
  }

  json_token number_with_zero_exp = test_string_to_single_json_token("1e0");
  for (int i = 1; i <= 3; i++) {
    TEST_ASSERT(test_setup_detokeniser_writer_for_failure(&w, (int)i));
    TEST_ASSERT(!json_detokenizer_write(w, &number_with_zero_exp));
  }
  TEST_ASSERT(test_setup_detokeniser_writer_for_failure(&w, (int)4));
  TEST_ASSERT(json_detokenizer_write(w, &number_with_zero_exp));

  for (int i = 1; i <= 3; i++) {
    TEST_ASSERT(test_setup_detokeniser_writer_for_failure(&w, i));
    TEST_ASSERT(!json_detokenizer_write(w, &((json_token){.kind = JSON_TOKEN_BOM})));
  }

  for (int i = 1; i <= 4; i++) {
    TEST_ASSERT(test_setup_detokeniser_writer_for_failure(&w, i));
    TEST_ASSERT(!json_detokenizer_write(w, &((json_token){.kind = JSON_TOKEN_TRUE})));
  }

  for (int i = 1; i <= 5; i++) {
    TEST_ASSERT(test_setup_detokeniser_writer_for_failure(&w, i));
    TEST_ASSERT(!json_detokenizer_write(w, &((json_token){.kind = JSON_TOKEN_FALSE})));
  }

  for (int i = 1; i <= 4; i++) {
    TEST_ASSERT(test_setup_detokeniser_writer_for_failure(&w, i));
    TEST_ASSERT(!json_detokenizer_write(w, &((json_token){.kind = JSON_TOKEN_NULL})));
  }

  for (int i = 1; i <= 2; i++) {
    TEST_ASSERT(test_setup_detokeniser_writer_for_failure(&w, i));
    TEST_ASSERT(!json_detokenizer_write(w, &((json_token){.kind = JSON_TOKEN_ESCAPED_CHARACTER, .character = (json_character_token){.character = 'n'}})));
  }

  for (int i = 1; i <= 3; i++) {
    uint64_t utf8_bytes = (0xE2 << 0) | (0x82 << 8) | (0xAC << 16);
    TEST_ASSERT(test_setup_detokeniser_writer_for_failure(&w, i));
    TEST_ASSERT(!json_detokenizer_write(w, &((json_token){.kind = JSON_TOKEN_CHARACTER, .unicode_character = (json_unicode_token){.value = utf8_bytes, .length = 3}})));
  }

  for (int i = 1; i <= 6; i++) {
    uint64_t charcode = ('1' << 0) | ('2' << 8) | ('3' << 16) | ('4' << 14);
    TEST_ASSERT(test_setup_detokeniser_writer_for_failure(&w, i));
    TEST_ASSERT(!json_detokenizer_write(w, &((json_token){.kind = JSON_TOKEN_ESCAPED_CHARCODE, .unicode_character = (json_unicode_token){.value = charcode}})));
  }

  writer_delete(w);
  return NULL;
}