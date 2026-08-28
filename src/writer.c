#pragma once

#include "commons.c"
#include "math.c"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

/** Writer is two buffers in a trenchcoat.

You are supposed to call write_something functions `while(!writer_is_current_buffer_exhausted())`,
after which you are supposed to provide fresh buffer via `writer_rotate_buffers()` and do something with full buffer returned.
At the end of the life you are supposed to call `writer_delete()`, which will return the last buffer. 

Note that buffers you provide are assumed to be zero-initialized on caller's side.*/
typedef struct {
	/** Byte size of one of the buffers. */
	size_t size;
	size_t size_mask;
	/** Current buffer to receive writes, or to read from */
	byte* current_buffer;
	/** Next buffer, which will receive writes/reads after current buffer is exhausted. */
	byte* next_buffer;
	/** Pointer to the next bit ready to be read/written to in current_buffer.
	If points beyond the end of current_buffer - then it points into next_buffer */
	size_t bit_pointer;
} writer;

writer* writer_new(size_t size, byte* current_buffer, byte* next_buffer){
	if(size < 1 || !is_power_of_two(size)){
		return NULL;
	}

	writer* stream = malloc(sizeof(writer));
	stream->size = size;
	stream->size_mask = (size << 3) - 1;
	stream->current_buffer = current_buffer;
	stream->next_buffer = next_buffer;
	stream->bit_pointer = 0;

	return stream;
}

typedef struct {
	byte* current_buffer;
	/** Amount of bytes in current_buffer that contain data written.
	If you didn't rotate the buffers before calling delete - this value may be larger than the buffer size */
	size_t length;
	/** This buffer should be clean if you rotated the buffers before deleting the writer */
	byte* next_buffer;
} writer_deletion_result;

/** Deletes the stream. Buffers are not deleted. Last partially-full buffer is returned. */
writer_deletion_result writer_delete(writer* writer){
	size_t bytes_written = (writer->bit_pointer >> 3);
	if(writer->bit_pointer & 7){
		bytes_written++;
	}

	writer_deletion_result result;
	result.current_buffer = writer->current_buffer;
	result.next_buffer = writer->next_buffer;
	result.length = bytes_written;
	
	free(writer);
	return result;
}

/** A check that indicates if it is time to rotate the buffers yet */
bool writer_is_current_buffer_exhausted(writer* writer){
	return (writer->bit_pointer >> 3) >= writer->size;
}

/** Provides new buffer for the writer. Exhausted buffer is returned. */
byte* writer_rotate_buffers(writer* writer, byte* fresh_buffer){
	byte* result = writer->current_buffer;
	writer->current_buffer = writer->next_buffer;
	writer->next_buffer = fresh_buffer;
	writer->bit_pointer -= writer->size << 3;
	return result;
}

byte* _writer_get_target_buffer(writer* writer){
	if((writer->bit_pointer >> 3) < writer->size){
		return writer->current_buffer;
	} else {
		return writer->next_buffer;
	}
}

void writer_write_byte(writer* writer, byte value){
	// usually streams are working either on bit level, or on byte level, without mixing
	assert((writer->bit_pointer & 7) == 0 && "No bit- and byte-level writer mixing!");

	byte* target = _writer_get_target_buffer(writer);
	size_t pointer = writer->bit_pointer & writer->size_mask;
	target[pointer >> 3] = value;
	
	writer->bit_pointer += 8;
}

void writer_write_bit(writer* writer, byte bit){
	byte* target = _writer_get_target_buffer(writer);
	size_t pointer = writer->bit_pointer & writer->size_mask;
	target[pointer >> 3] |= bit << (pointer & 7);

	writer->bit_pointer++;
}