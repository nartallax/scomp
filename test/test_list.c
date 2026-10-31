#pragma once
#include "arithmetic_coding/arithmetic_coding_test.c"
#include "arithmetic_coding/frequency_table_test.c"
#include "context_test.c"
#include "data_structures/arena_test.c"
#include "data_structures/byte_fixed_stack_test.c"
#include "data_structures/fenwick_tree_test.c"
#include "data_structures/fixed_queue_test.c"
#include "data_structures/queue_test.c"
#include "data_structures/reader_test.c"
#include "data_structures/ring_buffer_test.c"
#include "data_structures/stack_test.c"
#include "data_structures/writer_test.c"
#include "math_test.c"
#include "string_compression/byte_stream_matcher_test.c"
#include "string_compression/string_compressor_perf_test.c"
#include "string_compression/string_compressor_test.c"
#include "utf8_test.c"
#include "json/json_detokenizer_test.c"
#include "json/json_test_suite.c"
#include "json/json_tokenizer_test.c"

#define MAKE_TEST(fn) ((test){.tester = (fn), .name = #fn})

typedef const char *(*tester)(void);

typedef struct {
  tester tester;
  const char *name;
} test;

/*
TODO: rm this once tests are in place

ru_news_feed:
src: 						67132088		-
gzip -1:				14105463		0.545s
gzip -9:				11629087		1.491s  24.24%
brotli -q 1: 		11113255	 	0.137s
brotli -q 11: 	2896963 		58.262s

fda_recalls:
src:						29888764    -
gzip -1:				9042674			0.326s
gzip -9:				7246580			1.308s  17.32%
brotli -q 1:		7316679			0.100s
brotli -q 11:		2319899			29.397s
*/

test normal_tests[] = {
    MAKE_TEST(test_most_significant_bit),
    MAKE_TEST(test_context_double_error),
    MAKE_TEST(test_utf8_simple),
    MAKE_TEST(test_fenwick_tree_simple),
    MAKE_TEST(test_fenwick_tree_max_range),
    MAKE_TEST(test_fenwick_tree_range_sum_cornercase),
    MAKE_TEST(test_fenwick_tree_allocation_failure),
    MAKE_TEST(test_stack_simple),
    MAKE_TEST(test_stack_non_pointer),
    MAKE_TEST(test_stack_allocation_failures),
    MAKE_TEST(test_hbfstack_simple),
    MAKE_TEST(test_hbfstack_allocation_failures),
    MAKE_TEST(test_queue_simple),
    MAKE_TEST(test_queue_overflow_while_wrapping),
    MAKE_TEST(test_queue_overflow_while_not_wrapping),
    MAKE_TEST(test_queue_non_pointer_values),
    MAKE_TEST(test_queue_allocation_failures),
    MAKE_TEST(test_fixed_queue_simple),
    MAKE_TEST(test_fixed_queue_wrapping),
    MAKE_TEST(test_fixed_queue_allocation_failures),
    MAKE_TEST(test_arena_simple),
    MAKE_TEST(test_arena_one_big_allocation),
    MAKE_TEST(test_arena_allocation_failures),
    MAKE_TEST(test_ring_buffer_simple),
    MAKE_TEST(test_ring_buffer_allocation_fail),
    MAKE_TEST(test_writer_bytes),
    MAKE_TEST(test_writer_bits),
    MAKE_TEST(test_writer_buffer_reuse),
    MAKE_TEST(test_writer_buffer_resets),
    MAKE_TEST(test_writer_allocation_failure),
    MAKE_TEST(test_reader_simple),
    MAKE_TEST(test_reader_underflow),
    MAKE_TEST(test_reader_snapshots),
    MAKE_TEST(test_reader_allocation_failures),
    MAKE_TEST(test_ftable_simple),
    MAKE_TEST(test_ftable_with_eof),
    MAKE_TEST(test_ftable_init_one),
    MAKE_TEST(test_ftable_halving),
    MAKE_TEST(test_ftable_from_frequencies),
    MAKE_TEST(test_ftable_allocation_failures),
    MAKE_TEST(test_acod_simple),
    MAKE_TEST(test_json_tokenizer_string),
    MAKE_TEST(test_json_tokenizer_numbers),
    MAKE_TEST(test_json_tokenizer_constants),
    MAKE_TEST(test_json_tokenizer_array),
    MAKE_TEST(test_json_tokenizer_loooong_number),
    MAKE_TEST(test_json_tokenizer_object),
    MAKE_TEST(test_json_tokenizer_whitespaces),
    MAKE_TEST(test_json_tokenizer_nesting),
    MAKE_TEST(test_json_tokenizer_emptiness),
    MAKE_TEST(test_json_tokenizer_bom),
    MAKE_TEST(test_json_tokenizer_stack_limitations),
    MAKE_TEST(test_json_tokenizer_state_management),
    MAKE_TEST(test_json_tokenizer_broken_json_recovery),
    MAKE_TEST(test_json_tokenizer_stack_push_fails),
    MAKE_TEST(test_json_tokenizer_allocation_failures),
    MAKE_TEST(test_jsons_of_test_suite),
    MAKE_TEST(test_json_detokenizer_simple),
    MAKE_TEST(test_bsm_basic),
    MAKE_TEST(test_bsm_allocation_failures),
    MAKE_TEST(test_bsm_wraps),
    MAKE_TEST(test_string_compression_simple),
    MAKE_TEST(test_string_compression_wrapping),
    MAKE_TEST(test_string_compression_allocation_failures)
    // tests go here
};

test perf_tests[] = {MAKE_TEST(test_string_compressor_perf)};