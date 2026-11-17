module ring_buffer

fn test_ring_buffer_simple() {
	mut rb := ring_buffer.new(3)
	assert rb.get_capacity() == 8
	assert rb.get_position() == 0
	assert rb.get_byte(4) == 0

	rb.push_byte(2)
	assert rb.get_byte(0) == 2
	assert rb.get_position() == 1

	rb.push_byte(3)
	assert rb.get_byte(0) == 2
	assert rb.get_byte(1) == 3
	assert rb.get_position() == 2

	for i := 0; i < 7; i++ {
		rb.push_byte(4 + u8(i))
	}

	assert rb.get_position() == 1
	assert rb.get_byte(7) == 9
	assert rb.get_byte(0) == 10
	assert rb.get_byte(1) == 3
	assert rb.get_byte(-1) == 9
	assert rb.get_byte(15) == 9
	assert rb.get_byte(-8) == 10
	assert rb.get_byte(16) == 10
}

fn test_ring_buffer_fill() {
	mut rb := ring_buffer.new(3)

	mut src_array := []u8{}
	for i := u8(10); i < 100; i++ {
		src_array << i
	}

	mut consumed_bytes := rb.fill_with(src_array)
	assert consumed_bytes == 8
	assert rb.get_position() == 0
	assert rb.get_byte(0) == 10
	assert rb.get_byte(7) == 17

	rb.push_byte(1)
	rb.push_byte(1)
	assert rb.get_position() == 2
	consumed_bytes = rb.fill_with(src_array[8..])
	assert consumed_bytes == 8
	assert rb.get_byte(2) == 18
	assert rb.get_byte(7) == 23
	assert rb.get_byte(0) == 24
	assert rb.get_byte(1) == 25

	consumed_bytes = rb.fill_with(src_array[85..])
	assert consumed_bytes == 5
	assert rb.get_byte(1) == 25
	assert rb.get_byte(2) == 95
	assert rb.get_byte(6) == 99
	assert rb.get_byte(7) == 23
}
