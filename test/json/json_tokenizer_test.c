#pragma once
#include "../../src/json/json_tokenizer.c"
#include "../test_utils.c"
#include <stdio.h>

char test_json_print_buffer[128];
const char *test_json_print_token(json_token *t) {
  writer w;
  if (!writer_init(&w, test_context, 1024, 4)) {
    return "FAIL_TO_PRINT";
  }
  json_detokenizer_write(&w, t);
  buffer b = writer_peek_current_buffer(&w);
  for (size_t i = 0; i < b.length; i++) {
    test_json_print_buffer[i] = b.data[i];
  }
  test_json_print_buffer[b.length] = 0;
  writer_deinit(&w, test_context);
  return test_json_print_buffer;
}

bool test_json_tokenizer_push_string(json_tokenizer *t, queue *q, const char *str) {
  json_token *token;
  for (int i = 0; str[i] != 0; i++) {
    if (!json_tokenizer_push(t, str[i])) {
      return false;
    }
    while (true) {
      token = json_tokenizer_consume(t);
      if (!token) {
        break;
      }
      json_token *slot = queue_push(q);
      *slot = *token;
      // printf("found: %i; %s\n", token->kind, test_json_print_token(token));
    }
  }
  return true;
}

bool test_json_tokenizer_reinit_nonempty(json_tokenizer *t) {
  if (json_tokenizer_is_done(t)) {
    return false;
  }
  json_tokenizer_reset(t);
  return true;
}

bool test_json_tokenizer_reinit(json_tokenizer *t) {
  if (!json_tokenizer_is_done(t)) {
    return false;
  }
  json_tokenizer_reset(t);
  return true;
}

json_token *test_json_consume(queue *q) {
  if (queue_get_count(q) == 0) {
    return NULL;
  }
  return (json_token *)queue_pop(q);
}

const char *test_json_tokenizer_string() {
  json_tokenizer t;
  json_token *k;
  queue q;
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "\"abcde\""));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == 'a' && k->unicode_character.length == 1);
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == 'b' && k->unicode_character.length == 1);
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == 'c' && k->unicode_character.length == 1);
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == 'd' && k->unicode_character.length == 1);
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == 'e' && k->unicode_character.length == 1);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  // correct escaped codepoint
  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "\"\\u12aB\""));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  k = test_json_consume(&q);
  uint64_t merged_code = 0;
  merged_code = (merged_code << 8) | 'B';
  merged_code = (merged_code << 8) | 'a';
  merged_code = (merged_code << 8) | '2';
  merged_code = (merged_code << 8) | '1';
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPED_UNICODE_CHARCODE && k->unicode_character.value == merged_code);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  // incorrect escaped codepoint
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "\"\\uGg//\""));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  // incorrect escaped codepoints in all four positions
  const char *bad_escaped_codepoints[4] = {"\"\\uxfff", "\"\\ufxff", "\"\\uffxf", "\"\\ufffx"};
  for (size_t i = 0; i < 4; i++) {
    TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, bad_escaped_codepoints[i]));
    TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
    TEST_ASSERT(test_json_consume(&q) == NULL);
    TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));
  }

  // other escapings
  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "\"\\r\\\\\\n\\t\\\"\\/\\b\\f\""));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPE_SEQUENCE && k->character.character == 'r');
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPE_SEQUENCE && k->character.character == '\\');
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPE_SEQUENCE && k->character.character == 'n');
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPE_SEQUENCE && k->character.character == 't');
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPE_SEQUENCE && k->character.character == '"');
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPE_SEQUENCE && k->character.character == '/');
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPE_SEQUENCE && k->character.character == 'b');
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPE_SEQUENCE && k->character.character == 'f');
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  // correct unicode
  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "\"Привет, мир!\""));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == (0xD0 | (0x9F << 8)));
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == (0xD1 | (0x80 << 8)));
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == (0xD0 | (0xB8 << 8)));
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == (0xD0 | (0xB2 << 8)));
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == (0xD0 | (0xB5 << 8)));
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == (0xD1 | (0x82 << 8)));
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == ',');
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == ' ');
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == (0xD0 | (0xBC << 8)));
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == (0xD0 | (0xB8 << 8)));
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == (0xD1 | (0x80 << 8)));
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_UNICODE_CHARACTER && k->unicode_character.value == '!');
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  // unicode with broken continuation bytes
  const byte broken_continuation_bytes_unicode_pair[6] = {0xD0, 0xFF, 0xD0, 0xFF, 0xD0, 0xFF};
  char *broken_continuation_bytes_unicode_str = malloc(64);
  TEST_ASSERT(sprintf(broken_continuation_bytes_unicode_str, "\"%.*s\"", 6, broken_continuation_bytes_unicode_pair) < 64);
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, broken_continuation_bytes_unicode_str));
  free(broken_continuation_bytes_unicode_str);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  // unicode with broken continuation bytes in escape sequence
  broken_continuation_bytes_unicode_str = malloc(64);
  TEST_ASSERT(sprintf(broken_continuation_bytes_unicode_str, "\"\\%.*s\"", 6, broken_continuation_bytes_unicode_pair) < 64);
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, broken_continuation_bytes_unicode_str));
  free(broken_continuation_bytes_unicode_str);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);
  return NULL;
}

