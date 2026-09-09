#pragma once
#include "../src/frequency_table.c"
#include "./test_utils.c"
#include <stdlib.h>

const char *test_ftable_simple() {
  ftable *table = ftable_new(test_context, 5, FTABLE_INIT_ZERO | FTABLE_EXCLUDE_EOF);
  TEST_ASSERT(ftable_get_frequency(table, 4) == 0, "By default frequencies must be zero");
  TEST_ASSERT(ftable_get_symbol_count(table) == 5, "Symbol count must be equal to the length passed");

  ftable_increment(table, 3);
  ftable_increment(table, 2);
  ftable_increment(table, 2);
  TEST_ASSERT(ftable_get_frequency(table, 3) == 1, "After one increments, frequency must be 1");
  TEST_ASSERT(ftable_get_frequency(table, 2) == 2, "After two increments, frequency must be 2");
  TEST_ASSERT(ftable_get_low(table, 2) == 0, "Low value of 2 must be 0");
  TEST_ASSERT(ftable_get_high(table, 2) == 2, "High value of 2 must be 2");
  TEST_ASSERT(ftable_get_low(table, 3) == 2, "Low value of 3 must be 2");
  TEST_ASSERT(ftable_get_high(table, 3) == 3, "High value of 3 must be 3");

  ftable_delete(table);
  return NULL;
}

const char *test_ftable_with_eof() {
  ftable *table = ftable_new(test_context, 5, FTABLE_INIT_ZERO | FTABLE_INCLUDE_EOF);
  symbol eof = ftable_get_eof_symbol(table);
  TEST_ASSERT(eof == 5, "EOF symbol must be equal to length");
  TEST_ASSERT(ftable_get_frequency(table, eof) == 1, "EOF frequency must always be 1");
  TEST_ASSERT(ftable_get_symbol_count(table) == 5, "Even with EOF, symbol count must return length passed in constructor");

  ftable_increment(table, 2);
  ftable_increment(table, 4);
  TEST_ASSERT(ftable_get_frequency(table, eof) == 1, "EOF frequency must always be 1, even after increments to other symbols");
  TEST_ASSERT(ftable_get_low(table, eof) == 2, "EOF low must be 2 after 2 increments");
  TEST_ASSERT(ftable_get_high(table, eof) == 3, "EOF high must be 3 after 2 increments");

  ftable_delete(table);
  return NULL;
}

const char *test_ftable_init_one() {
  ftable *table = ftable_new(test_context, 5, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF);
  TEST_ASSERT(ftable_get_frequency(table, 3) == 1, "Frequencies are 1 before any increments");
  TEST_ASSERT(ftable_get_low(table, 3) == 3, "Low for 3 is 3 before any increments");
  TEST_ASSERT(ftable_get_high(table, 3) == 4, "High for 3 is 4 before any increments");

  ftable_increment(table, 0);
  ftable_increment(table, 2);
  ftable_increment(table, 2);
  ftable_increment(table, 3);
  TEST_ASSERT(ftable_get_low(table, 3) == 6, "Low is incremented");
  TEST_ASSERT(ftable_get_high(table, 3) == 8, "High is incremented");
  ftable_delete(table);

  table = ftable_new(test_context, 5, FTABLE_INIT_ONE | FTABLE_INCLUDE_EOF);
  symbol eof = ftable_get_eof_symbol(table);
  TEST_ASSERT(ftable_get_frequency(table, eof) == 1, "EOF symbol is 1 even when 1-initing");
  TEST_ASSERT(ftable_get_low(table, eof) == 5, "Low for eof is 5 before any increments");
  TEST_ASSERT(ftable_get_high(table, eof) == 6, "High for eof is 6 before any increments");
  ftable_delete(table);

  return NULL;
}

