#pragma once
#include "../../src/data_structures/writer.c"
#include "../../src/json/json_detokenizer.c"
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
  for (int64 i = 0; i < b.length; i++) {
    test_json_print_buffer[i] = b.data[i];
  }
  test_json_print_buffer[b.length] = 0;
  writer_deinit(&w, test_context);
  return test_json_print_buffer;
}

bool test_json_tokenizer_push_string(json_tokenizer *t, queue *q, const char *str) {
  if (!reader_push_buffer(t->reader, malloc_string_buffer(str))) {
    printf("Failed to push into reader\n");
    return false;
  }

  while (reader_get_bytes_available(t->reader) > 0) {
    json_token token = json_tokenizer_parse_next(t);
    if (token.kind == JSON_TOKEN_INVALID) {
      return false;
    }

    json_token *slot = queue_push(q);
    *slot = token;
  }

  return true;
}

NODISCARD bool test_json_tokenizer_reinit_nonempty(json_tokenizer *t) {
  if (context_is_errored(test_context)) {
    printf("Context is errored: %s\n", context_get_error(test_context)->message);
    return false;
  }

  if (json_tokenizer_is_done(t)) {
    return false;
  }
  json_tokenizer_reset(t);

  reader_deinit(t->reader, test_context);
  if (!reader_init(t->reader, test_context)) {
    printf("Failed to re-init the reader\n");
    return false;
  }

  return true;
}

NODISCARD bool test_json_tokenizer_reinit(json_tokenizer *t) {
  if (context_is_errored(test_context)) {
    printf("Context is errored: %s\n", context_get_error(test_context)->message);
    return false;
  }

  if (!json_tokenizer_is_done(t)) {
    printf("%i\n", json_tokenizer_get_current_state(t));
    return false;
  }
  json_tokenizer_reset(t);

  reader_deinit(t->reader, test_context);
  if (!reader_init(t->reader, test_context)) {
    printf("Failed to re-init the reader\n");
    return false;
  }

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
  reader r;
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context, &r));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "\"abcde\""));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
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
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  // correct escaped codepoint
  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "\"\\u12aB\""));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  k = test_json_consume(&q);
  uint64 merged_code = 0;
  merged_code = (merged_code << 8) | 'B';
  merged_code = (merged_code << 8) | 'a';
  merged_code = (merged_code << 8) | '2';
  merged_code = (merged_code << 8) | '1';
  TEST_ASSERT(k->kind == JSON_TOKEN_ESCAPED_UNICODE_CHARCODE && k->unicode_character.value == merged_code);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  // incorrect escaped codepoint
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "\"\\uGg//\""));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  // incorrect escaped codepoints in all four positions
  const char *bad_escaped_codepoints[4] = {"\"\\uxfff", "\"\\ufxff", "\"\\uffxf", "\"\\ufffx"};
  for (int64 i = 0; i < 4; i++) {
    TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, bad_escaped_codepoints[i]));
    TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
    TEST_ASSERT(test_json_consume(&q) == NULL);
    TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));
  }

  // other escapings
  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "\"\\r\\\\\\n\\t\\\"\\/\\b\\f\""));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
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
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  // correct unicode
  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "\"Привет, мир!\""));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
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
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  // unicode with broken continuation bytes
  const byte broken_continuation_bytes_unicode_pair[6] = {0xD0, 0xFF, 0xD0, 0xFF, 0xD0, 0xFF};
  char *broken_continuation_bytes_unicode_str = malloc(64);
  TEST_ASSERT(sprintf(broken_continuation_bytes_unicode_str, "\"%.*s\"", 6, broken_continuation_bytes_unicode_pair) < 64);
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, broken_continuation_bytes_unicode_str));
  free(broken_continuation_bytes_unicode_str);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  // unicode with broken continuation bytes in escape sequence
  broken_continuation_bytes_unicode_str = malloc(64);
  TEST_ASSERT(sprintf(broken_continuation_bytes_unicode_str, "\"\\%.*s\"", 6, broken_continuation_bytes_unicode_pair) < 64);
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, broken_continuation_bytes_unicode_str));
  free(broken_continuation_bytes_unicode_str);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);
  reader_deinit(&r, test_context);
  return NULL;
}

