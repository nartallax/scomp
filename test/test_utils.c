#pragma once
#define TEST_ASSERT(condition, comment)                                                                                                                                                                \
  do {                                                                                                                                                                                                 \
    if (!(condition)) {                                                                                                                                                                                \
      return (comment);                                                                                                                                                                                \
    }                                                                                                                                                                                                  \
  } while (0)