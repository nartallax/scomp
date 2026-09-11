#include "./arithmetic_coding_test.c"
#include "./fenwick_tree_test.c"
#include "./frequency_table_test.c"
#include "./math_test.c"
#include "./queue_test.c"
#include "./test_utils.c"
#include "./writer_test.c"
#include <stdio.h>

#define MAKE_TEST(fn) ((test){.tester = (fn), .name = #fn})

typedef const char *(*tester)(void);

typedef struct {
  tester tester;
  const char *name;
} test;

int main() {
  test tests[] = {
      MAKE_TEST(test_is_power_of_two),
      MAKE_TEST(test_fenwick_tree_simple),
      MAKE_TEST(test_fenwick_tree_max_range),
      MAKE_TEST(test_fenwick_tree_range_sum_cornercase),
      MAKE_TEST(test_fenwick_tree_allocation_failure),
      MAKE_TEST(test_ftable_simple),
      MAKE_TEST(test_ftable_with_eof),
      MAKE_TEST(test_ftable_init_one),
      MAKE_TEST(test_ftable_halving),
      MAKE_TEST(test_ftable_from_frequencies),
      MAKE_TEST(test_ftable_allocation_failures),
      MAKE_TEST(test_queue_simple),
      MAKE_TEST(test_queue_overflow_while_wrapping),
      MAKE_TEST(test_queue_overflow_while_not_wrapping),
      MAKE_TEST(test_queue_underflow),
      MAKE_TEST(test_queue_non_pointer_values),
      MAKE_TEST(test_queue_allocation_failures),
      MAKE_TEST(test_writer_bytes),
      MAKE_TEST(test_writer_bits),
      MAKE_TEST(test_writer_early_close),
      MAKE_TEST(test_writer_buffer_reuse),
      MAKE_TEST(test_writer_allocation_failure),
      MAKE_TEST(test_acod_simple),
      MAKE_TEST(test_acod_allocation_failures),
      // tests go here
  };

  size_t tester_count = sizeof tests / sizeof tests[0];
  size_t failed_tests = 0;
  for (size_t i = 0; i < tester_count; i++) {
    test current_test = tests[i];
    setup_test_context_default();
    const char *test_result = current_test.tester();
    if (test_result != NULL) {
      printf("Test \"%s\" failed: %s\n", current_test.name, test_result);
      failed_tests++;
    }
    free_test_context();
  }

  if (failed_tests > 0) {
    printf("Failed tests: %zu/%zu\n", failed_tests, tester_count);
    return 1;
  } else {
    printf("All tests passed.\n");
    return 0;
  }
}