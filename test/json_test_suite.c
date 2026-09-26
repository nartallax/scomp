#pragma once
#include "../src/json_detokenizer.c"
#include "../src/json_tokenizer.c"
#include "../src/writer.c"
#include "test_utils.c"

bool json_test_suite_has_errors = false;
int json_test_suite_successes = 0;
int json_test_suite_failures = 0;

const int json_test_suite_expected_files = 318;

typedef enum {
  JSON_SUITE_FAILURE = 1,
  JSON_SUITE_SUCCESS = 2,
  JSON_SUITE_IMPLEMENTATION_DEPENDENT = 3
} json_suite_expected_result;

bool test_parse_json_suite_path(const char *path, json_suite_expected_result *expected_result_slot, const char **filename_slot) {
  if (!ends_with(path, ".json")) {
    return false; // LICENSE or smth
  }

  *filename_slot = get_filename(path);
  if (filename_slot == NULL) {
    return false;
  }

  if (starts_with(*filename_slot, "y_")) {
    *expected_result_slot = JSON_SUITE_SUCCESS;
  } else if (starts_with(*filename_slot, "n_")) {
    *expected_result_slot = JSON_SUITE_FAILURE;
  } else if (starts_with(*filename_slot, "i_")) {
    *expected_result_slot = JSON_SUITE_IMPLEMENTATION_DEPENDENT;
  } else {
    return false;
  }

  return true;
}

buffer test_feed_file_into_tokenizer_detokenizer(const char *path, const char *filename) {
  writer *w = writer_new(test_context, 1024);
  json_tokenizer t;
  json_tokenizer_init(&t, test_context);
  json_token *token;

  FILE *src_file = fopen(path, "r");
  if (src_file == NULL) {
    printf("Failed to open %s for reading.\n", filename);
    json_test_suite_has_errors = true;
    return EMPTY_BUFFER;
  }

  bool is_success = true;
  size_t bytes_read;
  char read_buffer[4096];
  while (is_success && (bytes_read = fread(read_buffer, 1, sizeof(read_buffer), src_file)) > 0) {
    for (size_t i = 0; i < bytes_read; i++) {
      if (!json_tokenizer_push(&t, read_buffer[i])) {
        is_success = false;
        break;
      }
    }

    if (!is_success) {
      break;
    }

    while (true) {
      token = json_tokenizer_consume(&t);
      if (!token) {
        break;
      }
      if (!json_detokenizer_write(w, token)) {
        printf("Failed to tokenize token of kind %i in file %s\n", token->kind, filename);
        is_success = false;
        break;
      }
    }
  }

  json_tokenizer_finalize(&t);
  while (true) {
    token = json_tokenizer_consume(&t);
    if (!token) {
      break;
    }
    if (!json_detokenizer_write(w, token)) {
      printf("Failed to tokenize token of kind %i in file %s\n", token->kind, filename);
      is_success = false;
      break;
    }
  }

  if (ferror(src_file)) {
    printf("Failed to read %s.\n", filename);
    json_test_suite_has_errors = true;
  }

  fclose(src_file);

  is_success = is_success && json_tokenizer_is_empty(&t);

  buffer result_buffer = is_success ? writer_consume_all_buffers(w) : EMPTY_BUFFER;

  writer_delete(w);
  json_tokenizer_deinit(&t);

  return result_buffer;
}

bool test_buffer_equals_to_file(const char *path, buffer b) {
  FILE *src_file = fopen(path, "r");
  if (src_file == NULL) {
    printf("Failed to open %s for reading.\n", path);
    json_test_suite_has_errors = true;
    return false;
  }

  size_t index = 0;
  size_t bytes_read;
  bool is_different = false;
  char read_buffer[4096];
  while (!is_different && (bytes_read = fread(read_buffer, 1, sizeof(read_buffer), src_file)) > 0) {
    for (size_t i = 0; i < bytes_read; i++) {
      if (b.data[index + i] != (byte)read_buffer[i]) {
        is_different = true;
        break;
      }
    }
    index += bytes_read;
  }

  if (ferror(src_file)) {
    printf("Failed to read %s.\n", path);
    json_test_suite_has_errors = true;
  }

  fclose(src_file);

  return !is_different && b.length == index;
}

void test_file_from_json_suite(const char *path) {
  json_suite_expected_result expected_result;
  const char *filename;
  if (!test_parse_json_suite_path(path, &expected_result, &filename)) {
    return; // not a file that interests us
  }

  buffer result_buffer = test_feed_file_into_tokenizer_detokenizer(path, filename);
  bool is_error = result_buffer.length == 0;
  if (!is_error && !test_buffer_equals_to_file(path, result_buffer)) {
    is_error = true;
    printf("%s: file differs from re-printing: %zu bytes: %.*s\n", filename, result_buffer.length, (int)result_buffer.length, result_buffer.data);
  }
  switch (expected_result) {
  case JSON_SUITE_IMPLEMENTATION_DEPENDENT:
    // printf("%s: %s\n", filename, is_error ? "errored" : "succeeded");
    json_test_suite_successes++;
    break;
  case JSON_SUITE_FAILURE:
    if (!is_error) {
      printf("%s: was expected to fail, but didn't\n", filename);
      json_test_suite_has_errors = true;
      json_test_suite_failures++;
    } else {
      json_test_suite_successes++;
    }
    break;
  case JSON_SUITE_SUCCESS:
    if (is_error) {
      printf("%s: was expected to succeed, but failed\n", filename);
      json_test_suite_has_errors = true;
      json_test_suite_failures++;
    } else {
      json_test_suite_successes++;
    }
    break;
  }
}

#include "json_tokenizer_test.c"
const char *test_json_tokenizer_random_crap() {
  const char *random_crap = "[\"a\"]\n";
  json_tokenizer t;
  TEST_ASSERT(json_tokenizer_init(&t, test_context));
  TEST_ASSERT(test_json_tokenizer_push_string(&t, random_crap));
  json_token *token;
  while (true) {
    token = json_tokenizer_consume(&t);
    if (!token) {
      break;
    }
    // if (token->kind == JSON_TOKEN_NUMBER) {
    //   printf("number: %zu %c %zu %c %c %zu\n", token->number_token.integer_part, token->number_token.has_fraction_part ? '.' : ' ',
    //          token->number_token.has_fraction_part ? token->number_token.fraction_part : 0, token->number_token.exponent_symbol ? token->number_token.exponent_symbol : ' ',
    //          token->number_token.exponent_sign ? token->number_token.exponent_sign : ' ', token->number_token.exponent_symbol ? token->number_token.exponent_part : 0);
    // } else {
    printf("kind id: %i\n", token->kind);
    // }
  }
  printf("is empty: %s\n", json_tokenizer_is_empty(&t) ? "true" : "false");
  json_tokenizer_deinit(&t);
  return NULL;
}

const char *test_jsons_of_test_suite() {
  json_test_suite_has_errors = false;
  json_test_suite_successes = 0;
  TEST_ASSERT(foreach_file_in_directory("./test/test_data/json_test_suite", test_file_from_json_suite));
  if (json_test_suite_expected_files != (json_test_suite_successes + json_test_suite_failures)) {
    return "Expected different number of test files. Did any tests run at all?";
  }
  if (json_test_suite_has_errors || json_test_suite_successes == 0) {
    printf("JSON test suite failures: %i/%i\n", json_test_suite_failures, json_test_suite_failures + json_test_suite_successes);
    return "There were some errors running JSON test suite.";
  }
  return NULL;
  // return test_json_tokenizer_random_crap();
}