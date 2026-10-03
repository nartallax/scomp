#pragma once
#include "../src/byte_stream_matcher.c"
#include "test_utils.c"

const char *test_bsm_basic() {
  byte_stream_matcher bsm;
  bsm_match match;
  ring_buffer src;
  TEST_ASSERT(bsm_init(&bsm, test_context));
  TEST_ASSERT(ring_buffer_init(&src, test_context, 16));

  for (byte b = 0; b < 20; b++) {
    bsm_push(&bsm, 255 - b);
  }

  ring_buffer_push(&src, 253);
  ring_buffer_push(&src, 252);
  ring_buffer_push(&src, 251);
  ring_buffer_push(&src, 250);
  ring_buffer_push(&src, 249);
  ring_buffer_push(&src, 3);

  // match found
  match = bsm_find_match(&bsm, &src, 0);
  TEST_ASSERT(match.length == 5 && match.offset == 18);

  // no match found
  match = bsm_find_match(&bsm, &src, 3);
  TEST_ASSERT(match.length == 0 && match.offset == 0);

  // matcher won't go past src buffer end
  ring_buffer_push(&src, 250);
  ring_buffer_push(&src, 249);
  ring_buffer_push(&src, 248);
  ring_buffer_push(&src, 247);
  ring_buffer_push(&src, 246);
  ring_buffer_push(&src, 245);
  match = bsm_find_match(&bsm, &src, 6);
  TEST_ASSERT(match.length == 6 && match.offset == 15);

  bsm_deinit(&bsm, test_context);
  ring_buffer_deinit(&src, test_context);
  return NULL;
}

const char *test_bsm_wraps() {
  byte_stream_matcher bsm;
  bsm_match match;
  ring_buffer src;
  TEST_ASSERT(bsm_init(&bsm, test_context));
  TEST_ASSERT(ring_buffer_init(&src, test_context, 3));

  // test for matcher array wrapping
  for (size_t i = 0; i < _BSM_LENGTH + 5; i++) {
    bsm_push(&bsm, (byte)(i & 0xff));
  }

  ring_buffer_push(&src, 254);
  ring_buffer_push(&src, 255);
  ring_buffer_push(&src, 0);
  ring_buffer_push(&src, 1);
  ring_buffer_push(&src, 2);
  ring_buffer_push(&src, 3);

  match = bsm_find_match(&bsm, &src, 0);
  TEST_ASSERT(match.length == 6 && match.offset == 7);

  // this should wrap src ring buffer
  ring_buffer_push(&src, 254);
  ring_buffer_push(&src, 255);
  ring_buffer_push(&src, 0);
  ring_buffer_push(&src, 1);
  ring_buffer_push(&src, 2);
  ring_buffer_push(&src, 3);
  ring_buffer_push(&src, 4);

  match = bsm_find_match(&bsm, &src, 6);
  TEST_ASSERT(match.length == 7 && match.offset == 7);

  ring_buffer_deinit(&src, test_context);
  TEST_ASSERT(ring_buffer_init(&src, test_context, 16));

  for (size_t i = 0; i < _BSM_MATCH_LENGTH_LIMIT + 5; i++) {
    ring_buffer_push(&src, (byte)(i & 0xff));
  }
  match = bsm_find_match(&bsm, &src, 0);
  // interesting effect, offset is less than length
  // because we went past matcher buffer end
  // I don't expect this to happen outside synthetic tests like this one
  TEST_ASSERT(match.length == _BSM_MATCH_LENGTH_LIMIT && match.offset == 5);

  bsm_deinit(&bsm, test_context);
  ring_buffer_deinit(&src, test_context);
  return NULL;
}

const char *test_bsm_allocation_failures() {
  byte_stream_matcher bsm;
  setup_test_context(0);
  TEST_ASSERT(!bsm_init(&bsm, test_context));
  setup_test_context(1);
  TEST_ASSERT(!bsm_init(&bsm, test_context));
  setup_test_context(2);
  TEST_ASSERT(!bsm_init(&bsm, test_context));
  setup_test_context(3);
  TEST_ASSERT(bsm_init(&bsm, test_context));
  bsm_deinit(&bsm, test_context);
  return NULL;
}