const char *test_json_tokenizer_numbers() {
  json_tokenizer t;
  json_token *k;
  queue q;
  reader r;
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context, &r));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "12345"));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !(k->number.flags & JSON_NUMBER_HAS_FRACTION) && !(k->number.flags & JSON_NUMBER_HAS_EXPONENT) && k->number.integer_part == 12345 &&
              !(k->number.flags & JSON_NUMBER_IS_NEGATIVE) && !(k->number.flags & JSON_NUMBER_INTEGER_PART_IS_ZERO));
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "-12345"));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !(k->number.flags & JSON_NUMBER_HAS_FRACTION) && !(k->number.flags & JSON_NUMBER_HAS_EXPONENT) && k->number.integer_part == 12345 &&
              k->number.flags & JSON_NUMBER_IS_NEGATIVE && !(k->number.flags & JSON_NUMBER_INTEGER_PART_IS_ZERO));
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "123.456"));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && (k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.fraction_part == 456 && !(k->number.flags & JSON_NUMBER_HAS_EXPONENT) &&
              k->number.integer_part == 123);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "123e456"));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !(k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.flags & JSON_NUMBER_HAS_EXPONENT && !(k->number.flags & JSON_NUMBER_EXPONENT_UPPERCASE) &&
              k->number.exponent_part == 456 && k->number.integer_part == 123 && !(k->number.flags & (JSON_NUMBER_EXPONENT_IS_NEGATIVE | JSON_NUMBER_EXPONENT_HAS_PLUS)) &&
              !(k->number.flags & JSON_NUMBER_INTEGER_PART_IS_ZERO));
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "123e-456"));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !(k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.flags & JSON_NUMBER_HAS_EXPONENT && !(k->number.flags & JSON_NUMBER_EXPONENT_UPPERCASE) &&
              k->number.exponent_part == 456 && k->number.integer_part == 123 && k->number.flags & JSON_NUMBER_EXPONENT_IS_NEGATIVE && !(k->number.flags & JSON_NUMBER_INTEGER_PART_IS_ZERO));
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "123e+456"));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !(k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.flags & JSON_NUMBER_HAS_EXPONENT && !(k->number.flags & JSON_NUMBER_EXPONENT_UPPERCASE) &&
              k->number.exponent_part == 456 && k->number.integer_part == 123 && k->number.flags & JSON_NUMBER_EXPONENT_HAS_PLUS && !(k->number.flags & JSON_NUMBER_INTEGER_PART_IS_ZERO));
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "123E+456"));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && !(k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.flags & JSON_NUMBER_HAS_EXPONENT && k->number.flags & JSON_NUMBER_EXPONENT_UPPERCASE &&
              k->number.exponent_part == 456 && k->number.integer_part == 123 && k->number.flags & JSON_NUMBER_EXPONENT_HAS_PLUS);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "123.456e+678"));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && (k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.fraction_part == 456 && k->number.flags & JSON_NUMBER_HAS_EXPONENT &&
              !(k->number.flags & JSON_NUMBER_EXPONENT_UPPERCASE) && k->number.exponent_part == 678 && k->number.flags & JSON_NUMBER_EXPONENT_HAS_PLUS && k->number.integer_part == 123);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "123.456E+678"));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && (k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.fraction_part == 456 && k->number.flags & JSON_NUMBER_HAS_EXPONENT &&
              k->number.flags & JSON_NUMBER_EXPONENT_UPPERCASE && k->number.exponent_part == 678 && k->number.flags & JSON_NUMBER_EXPONENT_HAS_PLUS && k->number.integer_part == 123);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "1.01e02"));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && (k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.fraction_part == 1 && k->number.fraction_leading_zeroes == 1 &&
              k->number.exponent_leading_zeroes == 1 && k->number.exponent_part == 2);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "1.0001e0002"));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && (k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.fraction_part == 1 && k->number.fraction_leading_zeroes == 3 &&
              k->number.exponent_leading_zeroes == 3 && k->number.exponent_part == 2 && !(k->number.flags & JSON_NUMBER_INTEGER_PART_IS_ZERO));
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "0"));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->number.flags & JSON_NUMBER_INTEGER_PART_IS_ZERO);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "0.0e0"));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->number.flags & JSON_NUMBER_INTEGER_PART_IS_ZERO);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "1.000e000"));
  k = test_json_consume(&q);
  TEST_ASSERT(k != NULL);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && (k->number.flags & JSON_NUMBER_HAS_FRACTION) && k->number.fraction_part == 0 && k->number.fraction_leading_zeroes == 3 &&
              k->number.exponent_leading_zeroes == 3 && k->number.exponent_part == 0 && !(k->number.flags & JSON_NUMBER_INTEGER_PART_IS_ZERO));
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  // overflows
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "1844674407370955200000"));
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "1e1844674407370955200000"));
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "1.1844674407370955200000"));
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  // invalid number-like values
  const char *wrong_numbers[] = {"1.", ".1", "-.1", "e1", "-E1", "1.e5", "--", "1.-", "1ee", "01", "-01", "1..1", "1ee1", "-e1", "1e+-1", "1e--1", "1.-1", "1e", "1.", "-", NULL};
  for (int i = 0;; i++) {
    const char *str = wrong_numbers[i];
    if (!str) {
      break;
    }
    // printf("testing: %s\n", str);
    TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, str));
    TEST_ASSERT(test_json_consume(&q) == NULL);
    TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));
  }

  const char *almost_wrong_numbers[] = {"1-", "1e1+", "1.1.1", "1e1.1", "1e1e1", "1-1", "1e1-1", NULL};
  for (int i = 0;; i++) {
    const char *str = almost_wrong_numbers[i];
    if (!str) {
      break;
    }
    // printf("testing: %s\n", str);
    TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, str));
    TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_NUMBER);
    TEST_ASSERT(test_json_consume(&q) == NULL);
    TEST_ASSERT(reader_get_bytes_available(t.reader) > 0);
    TEST_ASSERT(test_json_tokenizer_reinit(&t));
  }

  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);
  reader_deinit(&r, test_context);
  return NULL;
}

