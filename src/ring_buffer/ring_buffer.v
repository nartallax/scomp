module ring_buffer

import math

/** Sequence of bytes that wraps around itself.
Doesn't distinguish between full and empty; all bytes in the buffer are treated as meaningful */
pub struct RingBuffer {
mut:
	buffer        []u8
	position      int
	position_mask int
}

pub fn new(size_bits int) &RingBuffer {
	size := 1 << size_bits
	return &RingBuffer{
		buffer:        []u8{len: size, cap: size}
		position:      0
		position_mask: size - 1
	}
}

pub fn (r &RingBuffer) get_capacity() int {
	return r.position_mask + 1
}

/** Returns position of the next byte that will be overwritten by the next push/fill */
pub fn (r &RingBuffer) get_position() int {
	return r.position
}

pub fn (mut r RingBuffer) push_byte(byte u8) {
	r.buffer[r.position] = byte
	r.position = (r.position + 1) & r.position_mask
}

/** Return a byte from the buffer by its index.
Offset can wrap around and go into negatives, but no more than one buffer's capacity */
pub fn (r &RingBuffer) get_byte(position int) u8 {
	fixed_position := (position + r.get_capacity()) & r.position_mask
	return r.buffer[fixed_position]
}

/** Copy bytes from provided C array into internal buffer until full.
Returns amount of bytes consumed. */
pub fn (mut r RingBuffer) fill_with_raw(raw_array &u8, len int) int {
	mut rem_src_len := len

	first_copy_amount := math.min(rem_src_len, r.get_capacity() - r.position)
	if first_copy_amount > 0 {
		unsafe {
			C.memcpy(&r.buffer[r.position], raw_array, first_copy_amount)
		}
		rem_src_len -= first_copy_amount
		r.position = (r.position + first_copy_amount) & r.position_mask
	}

	second_copy_amount := math.min(rem_src_len, r.get_capacity() - first_copy_amount)
	if second_copy_amount > 0 {
		unsafe {
			C.memcpy(r.buffer.data, &raw_array[first_copy_amount], second_copy_amount)
		}
		rem_src_len -= second_copy_amount
		r.position = (r.position + second_copy_amount) & r.position_mask
	}

	return len - rem_src_len
}

pub fn (mut r RingBuffer) fill_with(array []u8) int {
	unsafe {
		return r.fill_with_raw(array.data, array.len)
	}
}