const char *test_json_tokenizer_numbers() {
  json_tokenizer t;
  json_token *k;
  queue q;
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "12345 "));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !(k->number.flags & JSON_NUMBER_HAS_FRACTION) && !(k->number.flags & JSON_NUMBER_HAS_EXPONENT) && k->number.integer_part == 12345 &&
              !(k->number.flags & JSON_NUMBER_IS_NEGATIVE));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "-12345 "));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !(k->number.flags & JSON_NUMBER_HAS_FRACTION) && !(k->number.flags & JSON_NUMBER_HAS_EXPONENT) && k->number.integer_part == 12345 &&
              k->number.flags & JSON_NUMBER_IS_NEGATIVE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "123.456 "));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && (k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.fraction_part == 456 && !(k->number.flags & JSON_NUMBER_HAS_EXPONENT) &&
              k->number.integer_part == 123);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "123e456 "));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !(k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.flags & JSON_NUMBER_HAS_EXPONENT && !(k->number.flags & JSON_NUMBER_EXPONENT_UPPERCASE) &&
              k->number.exponent_part == 456 && k->number.integer_part == 123 && !(k->number.flags & (JSON_NUMBER_EXPONENT_IS_NEGATIVE | JSON_NUMBER_EXPONENT_HAS_PLUS)));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "123e-456 "));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !(k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.flags & JSON_NUMBER_HAS_EXPONENT && !(k->number.flags & JSON_NUMBER_EXPONENT_UPPERCASE) &&
              k->number.exponent_part == 456 && k->number.integer_part == 123 && k->number.flags & JSON_NUMBER_EXPONENT_IS_NEGATIVE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "123e+456 "));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !(k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.flags & JSON_NUMBER_HAS_EXPONENT && !(k->number.flags & JSON_NUMBER_EXPONENT_UPPERCASE) &&
              k->number.exponent_part == 456 && k->number.integer_part == 123 && k->number.flags & JSON_NUMBER_EXPONENT_HAS_PLUS);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "123E+456 "));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !(k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.flags & JSON_NUMBER_HAS_EXPONENT && k->number.flags & JSON_NUMBER_EXPONENT_UPPERCASE &&
              k->number.exponent_part == 456 && k->number.integer_part == 123 && k->number.flags & JSON_NUMBER_EXPONENT_HAS_PLUS);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "123.456e+678 "));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && (k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.fraction_part == 456 && k->number.flags & JSON_NUMBER_HAS_EXPONENT &&
              !(k->number.flags & JSON_NUMBER_EXPONENT_UPPERCASE) && k->number.exponent_part == 678 && k->number.flags & JSON_NUMBER_EXPONENT_HAS_PLUS && k->number.integer_part == 123);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "123.456E+678 "));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && (k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.fraction_part == 456 && k->number.flags & JSON_NUMBER_HAS_EXPONENT &&
              k->number.flags & JSON_NUMBER_EXPONENT_UPPERCASE && k->number.exponent_part == 678 && k->number.flags & JSON_NUMBER_EXPONENT_HAS_PLUS && k->number.integer_part == 123);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "1.01e02 "));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && (k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.fraction_part == 1 && k->number.fraction_leading_zeroes == 1 &&
              k->number.exponent_leading_zeroes == 1 && k->number.exponent_part == 2);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "1.0001e0002 "));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && (k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.fraction_part == 1 && k->number.fraction_leading_zeroes == 3 &&
              k->number.exponent_leading_zeroes == 3 && k->number.exponent_part == 2);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "1.000e000 "));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && (k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.fraction_part == 0 && k->number.fraction_leading_zeroes == 3 &&
              k->number.exponent_leading_zeroes == 3 && k->number.exponent_part == 0);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  // overflows
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "1844674407370955200000 "));
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "1e1844674407370955200000 "));
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "1.1844674407370955200000 "));
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  // invalid number-like values
  const char *wrong_numbers[] = {"1.",   ".1",    "-.1",  "e1",    "-E1", "1.e5",  "--",    "1-",  "1.-",  "1e1+",  "1ee", "01", "-01", "1.1.1",
                                 "1..1", "1e1.1", "1ee1", "1e1e1", "-e1", "1e+-1", "1e--1", "1-1", "1.-1", "1e1-1", "1e",  "1.", "-",   NULL};
  for (int i = 0;; i++) {
    const char *str = wrong_numbers[i];
    if (!str) {
      break;
    }
    // printf("testing: %s\n", str);
    TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, str) || !test_json_tokenizer_push_string(&t, &q, " "));
    TEST_ASSERT(test_json_consume(&q) == NULL);
    // it will be nonempty most of the time, but `1-` leaves it in clean state
    json_tokenizer_reset(&t);
  }

  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);
  return NULL;
}

