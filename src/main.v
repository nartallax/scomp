module main

import src.reader

fn main() {
	br := reader.new_bit_reader(5)
	println(br)
}