const char *test_json_tokenizer_constant(json_tokenizer *t, const byte *content, memsize length, json_token_kind expected_kind, bool is_correct_nonempty) {
  for (memsize i = 0; i < length; i++) {
    buffer b = buffer_make(malloc(length), i + 1, length);
    memcpy(b.data, content, length);

    TEST_ASSERT(reader_push_buffer(t->reader, b));
    if (i == length - 1) {
      TEST_ASSERT(json_tokenizer_parse_next(t).kind == expected_kind);
      if (is_correct_nonempty) {
        TEST_ASSERT(test_json_tokenizer_reinit_nonempty(t));
      } else {
        TEST_ASSERT(test_json_tokenizer_reinit(t));
      }

    } else {
      TEST_ASSERT(json_tokenizer_parse_next(t).kind == JSON_TOKEN_INVALID);
      TEST_ASSERT(test_json_tokenizer_reinit_nonempty(t));
    }
  }

  return NULL;
}

const char *test_json_tokenizer_constants() {
  json_tokenizer t;
  queue q;
  reader r;
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context, &r));

  TEST_ASSERT(!test_json_tokenizer_constant(&t, _JTOK_TRUE_BYTES, sizeof(_JTOK_TRUE_BYTES), JSON_TOKEN_TRUE, false));
  TEST_ASSERT(!test_json_tokenizer_constant(&t, _JTOK_FALSE_BYTES, sizeof(_JTOK_FALSE_BYTES), JSON_TOKEN_FALSE, false));
  TEST_ASSERT(!test_json_tokenizer_constant(&t, _JTOK_NULL_BYTES, sizeof(_JTOK_NULL_BYTES), JSON_TOKEN_NULL, false));
  TEST_ASSERT(!test_json_tokenizer_constant(&t, UTF8_BOM, sizeof(UTF8_BOM), JSON_TOKEN_BOM, true));

  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);
  reader_deinit(&r, test_context);
  return NULL;
}

