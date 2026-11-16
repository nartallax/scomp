module queue

fn test_growable_queue_simple() {
	mut q := queue.new_growable[int](2)
	q.push_tail(1)
	q.push_tail(2)
	q.push_tail(3)
	q.push_tail(4)
	assert q.count() == 4
	assert q.pop_head() == 1
	assert q.pop_head() == 2
	assert q.pop_head() == 3
	assert q.pop_head() == 4

	// wrapping
	q.push_tail(5)
	q.push_tail(6)
	q.push_tail(7)
	q.push_tail(8)
	q.push_tail(9)
	q.push_tail(10)
	q.push_tail(11)
	q.push_tail(12)
	assert q.pop_head() == 5
	assert q.pop_head() == 6
	assert q.pop_head() == 7
	assert q.pop_head() == 8
	assert q.pop_head() == 9
	assert q.pop_head() == 10
	assert q.pop_head() == 11
	assert q.pop_head() == 12
}