const char *test_json_tokenizer_constants() {
  json_tokenizer t;
  queue q;
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context));

  const char true_str[5] = "true";
  for (size_t i = 0; i < sizeof(true_str) - 1; i++) {
    for (size_t j = 0; j < sizeof(true_str) - 1; j++) {
      if (j <= i) {
        TEST_ASSERT(json_tokenizer_push(&t, true_str[j]));
      } else {
        TEST_ASSERT(!json_tokenizer_push(&t, '0'));
      }
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
      if (j <= i) {
        TEST_ASSERT(json_tokenizer_push(&t, false_str[j]));
      } else {
        TEST_ASSERT(!json_tokenizer_push(&t, '0'));
      }
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
      if (j <= i) {
        TEST_ASSERT(json_tokenizer_push(&t, null_str[j]));
      } else {
        TEST_ASSERT(!json_tokenizer_push(&t, '0'));
      }
    }
    if (i == sizeof(null_str) - 2) {
      TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_NULL);
      TEST_ASSERT(test_json_tokenizer_reinit(&t));
    } else {
      TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
      TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));
    }
  }

  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);
  return NULL;
}

const char *test_json_tokenizer_array() {
  json_tokenizer t;
  queue q;
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  json_token *k;
  TEST_ASSERT(json_tokenizer_init(&t, test_context));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "[1 , 2,3,4]"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && k->number.integer_part == 1);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_COMMA);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && k->number.integer_part == 2);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_COMMA);
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && k->number.integer_part == 3);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_COMMA);
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && k->number.integer_part == 4);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "[]"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "[\r \t\n]"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "[true, false, null]"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_TRUE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_COMMA);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_FALSE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_COMMA);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_NULL);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "[,]"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "[1,,]"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_NUMBER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_COMMA);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "[1 1]"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_NUMBER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);
  return NULL;
}

const char *test_json_tokenizer_object() {
  json_tokenizer t;
  queue q;
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "{}"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "{\r \t\n}"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "{\"width\":15, \"is_good\":true}"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'w');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'i');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'd');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 't');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'h');
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_COLON);
  TEST_ASSERT(test_json_consume(&q)->number.integer_part == 15);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_COMMA);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'i');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 's');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == '_');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'g');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'o');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'o');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'd');
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_COLON);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_TRUE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "{uwu"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);
  return NULL;
}

