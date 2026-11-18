module reader

import src.ring_buffer
import math

pub struct ByteReader {
mut:
	buffer   ring_buffer.RingBuffer
	position int
}

pub fn new_byte_reader(capacity_bits int) &ByteReader {
	result := &ByteReader{}
	result.init(capacity_bits)
	return result
}

fn (mut br ByteReader) init(capacity_bits int) {
	br.buffer = ring_buffer.new(capacity_bits)
	br.position = 0
}

pub fn (br &ByteReader) get_capacity() int {
	// -1 to be able to distinguish between full and empty reader
	return br.buffer.get_capacity() - 1
}

pub fn (br &ByteReader) get_available_bytes() int {
	mut diff := br.buffer.get_position() - br.position
	if diff < 0 {
		diff += br.get_capacity()
	}
	return diff
}

pub fn (br &ByteReader) get_free_byte_space() int {
	return br.get_capacity() - br.get_available_bytes()
}

pub fn (mut br ByteReader) fill_from(src []u8) int {
	return br.buffer.fill_from(src[..math.min(src.len, br.get_free_byte_space())])
}

pub fn (mut br ByteReader) fill_from_raw(src &u8, length int) int {
	return br.buffer.fill_from_raw(src, math.min(length, br.get_free_byte_space()))
}

pub fn (mut br ByteReader) read() u8 {
	assert br.get_available_bytes() > 0, 'Reader underflow'
	result := br.buffer.get_byte(br.position)
	br.position++
	return result
}
