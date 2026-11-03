#pragma once
#include "../../src/json/json_state_machine.c"
#include "../test_utils.c"

const char *test_jsm_returns_false_on_invalid_string_parts() {
  json_state_machine jsm;
  jsm_init(&jsm, test_context);
  TEST_ASSERT(jsm_process_token(&jsm, JSON_TOKEN_STRING_DELIMITER));
  TEST_ASSERT(!jsm_process_token(&jsm, JSON_TOKEN_ARRAY_OPEN));
  jsm_deinit(&jsm);
  return NULL;
}
