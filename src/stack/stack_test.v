module stack

fn test_stack_simple() {
	mut s := stack.new[int](2)
	assert s.get_count() == 0
	assert s.get_capacity() == 3
	assert s.is_empty()
	assert !s.is_full()

	s.push_fixed(1)
	s.push_fixed(2)
	assert !s.is_empty()
	assert !s.is_full()
	assert s.get_count() == 2

	s.push_growable(3)
	assert !s.is_empty()
	assert s.is_full()

	assert s.pop() == 3
	assert s.pop() == 2
	assert s.pop() == 1
	assert s.get_count() == 0
	assert s.is_empty()
	assert !s.is_full()
}
