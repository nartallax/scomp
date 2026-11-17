module queue

fn test_queue_simple() {
	mut q := queue.new[int](2)
	assert !q.is_full()
	assert q.is_empty()
	assert q.get_capacity() == 3

	q.push_tail_fixed(3)
	assert !q.is_full()
	assert !q.is_empty()
	assert q.peek_head() == 3
	assert q.peek_tail() == 3

	q.push_tail_fixed(4)
	assert !q.is_full()
	assert !q.is_empty()
	assert q.peek_head() == 3
	assert q.peek_tail() == 4

	q.push_tail_fixed(5)
	assert q.is_full()
	assert !q.is_empty()
	assert q.get_count() == 3
	assert q.peek_head() == 3
	assert q.peek_tail() == 5
	assert q.peek_head_at_offset(1) == 4
	assert q.peek_head_at_offset(2) == 5
	assert q.peek_tail_at_offset(1) == 4
	assert q.peek_tail_at_offset(2) == 3

	assert q.pop_head() == 3
	assert !q.is_full()
	assert q.get_count() == 2

	assert q.pop_head() == 4
	assert q.get_count() == 1

	q.push_tail_fixed(6)
	assert q.get_count() == 2
	assert !q.is_full()

	q.push_tail_fixed(7)
	assert q.get_count() == 3
	assert q.is_full()
	assert q.peek_head() == 5
	assert q.peek_tail() == 7
	assert q.peek_head_at_offset(1) == 6
	assert q.peek_head_at_offset(2) == 7
	assert q.peek_tail_at_offset(1) == 6
	assert q.peek_tail_at_offset(2) == 5

	assert q.pop_head() == 5
	assert q.get_count() == 2
	assert !q.is_full()
	assert q.pop_head() == 6
	assert q.get_count() == 1
	assert q.pop_head() == 7
	assert q.get_count() == 0
}

fn test_queue_refs() {
	q := queue.new[&TestStruct](2)
	first := TestStruct{1}
	second := TestStruct{1}
	third := TestStruct{1}
	q.push_tail_fixed(&first)
	q.push_tail_fixed(&second)
	q.push_tail_fixed(&third)
	assert q.pop_head() == &first
	assert q.pop_head() == &second
	assert q.pop_head() == &third
}

fn test_queue_pop_tail() {
	mut q := queue.new[int](2)
	q.push_tail_fixed(1)
	q.push_tail_fixed(2)
	q.push_tail_fixed(3)
	assert q.get_count() == 3
	assert q.pop_tail() == 3
	assert q.pop_tail() == 2
	assert q.pop_tail() == 1
	assert q.get_count() == 0

	// wrapping
	q.push_tail_fixed(4)
	q.push_tail_fixed(5)
	q.push_tail_fixed(6)
	assert q.get_count() == 3
	assert q.pop_head() == 4
	assert q.pop_head() == 5
	assert q.get_count() == 1
	q.push_tail_fixed(7)
	q.push_tail_fixed(8)
	assert q.get_count() == 3
	assert q.pop_tail() == 8
	assert q.pop_tail() == 7
	assert q.pop_tail() == 6
	assert q.get_count() == 0
}

fn test_queue_growth() {
	mut q := queue.new[int](2)
	q.push_tail_growable(1)
	q.push_tail_growable(2)
	q.push_tail_growable(3)
	q.push_tail_growable(4)
	assert q.get_count() == 4
	assert q.pop_head() == 1
	assert q.pop_head() == 2
	assert q.pop_head() == 3
	assert q.pop_head() == 4

	// wrapping
	q.push_tail_growable(5)
	q.push_tail_growable(6)
	q.push_tail_growable(7)
	q.push_tail_growable(8)
	q.push_tail_growable(9)
	q.push_tail_growable(10)
	q.push_tail_growable(11)
	q.push_tail_growable(12)
	assert q.pop_head() == 5
	assert q.pop_head() == 6
	assert q.pop_head() == 7
	assert q.pop_head() == 8
	assert q.pop_head() == 9
	assert q.pop_head() == 10
	assert q.pop_head() == 11
	assert q.pop_head() == 12
}

fn test_push_head() {
	mut q := queue.new[int](2)
	q.push_head_growable(1)
	q.push_head_growable(2)
	q.push_head_growable(3)
	assert q.peek_head() == 3
	assert q.peek_tail() == 1
	assert q.pop_head() == 3
	assert q.pop_head() == 2
	assert q.pop_head() == 1

	q.push_head_growable(4)
	q.push_head_growable(5)
	q.push_head_growable(6)
	q.push_head_growable(7)
	q.push_head_growable(8)
	q.push_tail_growable(9)
	assert q.peek_head() == 8
	assert q.peek_tail() == 9
	assert q.pop_head() == 8
	assert q.pop_head() == 7
	assert q.pop_head() == 6
	assert q.pop_head() == 5
	assert q.pop_head() == 4
	assert q.pop_head() == 9
}

struct TestStruct {
mut:
	field int
}