const char *test_json_tokenizer_loooong_number() {
  json_tokenizer t;
  queue q;
  reader r;
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context, &r));

  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "-0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001"));
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "-0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001"));

  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);
  reader_deinit(&r, test_context);
  return NULL;
}

const char *test_json_tokenizer_array() {
  json_tokenizer t;
  queue q;
  reader r;
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  json_token *k;
  TEST_ASSERT(json_tokenizer_init(&t, test_context, &r));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "[1 , 2,3,4]"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && k->number.integer_part == 1);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_VALUE_SEPARATOR);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && k->number.integer_part == 2);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_VALUE_SEPARATOR);
  k = test_json_consume(&q);
  TEST_ASSERT(k->kind == JSON_TOKEN_NUMBER && k->number.integer_part == 3);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_VALUE_SEPARATOR);
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
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_VALUE_SEPARATOR);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_FALSE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_VALUE_SEPARATOR);
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
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_VALUE_SEPARATOR);
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
  reader_deinit(&r, test_context);
  return NULL;
}

const char *test_json_tokenizer_object() {
  json_tokenizer t;
  queue q;
  reader r;
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context, &r));

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
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'w');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'i');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'd');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 't');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'h');
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_KV_SEPARATOR);
  TEST_ASSERT(test_json_consume(&q)->number.integer_part == 15);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_VALUE_SEPARATOR);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'i');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 's');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == '_');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'g');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'o');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'o');
  TEST_ASSERT(test_json_consume(&q)->unicode_character.value == 'd');
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_KV_SEPARATOR);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_TRUE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "{uwu"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  TEST_ASSERT(test_json_tokenizer_reinit_nonempty(&t));

  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);
  reader_deinit(&r, test_context);
  return NULL;
}

const char *test_json_tokenizer_whitespaces() {
  json_tokenizer t;
  queue q;
  reader r;
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  json_token *k;
  TEST_ASSERT(json_tokenizer_init(&t, test_context, &r));

  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "[1, \n\r\t2]"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_NUMBER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_VALUE_SEPARATOR);
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
  reader_deinit(&r, test_context);
  return NULL;
}

const char *test_json_tokenizer_bom() {
  json_tokenizer t;
  queue q;
  reader r;
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context, &r));
  TEST_ASSERT(json_tokenizer_get_current_state(&t) == JSON_STATE_START);

  buffer bom_buffer = buffer_make(malloc(sizeof(UTF8_BOM)), sizeof(UTF8_BOM), sizeof(UTF8_BOM));
  memcpy(bom_buffer.data, UTF8_BOM, 3);
  TEST_ASSERT(reader_push_buffer(t.reader, bom_buffer));
  TEST_ASSERT(json_tokenizer_parse_next(&t).kind == JSON_TOKEN_BOM);
  TEST_ASSERT(reader_push_buffer(t.reader, malloc_string_buffer("{}")));
  TEST_ASSERT(json_tokenizer_parse_next(&t).kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(json_tokenizer_parse_next(&t).kind == JSON_TOKEN_OBJECT_CLOSE);
  TEST_ASSERT(test_json_tokenizer_reinit(&t));

  json_tokenizer_deinit(&t, test_context);
  queue_deinit(&q, test_context);
  reader_deinit(&r, test_context);
  return NULL;
}

const char *test_json_tokenizer_nesting() {
  json_tokenizer t;
  queue q;
  reader r;
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context, &r));
  // this also tests for whitespaces everywhere
  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, "[ { \"a\" : { \"b\" : [ [ { \"c\" : null } ] ] } } ]"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_UNICODE_CHARACTER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_KV_SEPARATOR);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_UNICODE_CHARACTER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_KV_SEPARATOR);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_ARRAY_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_UNICODE_CHARACTER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_STRING_DELIMITER);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_WHITESPACE);
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_KV_SEPARATOR);
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
  reader_deinit(&r, test_context);
  return NULL;
}

const char *test_json_tokenizer_emptiness() {
  json_tokenizer t;
  queue q;
  reader r;
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context, &r));
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
  reader_deinit(&r, test_context);
  return NULL;
}

