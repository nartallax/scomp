#pragma once
#include "../context.c"
#include "../data_structures/byte_fixed_stack.c"
#include "json_base.c"

/** This is a finite state machine (with a stack) that describes JSON parsing states */
typedef struct {
  byte_fixed_stack state_stack;
  context *context;
} json_state_machine;

void jsm_deinit(json_state_machine *jsm, context *context) {
  bfstack_deinit(&jsm->state_stack, context);

  *jsm = (json_state_machine){0};
}

void jsm_reset(json_state_machine *jsm) {
  bfstack_reset(&jsm->state_stack);
  bfstack_push(&jsm->state_stack, JSON_STATE_ROOT);
  bfstack_push(&jsm->state_stack, JSON_STATE_START);
}

NODISCARD bool jsm_init(json_state_machine *jsm, context *context) {
  *jsm = (json_state_machine){0};

  jsm->context = context;

  if (!bfstack_init(&jsm->state_stack, context, JSON_MAX_STATE_STACK_LENGTH)) {
    jsm_deinit(jsm, context);
    return false;
  }

  jsm_reset(jsm);

  return true;
}

INLINE json_state jsm_get_current_state(json_state_machine *jsm) {
  return bfstack_peek(&jsm->state_stack);
}

// TODO: do we need this?
void _jsm_report_invalid_situation(json_state_machine *jsm, bool is_start, json_token_kind token_kind) {
  json_state state = jsm_get_current_state(jsm);
  context_set_error(jsm->context, "Impossible to %s token of kind %i at a state %i", is_start ? "start" : "end", token_kind, state);
}

NODISCARD INLINE bool _jsm_push_state(json_state_machine *jsm, json_state state) {
  if (bfstack_get_count(&jsm->state_stack) == bfstack_get_capacity(&jsm->state_stack)) {
    return false;
  }
  bfstack_push(&jsm->state_stack, state);
  return true;
}

NODISCARD INLINE bool _jsm_push_value_state(json_state_machine *jsm, json_state state) {
  if (bfstack_peek(&jsm->state_stack) == JSON_STATE_VALUE) {
    bfstack_pop(&jsm->state_stack);
  }
  return _jsm_push_state(jsm, state);
}

INLINE void _jsm_try_update_stack_top_after_reading_value(json_state_machine *jsm) {
  json_state base_state = jsm_get_current_state(jsm);
  if (base_state == JSON_STATE_ARRAY_START) {
    bfstack_replace(&jsm->state_stack, JSON_STATE_ARRAY_CONTINUE);
  } else if (base_state == JSON_STATE_OBJECT_START) {
    bfstack_replace(&jsm->state_stack, JSON_STATE_OBJECT_CONTINUE);
  }
}

INLINE void _jsm_pop_state_after_reading_value(json_state_machine *jsm) {
  bfstack_pop(&jsm->state_stack);
  _jsm_try_update_stack_top_after_reading_value(jsm);
}

NODISCARD bool _jsm_try_start_value(json_state_machine *jsm, json_token_kind token_kind) {
  switch (token_kind) {
  case JSON_TOKEN_STRING_DELIMITER:
    if (bfstack_peek(&jsm->state_stack) == JSON_STATE_VALUE) {
      bfstack_pop(&jsm->state_stack);
    }
    return _jsm_push_value_state(jsm, JSON_STATE_STRING);
  case JSON_TOKEN_OBJECT_OPEN:
    if (bfstack_peek(&jsm->state_stack) == JSON_STATE_VALUE) {
      bfstack_pop(&jsm->state_stack);
    }
    return _jsm_push_value_state(jsm, JSON_STATE_OBJECT_START);
  case JSON_TOKEN_ARRAY_OPEN:
    if (bfstack_peek(&jsm->state_stack) == JSON_STATE_VALUE) {
      bfstack_pop(&jsm->state_stack);
    }
    return _jsm_push_value_state(jsm, JSON_STATE_ARRAY_START);
  case JSON_TOKEN_WHITESPACE:
    return true;
  case JSON_TOKEN_NUMBER:
  case JSON_TOKEN_TRUE:
  case JSON_TOKEN_FALSE:
  case JSON_TOKEN_NULL:
    if (bfstack_peek(&jsm->state_stack) == JSON_STATE_VALUE) {
      bfstack_pop(&jsm->state_stack);
    }
    _jsm_try_update_stack_top_after_reading_value(jsm);
    return true;
  default:
    return false;
  }
}