const char *test_json_tokenizer_whitespaces() {
  json_tokenizer t;
  queue q;
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  json_token *k;
  TEST_ASSERT(json_tokenizer_init(&t, test_context));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "[1, \n\r\t2]"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_NUMBER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_COMMA);
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_WHITESPACE && k->character.character == ' ');
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_WHITESPACE && k->character.character == '\n');
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_WHITESPACE && k->character.character == '\r');
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_WHITESPACE && k->character.character == '\t');
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_NUMBER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "[owo]"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);
  return NULL;
}

const char *test_json_tokenizer_bom() {
  json_tokenizer t;
  queue q;
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context));

  TEST_ASSERT(json_tokenizer_push(&t, UTF8_BOM[0]));
  TEST_ASSERT(json_tokenizer_push(&t, UTF8_BOM[1]));
  TEST_ASSERT(json_tokenizer_push(&t, UTF8_BOM[2]));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_BOM);
  TEST_ASSERT(json_tokenizer_push(&t, '{'));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(json_tokenizer_push(&t, '}'));
  TEST_ASSERT(json_tokenizer_consume(&t)->kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(json_tokenizer_push(&t, UTF8_BOM[0]));
  TEST_ASSERT(json_tokenizer_push(&t, UTF8_BOM[1]));
  TEST_ASSERT(!json_tokenizer_push(&t, 0x0));
  TEST_ASSERT(json_tokenizer_consume(&t) == NULL);
  TEST_ASSERT(!json_tokenizer_is_done(&t));

  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);
  return NULL;
}

const char *test_json_tokenizer_nesting() {
  json_tokenizer t;
  queue q;
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context));
  // this also tests for whitespaces everywhere
  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "[ { \"a\" : { \"b\" : [ [ { \"c\" : null } ] ] } } ]"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_UNICODE_CHARACTER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_COLON);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_UNICODE_CHARACTER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_COLON);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_UNICODE_CHARACTER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_QUOTES);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_COLON);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_NULL);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);
  return NULL;
}

const char *test_json_tokenizer_emptiness() {
  json_tokenizer t;
  queue q;
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context));
  TEST_ASSERT(!json_tokenizer_is_done(&t));
  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "["));
  TEST_ASSERT(!json_tokenizer_is_done(&t));
  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "]"));
  TEST_ASSERT(json_tokenizer_is_done(&t));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_CLOSE);
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "123"));
  TEST_ASSERT(json_tokenizer_is_done(&t));

  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);
  return NULL;
}

const char *test_json_tokenizer_stack_limitations() {
  json_tokenizer t;
  queue q;
  char *long_string_with_repeats;

  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  setup_test_context_default();
  TEST_ASSERT(json_tokenizer_init(&t, test_context));
  long_string_with_repeats = string_repeat("[", 1, JSON_MAX_STATE_STACK_LENGTH);
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, long_string_with_repeats));
  free(long_string_with_repeats);
  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);

  setup_test_context_default();
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context));
  long_string_with_repeats = string_repeat("[", 1, JSON_MAX_STATE_STACK_LENGTH - 1);
  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, long_string_with_repeats));
  free(long_string_with_repeats);
  queue_reset(&q);
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "\""));
  TEST_ASSERT(test_json_consume(&q) == NULL);
  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);

  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  setup_test_context_default();
  TEST_ASSERT(json_tokenizer_init(&t, test_context));
  long_string_with_repeats = string_repeat("{\"a\":", 5, JSON_MAX_STATE_STACK_LENGTH - 2);
  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, long_string_with_repeats));
  free(long_string_with_repeats);
  queue_reset(&q);
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "{\"a\":"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);

  return NULL;
}

const char *test_json_tokenizer_allocation_failures() {
  json_tokenizer t;
  setup_test_context(0);
  TEST_ASSERT(!json_tokenizer_init(&t, test_context));

  setup_test_context(1);
  TEST_ASSERT(!json_tokenizer_init(&t, test_context));

  setup_test_context(2);
  TEST_ASSERT(json_tokenizer_init(&t, test_context));

  json_tokenizer_deinit(&t, test_context);
  return NULL;
}