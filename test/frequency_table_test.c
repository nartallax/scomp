#pragma once
#include "../src/frequency_table.c"
#include "./test_utils.c"
#include <stdlib.h>

const char *test_ftable_simple() {
  ftable table;
  TEST_ASSERT(ftable_init(&table, test_context, 5, FTABLE_INIT_ZERO | FTABLE_EXCLUDE_EOF));
  TEST_ASSERT(ftable_get_frequency(&table, 4) == 0);
  TEST_ASSERT(ftable_get_symbol_count(&table) == 5);

  ftable_increment(&table, 3);
  ftable_increment(&table, 2);
  ftable_increment(&table, 2);
  TEST_ASSERT(ftable_get_frequency(&table, 3) == 1);
  TEST_ASSERT(ftable_get_frequency(&table, 2) == 2);
  TEST_ASSERT(ftable_get_low(&table, 2) == 0);
  TEST_ASSERT(ftable_get_high(&table, 2) == 2);
  TEST_ASSERT(ftable_get_low(&table, 3) == 2);
  TEST_ASSERT(ftable_get_high(&table, 3) == 3);

  ftable_deinit(&table);
  return NULL;
}

const char *test_ftable_with_eof() {
  ftable table;
  TEST_ASSERT(ftable_init(&table, test_context, 5, FTABLE_INIT_ZERO | FTABLE_INCLUDE_EOF));
  symbol eof = ftable_get_eof_symbol(&table);
  TEST_ASSERT(eof == 5);
  TEST_ASSERT(ftable_get_frequency(&table, eof) == 1);
  TEST_ASSERT(ftable_get_symbol_count(&table) == 5);

  ftable_increment(&table, 2);
  ftable_increment(&table, 4);
  TEST_ASSERT(ftable_get_frequency(&table, eof) == 1);
  TEST_ASSERT(ftable_get_low(&table, eof) == 2);
  TEST_ASSERT(ftable_get_high(&table, eof) == 3);

  ftable_deinit(&table);
  return NULL;
}

const char *test_ftable_init_one() {
  ftable table;
  TEST_ASSERT(ftable_init(&table, test_context, 5, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF));
  TEST_ASSERT(ftable_get_frequency(&table, 3) == 1);
  TEST_ASSERT(ftable_get_low(&table, 3) == 3);
  TEST_ASSERT(ftable_get_high(&table, 3) == 4);

  ftable_increment(&table, 0);
  ftable_increment(&table, 2);
  ftable_increment(&table, 2);
  ftable_increment(&table, 3);
  TEST_ASSERT(ftable_get_low(&table, 3) == 6);
  TEST_ASSERT(ftable_get_high(&table, 3) == 8);
  ftable_deinit(&table);

  TEST_ASSERT(ftable_init(&table, test_context, 5, FTABLE_INIT_ONE | FTABLE_INCLUDE_EOF));
  symbol eof = ftable_get_eof_symbol(&table);
  TEST_ASSERT(ftable_get_frequency(&table, eof) == 1);
  TEST_ASSERT(ftable_get_low(&table, eof) == 5);
  TEST_ASSERT(ftable_get_high(&table, eof) == 6);
  ftable_deinit(&table);

  return NULL;
}

const char *test_ftable_halving() {
  ftable table;
  TEST_ASSERT(ftable_init(&table, test_context, 5, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF));

  ftable_increment(&table, 3);
  ftable_increment(&table, 3);
  ftable_increment(&table, 3);
  ftable_increment(&table, 3);
  ftable_increment(&table, 3);

  ftable_increment(&table, 2);
  ftable_increment(&table, 2);

  ftable_increment(&table, 0);

  TEST_ASSERT(ftable_get_frequency(&table, 0) == 2);
  TEST_ASSERT(ftable_get_frequency(&table, 2) == 3);
  TEST_ASSERT(ftable_get_frequency(&table, 3) == 6);
  TEST_ASSERT(ftable_get_low(&table, 3) == 6);
  TEST_ASSERT(ftable_get_high(&table, 3) == 12);

  ftable_halve_until_total_below_limit(&table, 10);
  TEST_ASSERT(ftable_get_frequency(&table, 0) == 1);
  TEST_ASSERT(ftable_get_frequency(&table, 2) == 2);
  TEST_ASSERT(ftable_get_frequency(&table, 3) == 3);
  TEST_ASSERT(ftable_get_low(&table, 3) == 4);
  TEST_ASSERT(ftable_get_high(&table, 3) == 7);

  ftable_deinit(&table);
  return NULL;
}

const char *test_ftable_from_frequencies() {
  symbol_frequency *freqs = calloc(5, sizeof(symbol_frequency));
  freqs[0] = 2;
  freqs[1] = 5;
  freqs[3] = 3;

  ftable table;

  TEST_ASSERT(ftable_init(&table, test_context, 5, FTABLE_INCLUDE_EOF));
  ftable_fill_from_frequencies(&table, freqs);
  symbol eof = ftable_get_eof_symbol(&table);
  TEST_ASSERT(ftable_get_frequency(&table, 3) == 3);
  TEST_ASSERT(ftable_get_low(&table, 3) == 7);
  TEST_ASSERT(ftable_get_high(&table, 3) == 10);
  TEST_ASSERT(eof == 5);
  TEST_ASSERT(ftable_get_frequency(&table, eof) == 1);
  TEST_ASSERT(ftable_get_low(&table, eof) == 10);
  TEST_ASSERT(ftable_get_high(&table, eof) == 11);
  ftable_deinit(&table);

  TEST_ASSERT(ftable_init(&table, test_context, 5, FTABLE_EXCLUDE_EOF));
  ftable_fill_from_frequencies(&table, freqs);
  TEST_ASSERT(ftable_get_frequency(&table, 3) == 3);
  TEST_ASSERT(ftable_get_low(&table, 3) == 7);
  TEST_ASSERT(ftable_get_high(&table, 3) == 10);
  ftable_deinit(&table);

  free(freqs);
  return NULL;
}

const char *test_ftable_allocation_failures() {
  ftable table;
  TEST_ASSERT(context_is_errored(test_context) == false);

  setup_test_context(0);
  TEST_ASSERT(!ftable_init(&table, test_context, 5, FTABLE_INCLUDE_EOF));
  TEST_ASSERT(context_is_errored(test_context) == true);

  setup_test_context(1);
  TEST_ASSERT(!ftable_init(&table, test_context, 5, FTABLE_INCLUDE_EOF));
  TEST_ASSERT(context_is_errored(test_context) == true);

  setup_test_context(2);
  TEST_ASSERT(ftable_init(&table, test_context, 5, FTABLE_INCLUDE_EOF));
  TEST_ASSERT(context_is_errored(test_context) == false);
  ftable_deinit(&table);

  return NULL;
}