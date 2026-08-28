#pragma once

#include "../src/writer.c"

const char* test_writer_bytes(){
	writer* writer = writer_new(2, calloc(2, sizeof(byte)), calloc(2, sizeof(byte)));
	if(writer_is_current_buffer_exhausted(writer)){
		return "Buffer exhausted at zero";
	}

	writer_write_byte(writer, 0b11001010);
	if(writer_is_current_buffer_exhausted(writer)){
		return "Buffer exhausted too early";
	}
	writer_write_byte(writer, 0b00110101);
	if(!writer_is_current_buffer_exhausted(writer)){
		return "Buffer is not exhausted after two writes";
	}

	writer_write_byte(writer, 0b11110000);
	if(!writer_is_current_buffer_exhausted(writer)){
		return "Buffer is not exhausted after three writes";
	}

	byte* result_buffer = writer_rotate_buffers(writer, calloc(2, sizeof(byte)));
	if(result_buffer[0] != 0b11001010 || result_buffer[1] != 0b00110101){
		return "Wrong data in the first buffer";
	}
	if(writer_is_current_buffer_exhausted(writer)){
		return "Buffer is exhausted after rotation";
	}
	free(result_buffer);

	writer_write_byte(writer, 0b00001111);
	if(!writer_is_current_buffer_exhausted(writer)){
		return "Buffer is not exhausted after four writes";
	}
	
	result_buffer = writer_rotate_buffers(writer, calloc(2, sizeof(byte)));
	if(result_buffer[0] != 0b11110000 || result_buffer[1] != 0b00001111){
		return "Wrong data in the second buffer";
	}
	free(result_buffer);
	if(writer_is_current_buffer_exhausted(writer)){
		return "Buffer is exhausted after second rotation";
	}

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
	if(writer_is_current_buffer_exhausted(writer)){
		return "Buffer is exhausted after 2 bits";
	}

	_write_byte_as_bits(writer, 0b10101100);
	if(writer_is_current_buffer_exhausted(writer)){
		return "Buffer is exhausted after 10 bits";
	}

	_write_byte_as_bits(writer, 0b01010011);
	if(!writer_is_current_buffer_exhausted(writer)){
		return "Buffer is not exhausted after 18 bits";
	}

	byte* result_buffer = writer_rotate_buffers(writer, calloc(2, sizeof(byte)));
	if(result_buffer[0] != 0b10110001 || result_buffer[1] != 0b01001110){
		return "Wrong data in the first buffer";
	}
	free(result_buffer);
	if(writer_is_current_buffer_exhausted(writer)){
		return "Buffer is exhausted after rotation";
	}

	_write_byte_as_bits(writer, 0b11111111);
	writer_write_bit(writer, 0);
	writer_write_bit(writer, 0);
	writer_write_bit(writer, 0);
	writer_write_bit(writer, 0);
	writer_write_bit(writer, 0);
	if(writer_is_current_buffer_exhausted(writer)){
		return "Buffer is exhausted after 31 bits";
	}
	writer_write_bit(writer, 0);
	if(!writer_is_current_buffer_exhausted(writer)){
		return "Buffer is not exhausted after 32 bits";
	}

	result_buffer = writer_rotate_buffers(writer, calloc(2, sizeof(byte)));
	if(result_buffer[0] != 0b11111101 || result_buffer[1] != 0b00000011){
		return "Wrong data in the second buffer";
	}
	free(result_buffer);

	return NULL;
}