const char *test_ftable_halving() {
  ftable *table = ftable_new(test_context, 5, FTABLE_INIT_ONE | FTABLE_EXCLUDE_EOF);

  ftable_increment(table, 3);
  ftable_increment(table, 3);
  ftable_increment(table, 3);
  ftable_increment(table, 3);
  ftable_increment(table, 3);

  ftable_increment(table, 2);
  ftable_increment(table, 2);

  ftable_increment(table, 0);

  TEST_ASSERT(ftable_get_frequency(table, 0) == 2, "Before halving, frequency of 0 is 2");
  TEST_ASSERT(ftable_get_frequency(table, 2) == 3, "Before halving, frequency of 2 is 3");
  TEST_ASSERT(ftable_get_frequency(table, 3) == 6, "Before halving, frequency of 3 is 6");
  TEST_ASSERT(ftable_get_low(table, 3) == 6, "Before halving, low of 3 is 6");
  TEST_ASSERT(ftable_get_high(table, 3) == 12, "Before halving, high of 3 is 12");

  ftable_halve_until_total_below_limit(table, 10);
  TEST_ASSERT(ftable_get_frequency(table, 0) == 1, "After halving, frequency of 0 is 1");
  TEST_ASSERT(ftable_get_frequency(table, 2) == 2, "After halving, frequency of 2 is 2");
  TEST_ASSERT(ftable_get_frequency(table, 3) == 3, "After halving, frequency of 3 is 3");
  TEST_ASSERT(ftable_get_low(table, 3) == 4, "After halving, low of 3 is 4");
  TEST_ASSERT(ftable_get_high(table, 3) == 7, "After halving, high of 3 is 7");

  ftable_delete(table);
  return NULL;
}

const char *test_ftable_from_frequencies() {
  symbol_frequency *freqs = calloc(5, sizeof(symbol_frequency));
  freqs[0] = 2;
  freqs[1] = 5;
  freqs[3] = 3;

  ftable *table;

  table = ftable_new(test_context, 5, FTABLE_INCLUDE_EOF);
  ftable_fill_from_frequencies(table, freqs);
  symbol eof = ftable_get_eof_symbol(table);
  TEST_ASSERT(ftable_get_frequency(table, 3) == 3, "Frequencies are preserved");
  TEST_ASSERT(ftable_get_low(table, 3) == 7, "Lows are calculated");
  TEST_ASSERT(ftable_get_high(table, 3) == 10, "Highs are calculated");
  TEST_ASSERT(eof == 5, "EOF is added");
  TEST_ASSERT(ftable_get_frequency(table, eof) == 1, "EOF frequency is 1");
  TEST_ASSERT(ftable_get_low(table, eof) == 10, "EOF low is 10");
  TEST_ASSERT(ftable_get_high(table, eof) == 11, "EOF high is 11");
  ftable_delete(table);

  table = ftable_new(test_context, 5, FTABLE_EXCLUDE_EOF);
  ftable_fill_from_frequencies(table, freqs);
  TEST_ASSERT(ftable_get_frequency(table, 3) == 3, "Frequencies are preserved");
  TEST_ASSERT(ftable_get_low(table, 3) == 7, "Lows are calculated");
  TEST_ASSERT(ftable_get_high(table, 3) == 10, "Highs are calculated");
  ftable_delete(table);

  free(freqs);
  return NULL;
}

const char *test_ftable_allocation_failures() {
  ftable *table;
  TEST_ASSERT(context_is_errored(test_context) == false, "Error must not be set at the test start");

  setup_test_context(0);
  table = ftable_new(test_context, 5, FTABLE_INCLUDE_EOF);
  TEST_ASSERT(context_is_errored(test_context) == true, "Error must be present when allocation fails");
  TEST_ASSERT(table == NULL, "Table must be null when allocation fails");

  setup_test_context(1);
  table = ftable_new(test_context, 5, FTABLE_INCLUDE_EOF);
  TEST_ASSERT(context_is_errored(test_context) == true, "Error must be present when allocation fails");
  TEST_ASSERT(table == NULL, "Table must be null when allocation fails");

  setup_test_context(2);
  table = ftable_new(test_context, 5, FTABLE_INCLUDE_EOF);
  TEST_ASSERT(context_is_errored(test_context) == true, "Error must be present when allocation fails");
  TEST_ASSERT(table == NULL, "Table must be null when allocation fails");

  setup_test_context(3);
  table = ftable_new(test_context, 5, FTABLE_INCLUDE_EOF);
  TEST_ASSERT(context_is_errored(test_context) == false, "Error must not be present after set number of allocations");
  TEST_ASSERT(table != NULL, "Table must not be null when allocation doesn't fail");
  ftable_delete(table);

  return NULL;
}