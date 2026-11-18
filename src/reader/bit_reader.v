module reader

pub struct BitReader {
	ByteReader
mut:
	current_byte u8
	bits_left    int
}

pub fn new_bit_reader(capacity_in_bytes_bits int) &BitReader {
	result := &BitReader{
		bits_left:    0
		current_byte: 0
	}
	result.ByteReader.init(capacity_in_bytes_bits)
	return result
}

pub fn (br &BitReader) get_available_bits() int {
	return (br.ByteReader.get_available_bytes() << 3) + br.bits_left
}

pub fn (mut br BitReader) read() u8 {
	if br.bits_left == 0 {
		br.current_byte = br.ByteReader.read()
		br.bits_left = 8
	}

	result := br.current_byte & 1
	br.bits_left--
	br.current_byte >>= 1
	return result
}