NODISCARD bool jsm_process_token(json_state_machine *jsm, json_token_kind token_kind) {
  json_state state = jsm_get_current_state(jsm);
  switch (state) {
  case JSON_STATE_START:
    if (token_kind == JSON_TOKEN_BOM) {
      bfstack_replace(&jsm->state_stack, JSON_STATE_VALUE);
      return true;
    }
    // probably a value
    bfstack_replace(&jsm->state_stack, JSON_STATE_VALUE);
    return jsm_process_token(jsm, token_kind);
  case JSON_STATE_VALUE:
    return _jsm_try_start_value(jsm, token_kind);
  case JSON_STATE_STRING:
  case JSON_STATE_OBJECT_KEY:
    if (token_kind == JSON_TOKEN_STRING_DELIMITER) {
      if (state == JSON_STATE_OBJECT_KEY) {
        bfstack_replace(&jsm->state_stack, JSON_STATE_OBJECT_KV_SEPARATOR);
      } else {
        _jsm_pop_state_after_reading_value(jsm);
      }
      return true;
    } else {
      return token_kind == JSON_TOKEN_UNICODE_CHARACTER || token_kind == JSON_TOKEN_ESCAPE_SEQUENCE || token_kind == JSON_TOKEN_ESCAPED_UNICODE_CHARCODE;
    }
  case JSON_STATE_ARRAY_START:
    if (token_kind == JSON_TOKEN_ARRAY_CLOSE) {
      _jsm_pop_state_after_reading_value(jsm);
      return true;
    } else {
      return _jsm_try_start_value(jsm, token_kind);
    }
  case JSON_STATE_ARRAY_CONTINUE:
    if (token_kind == JSON_TOKEN_ARRAY_CLOSE) {
      _jsm_pop_state_after_reading_value(jsm);
      return true;
    } else if (token_kind == JSON_TOKEN_VALUE_SEPARATOR) {
      return _jsm_push_state(jsm, JSON_STATE_VALUE);
    } else {
      return token_kind == JSON_TOKEN_WHITESPACE;
    }
  case JSON_STATE_OBJECT_START:
    if (token_kind == JSON_TOKEN_OBJECT_CLOSE) {
      _jsm_pop_state_after_reading_value(jsm);
      return true;
    } else if (token_kind == JSON_TOKEN_STRING_DELIMITER) {
      return _jsm_push_state(jsm, JSON_STATE_OBJECT_KEY);
    } else {
      return token_kind == JSON_TOKEN_WHITESPACE;
    }
  case JSON_STATE_OBJECT_CONTINUE:
    if (token_kind == JSON_TOKEN_OBJECT_CLOSE) {
      _jsm_pop_state_after_reading_value(jsm);
      return true;
    } else if (token_kind == JSON_TOKEN_VALUE_SEPARATOR) {
      bool is_state_updated = _jsm_push_state(jsm, JSON_STATE_OBJECT_KEY_START);
      assert(is_state_updated && "This state push should be always successful, as we already did read a key for this object, and should be able to read another");
      return is_state_updated;
    } else {
      return token_kind == JSON_TOKEN_WHITESPACE;
    }
  case JSON_STATE_OBJECT_KEY_START:
    if (token_kind == JSON_TOKEN_STRING_DELIMITER) {
      bfstack_replace(&jsm->state_stack, JSON_STATE_OBJECT_KEY);
      return true;
    } else {
      return token_kind == JSON_TOKEN_WHITESPACE;
    }
  case JSON_STATE_OBJECT_KV_SEPARATOR:
    if (token_kind == JSON_TOKEN_KV_SEPARATOR) {
      bfstack_replace(&jsm->state_stack, JSON_STATE_VALUE);
      return true;
    } else {
      return token_kind == JSON_TOKEN_WHITESPACE;
    }
  case JSON_STATE_ROOT:
    return token_kind == JSON_TOKEN_WHITESPACE;
  default: // numbers, true, false, and other stuff that is not supposed to have more tokens in them
    return false;
  }
}