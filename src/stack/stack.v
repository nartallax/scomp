module stack

import queue

pub struct Stack[T] {
mut:
	// stack is very similar to a queue with less operations over its data
	// so (for now at least) stack IS a queue
	values queue.Queue[T]
}

pub fn new[T](initial_capacity_bits int) &Stack[T] {
	return &Stack[T]{
		values: queue.new[T](initial_capacity_bits)
	}
}

pub fn (mut s Stack[T]) push_fixed(value T) {
	s.values.push_tail_fixed(value)
}

pub fn (mut s Stack[T]) push_growable(value T) {
	s.values.push_tail_growable(value)
}

pub fn (mut s Stack[T]) pop() T {
	return s.values.pop_tail()
}

pub fn (s &Stack[T]) get_capacity() int {
	return s.values.get_capacity()
}

pub fn (s &Stack[T]) get_count() int {
	return s.values.get_count()
}

pub fn (s &Stack[T]) is_empty() bool {
	return s.values.is_empty()
}

pub fn (s &Stack[T]) is_full() bool {
	return s.values.is_full()
}
