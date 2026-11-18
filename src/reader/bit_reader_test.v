module reader

fn test_bit_reader_simple() {
	mut br := new_bit_reader(3)

	assert br.fill_from([0x01, 0xff, 0xaa]) == 3
	assert br.get_available_bits() == 24

	assert br.read() == 1
	assert br.get_available_bits() == 23
	assert br.read() == 0
	assert br.read() == 0
	assert br.read() == 0

	assert br.get_available_bits() == 20
	assert br.read() == 0
	assert br.read() == 0
	assert br.read() == 0
	assert br.read() == 0
	assert br.get_available_bits() == 16

	assert br.read() == 1
	assert br.read() == 1
	assert br.read() == 1
	assert br.read() == 1
	assert br.get_available_bits() == 12

	assert br.read() == 1
	assert br.fill_from([0xbb]) == 1
	assert br.get_available_bits() == 19
	assert br.read() == 1
	assert br.read() == 1
	assert br.read() == 1
	assert br.get_available_bits() == 16

	assert br.read() == 0
	assert br.read() == 1
	assert br.read() == 0
	assert br.read() == 1
	assert br.read() == 0
	assert br.read() == 1
	assert br.read() == 0
	assert br.read() == 1
	assert br.get_available_bits() == 8

	assert br.read() == 1
	assert br.read() == 1
	assert br.read() == 0
	assert br.read() == 1
	assert br.read() == 1
	assert br.read() == 1
	assert br.read() == 0
	assert br.read() == 1
	assert br.get_available_bits() == 0
}
