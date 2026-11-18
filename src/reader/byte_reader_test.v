module reader

fn test_byte_reader_simple() {
	mut br := reader.new_byte_reader(3)
	assert br.get_available_bytes() == 0
	assert br.get_free_byte_space() == 7

	mut src_array := []u8{}
	for i := u8(10); i < 100; i++ {
		src_array << i
	}

	assert br.fill_from(src_array) == 7
	assert br.get_available_bytes() == 7
	assert br.get_free_byte_space() == 0

	assert br.fill_from(src_array) == 0
	assert br.get_available_bytes() == 7
	assert br.get_free_byte_space() == 0

	assert br.read() == 10
	assert br.read() == 11
	assert br.read() == 12

	unsafe {
		assert br.fill_from_raw(src_array[7..].data, src_array.len - 7) == 3
	}
	assert br.read() == 13
	assert br.read() == 14
	assert br.read() == 15
	assert br.read() == 16
	assert br.read() == 17
	assert br.read() == 18
	assert br.get_available_bytes() == 0
	assert br.get_free_byte_space() == 7

	unsafe {
		assert br.fill_from_raw(src_array[10..].data, 3) == 3
	}
	assert br.get_available_bytes() == 3
	assert br.get_free_byte_space() == 4
	assert br.read() == 19
	assert br.read() == 20
	assert br.read() == 21
	assert br.get_available_bytes() == 0
	assert br.get_free_byte_space() == 7
}