const char *test_json_tokenizer_stack_limitations() {
  json_tokenizer t;
  queue q;
  reader r;

  char *long_string_with_repeats;

  setup_test_context_default();
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context, &r));
  long_string_with_repeats = string_repeat("[", JSON_MAX_STATE_STACK_LENGTH);
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, long_string_with_repeats));
  free(long_string_with_repeats);
  json_tokenizer_deinit(&t, test_context);
  reader_deinit(&r, test_context);
  queue_deinit(&q, test_context);

  setup_test_context_default();
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context, &r));
  long_string_with_repeats = string_repeat("[", JSON_MAX_STATE_STACK_LENGTH - 1);
  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, long_string_with_repeats));
  free(long_string_with_repeats);
  queue_reset(&q);
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "\""));
  TEST_ASSERT(test_json_consume(&q) == NULL);
  json_tokenizer_deinit(&t, test_context);
  reader_deinit(&r, test_context);
  queue_deinit(&q, test_context);

  setup_test_context_default();
  TEST_ASSERT(reader_init(&r, test_context));
  TEST_ASSERT(queue_init(&q, test_context, sizeof(json_token), 4));
  TEST_ASSERT(json_tokenizer_init(&t, test_context, &r));
  long_string_with_repeats = string_repeat("{\"a\":", JSON_MAX_STATE_STACK_LENGTH - 2);
  TEST_ASSERT(test_json_tokenizer_push_string(&t, &q, long_string_with_repeats));
  free(long_string_with_repeats);
  queue_reset(&q);
  TEST_ASSERT(!test_json_tokenizer_push_string(&t, &q, "{\"a\":"));
  TEST_ASSERT(test_json_consume(&q)->kind == JSON_TOKEN_OBJECT_OPEN);
  TEST_ASSERT(test_json_consume(&q) == NULL);
  json_tokenizer_deinit(&t, test_context);
  reader_deinit(&r, test_context);
  queue_deinit(&q, test_context);

  return NULL;
}

bool test_json_tokenizer_will_parse(const char *format, const char *base, int times) {
  char *src = string_repeat_sprintf(format, base, times);
  reader r;
  if (!reader_init(&r, test_context)) {
    printf("Failed to init reader\n");
    return false;
  }

  json_tokenizer t;
  if (!json_tokenizer_init(&t, test_context, &r)) {
    printf("Failed to init\n");
    return false;
  }

  if (!reader_push_buffer(&r, malloc_string_buffer(src))) {
    printf("Failed to push to reader\n");
    return false;
  }

  while (reader_get_bytes_available(&r) > 0) {
    if (json_tokenizer_parse_next(&t).kind == JSON_TOKEN_INVALID) {
      printf("Failed to parse\n");
      return false;
    }
  }

  if (!json_tokenizer_is_done(&t)) {
    printf("Not done after all is pushed\n");
    return false;
  }

  json_tokenizer_deinit(&t, test_context);
  reader_deinit(&r, test_context);
  free(src);
  return true;
}

// this tests exists to check if I screwed up in pushing/popping/replacing states somewhere
const char *test_json_tokenizer_state_management() {
  int64 times = JSON_MAX_STATE_STACK_LENGTH * 3;
  const char *fmt = "[%s true]";
  TEST_ASSERT(test_json_tokenizer_will_parse(fmt, "true, ", times));
  TEST_ASSERT(test_json_tokenizer_will_parse(fmt, "false, ", times));
  TEST_ASSERT(test_json_tokenizer_will_parse(fmt, "null, ", times));
  TEST_ASSERT(test_json_tokenizer_will_parse(fmt, "[], ", times));
  TEST_ASSERT(test_json_tokenizer_will_parse(fmt, "[1], ", times));
  TEST_ASSERT(test_json_tokenizer_will_parse(fmt, "{}, ", times));
  TEST_ASSERT(test_json_tokenizer_will_parse(fmt, "{\"key\":\"value\"}, ", times));
  TEST_ASSERT(test_json_tokenizer_will_parse(fmt, "\"12345678910, ня!\", ", times));
  TEST_ASSERT(test_json_tokenizer_will_parse(fmt, "\"\\n\\r\\t\\f\\\\\", ", times));
  TEST_ASSERT(test_json_tokenizer_will_parse(fmt, "\"\\u1234\\ubeEF\", ", times));
  TEST_ASSERT(test_json_tokenizer_will_parse(fmt, "123456, ", times));
  TEST_ASSERT(test_json_tokenizer_will_parse(fmt, "123456.123456, ", times));
  TEST_ASSERT(test_json_tokenizer_will_parse(fmt, "123456.123456e1234, ", times));
  TEST_ASSERT(test_json_tokenizer_will_parse(fmt, "-123456.123456E-1234, ", times));

  return NULL;
}

