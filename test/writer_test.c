#pragma once

#include "./test_utils.c"
#include "../src/writer.c"

const char* test_writer_bytes(){
	writer* writer = writer_new(2, calloc(2, sizeof(byte)), calloc(2, sizeof(byte)));
	TEST_ASSERT(!writer_is_current_buffer_exhausted(writer), "Buffer should not be exhausted at zero");

	writer_write_byte(writer, 0b11001010);
	TEST_ASSERT(!writer_is_current_buffer_exhausted(writer), "Buffer should not be exhausted after one write");
	writer_write_byte(writer, 0b00110101);
	TEST_ASSERT(writer_is_current_buffer_exhausted(writer), "Buffer should be exhausted after two writes");
	
	writer_write_byte(writer, 0b11110000);
	TEST_ASSERT(writer_is_current_buffer_exhausted(writer), "Buffer should stay exhausted after three writes");
	
	byte* result_buffer = writer_rotate_buffers(writer, calloc(2, sizeof(byte)));
	TEST_ASSERT(result_buffer[0] == 0b11001010 && result_buffer[1] == 0b00110101, "First buffer should have expected data");
	TEST_ASSERT(!writer_is_current_buffer_exhausted(writer), "Buffer should not be exhausted after rotation");
	free(result_buffer);

	writer_write_byte(writer, 0b00001111);
	TEST_ASSERT(writer_is_current_buffer_exhausted(writer), "Buffer should be exhausted after four writes");
	
	result_buffer = writer_rotate_buffers(writer, calloc(2, sizeof(byte)));
	TEST_ASSERT(result_buffer[0] == 0b11110000 && result_buffer[1] == 0b00001111, "Second buffer should have expected data");
	free(result_buffer);

	TEST_ASSERT(!writer_is_current_buffer_exhausted(writer), "Buffer should not be exhausted after second rotation");
	
	writer_buffer last_buffer = writer_delete(writer);
	TEST_ASSERT(last_buffer.length == 0, "Last buffer is expected to be zero-length");
	free(last_buffer.buffer);

	return NULL;
}

void _write_byte_as_bits(writer* writer, byte value){
	for(int i = 0; i < 8; i++){
		byte bit = value & (1 << i)? 1: 0;
		writer_write_bit(writer, bit);
	}
}

const char* test_writer_bits(){
	writer* writer = writer_new(2, calloc(2, sizeof(byte)), calloc(2, sizeof(byte)));
	writer_write_bit(writer, 1);
	writer_write_bit(writer, 0);
	TEST_ASSERT(!writer_is_current_buffer_exhausted(writer), "Buffer should not be exhausted after 2 bits");
	
	_write_byte_as_bits(writer, 0b10101100);
	TEST_ASSERT(!writer_is_current_buffer_exhausted(writer), "Buffer should not be exhausted after 10 bits");
	
	_write_byte_as_bits(writer, 0b01010011);
	TEST_ASSERT(writer_is_current_buffer_exhausted(writer), "Buffer should be exhausted after 18 bits");

	byte* result_buffer = writer_rotate_buffers(writer, calloc(2, sizeof(byte)));
	TEST_ASSERT(result_buffer[0] == 0b10110001 && result_buffer[1] == 0b01001110, "First buffer should have expected data");
	free(result_buffer);
	TEST_ASSERT(!writer_is_current_buffer_exhausted(writer), "Buffer should not be exhausted after first rotation");
	
	_write_byte_as_bits(writer, 0b11111111);
	writer_write_bit(writer, 0);
	writer_write_bit(writer, 0);
	writer_write_bit(writer, 0);
	writer_write_bit(writer, 0);
	writer_write_bit(writer, 0);
	TEST_ASSERT(!writer_is_current_buffer_exhausted(writer), "Buffer should not be exhausted after 31 bits");
	writer_write_bit(writer, 0);
	TEST_ASSERT(writer_is_current_buffer_exhausted(writer), "Buffer should be exhausted after 32 bits");
	
	result_buffer = writer_rotate_buffers(writer, calloc(2, sizeof(byte)));
	TEST_ASSERT(result_buffer[0] == 0b11111101 && result_buffer[1] == 0b00000011, "Second buffer should have expected data");
	free(result_buffer);

	_write_byte_as_bits(writer, 0b11111111);
	writer_write_bit(writer, 0);
	writer_write_bit(writer, 1);
	writer_buffer last_buffer = writer_delete(writer);
	TEST_ASSERT(last_buffer.length == 2, "Last buffer should have length of 2");
	TEST_ASSERT(last_buffer.buffer[0] == 0b11111111 && last_buffer.buffer[1] == 0b00000010, "Last buffer should have expected data");
	free(last_buffer.buffer);

	return NULL;
}

const char* test_writer_wrong_init(){
	writer* writer = writer_new(0, NULL, NULL);
	TEST_ASSERT(writer == NULL, "Writer must not accept zero size");
	
	writer = writer_new(5, NULL, NULL);
	TEST_ASSERT(writer == NULL, "Writer must not accept non-power-of-two size");
	
	return NULL;
}