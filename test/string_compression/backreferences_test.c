#pragma once
#include "../../src/string_compression/base.c"
#include "../test_utils.c"

const char *test_string_compression_backreference_write_data() {
  // for (size_t i = 0; i <= 128; i++) {
  //   printf("%zu %i\n", i, _strcomp_backreference_value_to_writeable_data(i).raw_bits_count);
  // }
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(3).raw_bits_count == 2);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(3).raw_bits_content == 0x3);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(4).raw_bits_count == 2);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(4).raw_bits_content == 0x0);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(11).raw_bits_count == 2);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(11).raw_bits_content == 0x7);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(12).raw_bits_count == 4);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(12).raw_bits_content == 0x0);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(27).raw_bits_count == 4);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(27).raw_bits_content == 0x0f);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(28).raw_bits_count == 5);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(28).raw_bits_content == 0x00);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(59).raw_bits_count == 5);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(59).raw_bits_content == 0x1f);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(60).raw_bits_count == 6);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(60).raw_bits_content == 0x00);
  // printf("%i\n", _strcomp_backreference_value_to_writeable_data(0xfff0).raw_bits_count);
  // printf("%zu\n", _strcomp_backreference_value_to_writeable_data(0xfff0).raw_bits_content);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(0xffff).raw_bits_count == 6);
  // TEST_ASSERT(_strcomp_backreference_value_to_writeable_data(0xffff).raw_bits_content == 0x00);

  return NULL;
}