bool test_json_tokenizer_breakage(const char *src) {
  writer w;
  writer tmp_writer;
  json_tokenizer t;
  reader r;
  if (!reader_init(&r, test_context) || !json_tokenizer_init(&t, test_context, &r) || !writer_init(&w, test_context, 256, 4) || !writer_init(&tmp_writer, test_context, 256, 4)) {
    printf("Failed to init\n");
    return false;
  }

  if (!reader_push_buffer(&r, malloc_string_buffer(src))) {
    printf("Failed to push to reader\n");
    return false;
  }

  bool has_breakage = false;
  while (reader_get_bytes_available(t.reader) > 0) {
    json_token token = json_tokenizer_parse_next(&t);
    if (token.kind == JSON_TOKEN_INVALID) {
      has_breakage = true;
      break;
    }

    json_detokenizer_write(&w, &token);
  }

  if (!has_breakage && json_tokenizer_is_done(&t)) {
    printf("Expected to had breakage, but didnt: for %s\n", src);
    return false;
  }

  int64 src_len = strlen(src);
  while (reader_get_bytes_available(t.reader) > 0) {
    writer_write_byte(&w, reader_read_byte(t.reader));
  }

  buffer_or_error reprint_or_error = writer_consume_all_buffers(&w);
  if (reprint_or_error.is_error) {
    printf("Error producing buffer\n");
    return false;
  }
  buffer reprint = reprint_or_error.buffer;

  for (int64 i = 0; src[i] != 0 && i < reprint.length; i++) {
    if ((byte)src[i] != reprint.data[i]) {
      printf("Failed to reprint (mismatch at %zu: %i vs %i) %s: reprinted as %.*s\n", i, (byte)src[i], reprint.data[i], src, (int)reprint.length, reprint.data);
      return false;
    }
  }

  if (src_len != reprint.length) {
    printf("Failed to reprint (length mismatch) %s: reprinted as %.*s\n", src, (int)reprint.length, reprint.data);
    return false;
  }

  free(reprint.data);

  json_tokenizer_deinit(&t, test_context);
  writer_deinit(&w, test_context);
  writer_deinit(&tmp_writer, test_context);
  reader_deinit(&r, test_context);
  return true;
}

bool test_json_tokenizer_breakage_with_suffixes(const char *src) {
  int64 src_len = strlen(src);
  char *input = malloc(sizeof(char) * (src_len + 1));
  input[src_len] = 0;
  for (int64 i = 0; i < src_len; i++) {
    input[i] = 'X';
  }

  for (int64 i = 0; i < src_len; i++) {
    if (!test_json_tokenizer_breakage(input)) {
      return false;
    }
    input[i] = src[i];
  }

  free(input);
  return true;
}

const char *test_json_tokenizer_broken_json_recovery() {
  TEST_ASSERT(test_json_tokenizer_breakage_with_suffixes("true"));
  TEST_ASSERT(test_json_tokenizer_breakage_with_suffixes("false"));
  TEST_ASSERT(test_json_tokenizer_breakage_with_suffixes("null"));
  const char utf8_bom_zeroterminated[4] = {UTF8_BOM[0], UTF8_BOM[1], UTF8_BOM[2], 0};
  TEST_ASSERT(test_json_tokenizer_breakage_with_suffixes(utf8_bom_zeroterminated));
  TEST_ASSERT(test_json_tokenizer_breakage_with_suffixes("[-0, -1, -2]"));
  TEST_ASSERT(test_json_tokenizer_breakage_with_suffixes("[-01.01e02, -1.02e03, -2.03E04]"));
  TEST_ASSERT(test_json_tokenizer_breakage_with_suffixes("[-1.02e03, -2.03E04, 01.01e02]"));
  TEST_ASSERT(test_json_tokenizer_breakage_with_suffixes("\"\\n\\r\\t\\\"\\\\\""));
  TEST_ASSERT(test_json_tokenizer_breakage_with_suffixes("\"\\u1234\\uBeEf\""));
  TEST_ASSERT(test_json_tokenizer_breakage_with_suffixes("\"Привет, мир!\""));
  TEST_ASSERT(test_json_tokenizer_breakage_with_suffixes("{\"key\":\"value\"}"));

  return NULL;
}

