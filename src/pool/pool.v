module pool

import src.queue

/** A helper structure that allocates memory for some amount of objects at the same time, and helps to reuse that memory. */
pub struct Pool[T] {
mut:
	values &queue.Queue[&T]
}

pub fn new[T](initial_capacity_bits int) &Pool[T] {
	mut values := queue.new[&T](initial_capacity_bits)
	cap := values.get_capacity()
	for i := cap - 1; i >= 0; i-- {
		values.push_tail_fixed(&T{})
	}
	return &Pool[T]{
		values: values
	}
}

pub fn (mut p Pool[T]) acquire_fixed() &T {
	assert !p.values.is_empty(), 'Pool underflow, more acquires than allocations'
	return p.values.pop_tail()
}

pub fn (mut p Pool[T]) acquire_growable() &T {
	if (p.values.is_empty()) {
		return &T{}
	}
	return p.acquire_fixed()
}

pub fn (mut p Pool[T]) release_fixed(value &T) {
	assert !p.values.is_full(), 'Pool overflow, more releases than acquires'
	p.values.push_tail_fixed(value)
}

pub fn (mut p Pool[T]) release_growable(value &T) {
	p.values.push_tail_growable(value)
}
