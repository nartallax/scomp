module queue

fn test_fixed_queue_simple() {
	mut q := queue.new_fixed[int](2)
	assert !q.is_full()
	assert q.is_empty()
	assert q.get_capacity() == 3

	q.push_tail(3)
	assert !q.is_full()
	assert !q.is_empty()
	assert q.peek_head() == 3
	assert q.peek_tail() == 3

	q.push_tail(4)
	assert !q.is_full()
	assert !q.is_empty()
	assert q.peek_head() == 3
	assert q.peek_tail() == 4

	q.push_tail(5)
	assert q.is_full()
	assert !q.is_empty()
	assert q.count() == 3
	assert q.peek_head() == 3
	assert q.peek_tail() == 5
	assert q.peek_head_at_offset(1) == 4
	assert q.peek_head_at_offset(2) == 5
	assert q.peek_tail_at_offset(1) == 4
	assert q.peek_tail_at_offset(2) == 3

	assert q.pop_head() == 3
	assert !q.is_full()
	assert q.count() == 2

	assert q.pop_head() == 4
	assert q.count() == 1

	q.push_tail(6)
	assert q.count() == 2
	assert !q.is_full()

	q.push_tail(7)
	assert q.count() == 3
	assert q.is_full()
	assert q.peek_head() == 5
	assert q.peek_tail() == 7
	assert q.peek_head_at_offset(1) == 6
	assert q.peek_head_at_offset(2) == 7
	assert q.peek_tail_at_offset(1) == 6
	assert q.peek_tail_at_offset(2) == 5

	assert q.pop_head() == 5
	assert q.count() == 2
	assert !q.is_full()
	assert q.pop_head() == 6
	assert q.count() == 1
	assert q.pop_head() == 7
	assert q.count() == 0
}

struct TestStruct {
mut:
	field int
}

fn test_fixed_queue_refs() {
	q := queue.new_fixed[&TestStruct](2)
	first := TestStruct{1}
	second := TestStruct{1}
	third := TestStruct{1}
	q.push_tail(&first)
	q.push_tail(&second)
	q.push_tail(&third)
	assert q.pop_head() == &first
	assert q.pop_head() == &second
	assert q.pop_head() == &third
}

fn test_fixed_queue_pop_tail() {
	mut q := queue.new_fixed[int](2)
	q.push_tail(1)
	q.push_tail(2)
	q.push_tail(3)
	assert q.count() == 3
	assert q.pop_tail() == 3
	assert q.pop_tail() == 2
	assert q.pop_tail() == 1
	assert q.count() == 0

	// wrapping
	q.push_tail(4)
	q.push_tail(5)
	q.push_tail(6)
	assert q.count() == 3
	assert q.pop_head() == 4
	assert q.pop_head() == 5
	assert q.count() == 1
	q.push_tail(7)
	q.push_tail(8)
	assert q.count() == 3
	assert q.pop_tail() == 8
	assert q.pop_tail() == 7
	assert q.pop_tail() == 6
	assert q.count() == 0
}