bool test_json_tokenizer_stack_push_fail(const char *src, int pushes_before_failure) {
  writer w;
  writer tmp_writer;
  json_tokenizer t;
  reader r;
  if (!reader_init(&r, test_context) || !json_tokenizer_init(&t, test_context, &r) || !writer_init(&w, test_context, 256, 4) || !writer_init(&tmp_writer, test_context, 256, 4)) {
    printf("Failed to init\n");
    return false;
  }

  // bit weird to do it like that, but it's the easiest way to get tokenizer to that state
  t.jsm.state_stack.byte_length = t.jsm.state_stack.index + pushes_before_failure;
  if (!reader_push_buffer(&r, malloc_string_buffer(src))) {
    printf("Failed to push to reader\n");
    return false;
  }

  bool has_breakage = false;
  while (reader_get_bytes_available(&r) > 0) {
    json_token token = json_tokenizer_parse_next(&t);
    if (token.kind == JSON_TOKEN_INVALID) {
      has_breakage = true;
      break;
    }
    json_detokenizer_write(&w, &token);
  }

  if (!has_breakage) {
    printf("Expected to had breakage, but didnt: for %s\n", src);
    return false;
  }

  int64 src_len = strlen(src);
  while (reader_get_bytes_available(&r) > 0) {
    writer_write_byte(&w, reader_read_byte(&r));
  }

  buffer_or_error reprint_or_error = writer_consume_all_buffers(&w);
  if (reprint_or_error.is_error) {
    printf("Error producing buffer\n");
    return false;
  }
  buffer reprint = reprint_or_error.buffer;

  for (int64 i = 0; src[i] != 0 && i < reprint.length; i++) {
    if ((byte)src[i] != reprint.data[i]) {
      printf("Failed to reprint (mismatch at %zu: %i vs %i) %s: reprinted as %.*s\n", i, (byte)src[i], reprint.data[i], src, (int)reprint.length, reprint.data);
      return false;
    }
  }

  if (src_len != reprint.length) {
    printf("Failed to reprint (length mismatch) %s: reprinted as %.*s\n", src, (int)reprint.length, reprint.data);
    return false;
  }

  free(reprint.data);

  json_tokenizer_deinit(&t, test_context);
  writer_deinit(&w, test_context);
  writer_deinit(&tmp_writer, test_context);
  reader_deinit(&r, test_context);
  return true;
}

const char *test_json_tokenizer_stack_push_fails() {
  TEST_ASSERT(test_json_tokenizer_stack_push_fail("[1,2]", 0));
  TEST_ASSERT(test_json_tokenizer_stack_push_fail("[\"test\"]", 0));
  TEST_ASSERT(test_json_tokenizer_stack_push_fail("[{}]", 0));
  TEST_ASSERT(test_json_tokenizer_stack_push_fail("[[]]", 0));
  TEST_ASSERT(test_json_tokenizer_stack_push_fail("{\"key\": 1, \"key2\": 2}", 0));
  return NULL;
}

const char *test_json_tokenizer_allocation_failures() {
  json_tokenizer t;
  setup_test_context(0);
  TEST_ASSERT(!json_tokenizer_init(&t, test_context, NULL));

  setup_test_context(1);
  TEST_ASSERT(json_tokenizer_init(&t, test_context, NULL));

  json_tokenizer_deinit(&t, test_context);
  return NULL;
}