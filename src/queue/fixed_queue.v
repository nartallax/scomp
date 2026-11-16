module queue

struct FixedQueue[T] {
mut:
	values        []T
	head          int
	tail          int
	capacity_mask int
}

pub fn new_fixed[T](capacity_bits int) &FixedQueue[T] {
	mut result := &FixedQueue[T]{}
	result.init(capacity_bits)
	return result
}

fn (mut q FixedQueue[T]) init(capacity_bits int) {
	cap := 1 << capacity_bits
	q.capacity_mask = cap - 1
	q.head = 0
	q.tail = 0
	q.values = []T{len: cap, cap: cap}
}

fn (q &FixedQueue[T]) get_capacity() int {
	// allocated space is capacity_mask + 1
	// but we don't allow to fill that last spot
	// because then states "queue is empty" and "queue is full" would look exactly the same
	return q.capacity_mask
}

/// Returns amount of values in the queue
fn (q &FixedQueue[T]) count() int {
	head := q.head
	mut tail := q.tail
	if head > tail {
		tail += q.capacity_mask + 1
	}
	return tail - head
}

fn (q &FixedQueue[T]) is_full() bool {
	return q.count() >= q.get_capacity()
}

fn (q &FixedQueue[T]) is_empty() bool {
	return q.count() == 0
}

fn (mut q FixedQueue[T]) push_tail(value T) {
	q.values[q.tail] = value
	q.tail = (q.tail + 1) & q.capacity_mask
	assert q.head != q.tail, 'Queue overflow'
}

fn (q &FixedQueue[T]) peek_head_at_offset(offset int) T {
	assert offset < q.count()
	index := (q.head + offset) & q.capacity_mask
	return q.values[index]
}

fn (q &FixedQueue[T]) peek_head() T {
	return q.peek_head_at_offset(0)
}

fn (q &FixedQueue[T]) peek_tail_at_offset(offset int) T {
	assert offset < q.count()
	index := (q.tail - offset + q.capacity_mask) & q.capacity_mask
	return q.values[index]
}

fn (q &FixedQueue[T]) peek_tail() T {
	return q.peek_tail_at_offset(0)
}

fn (mut q FixedQueue[T]) pop_head() T {
	assert q.head != q.tail, 'Queue underflow'
	result := q.values[q.head]
	q.head = (q.head + 1) & q.capacity_mask
	return result
}

fn (mut q FixedQueue[T]) pop_tail() T {
	assert q.head != q.tail, 'Queue underflow'
	q.tail = (q.tail + q.capacity_mask) & q.capacity_mask // -1 without the overflow
	result := q.values[q.tail]
	return result
}
