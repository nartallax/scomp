module queue

pub struct Queue[T] {
mut:
	values        []T
	head          int
	tail          int
	capacity_mask int
}

pub fn new[T](initial_capacity_bits int) &Queue[T] {
	zero := T{}
	cap := 1 << initial_capacity_bits
	return &Queue[T]{
		capacity_mask: cap - 1
		head:          0
		tail:          0
		values:        []T{len: cap, cap: cap, init: zero}
	}
}

pub fn (q &Queue[T]) get_capacity() int {
	// allocated space is capacity_mask + 1
	// but we don't allow to fill that last spot
	// because then states "queue is empty" and "queue is full" would look exactly the same
	return q.capacity_mask
}

/// Returns amount of values in the queue
pub fn (q &Queue[T]) get_count() int {
	head := q.head
	mut tail := q.tail
	if head > tail {
		tail += q.capacity_mask + 1
	}
	return tail - head
}

pub fn (q &Queue[T]) is_full() bool {
	return q.get_count() >= q.get_capacity()
}

pub fn (q &Queue[T]) is_empty() bool {
	return q.get_count() == 0
}

/// Push a value to the tail of the queue. Fails if the queue is at its capacity.
fn (mut q Queue[T]) maybe_grow() {
	if !q.is_full() {
		return
	}
	start_len := q.values.len
	expected_len := (q.capacity_mask + 1) * 2
	for i := 0; q.values.len < expected_len; i++ {
		q.values << q.values[i]
	}
	if q.tail < q.head {
		q.head += start_len
	}
	q.capacity_mask = expected_len - 1
}

pub fn (mut q Queue[T]) push_tail_fixed(value T) {
	q.values[q.tail] = value
	q.tail = (q.tail + 1) & q.capacity_mask
	assert q.head != q.tail, 'Queue overflow'
}

/** Push a valut to the tail of the queue. Grows if the queue is at its capacity */
pub fn (mut q Queue[T]) push_tail_growable(value T) {
	q.maybe_grow()
	q.push_tail_fixed(value)
}

pub fn (mut q Queue[T]) push_head_fixed(value T) {
	q.head = (q.head + q.capacity_mask) & q.capacity_mask
	q.values[q.head] = value
	assert q.head != q.tail, 'Queue overflow'
}

pub fn (mut q Queue[T]) push_head_growable(value T) {
	q.maybe_grow()
	q.push_head_fixed(value)
}

pub fn (q &Queue[T]) peek_head_at_offset(offset int) T {
	assert offset < q.get_count()
	index := (q.head + offset) & q.capacity_mask
	return q.values[index]
}

pub fn (q &Queue[T]) peek_head() T {
	return q.peek_head_at_offset(0)
}

pub fn (q &Queue[T]) peek_tail_at_offset(offset int) T {
	assert offset < q.get_count()
	index := (q.tail - offset + q.capacity_mask) & q.capacity_mask
	return q.values[index]
}

pub fn (q &Queue[T]) peek_tail() T {
	return q.peek_tail_at_offset(0)
}

pub fn (mut q Queue[T]) pop_head() T {
	assert q.head != q.tail, 'Queue underflow'
	result := q.values[q.head]
	q.head = (q.head + 1) & q.capacity_mask
	return result
}

pub fn (mut q Queue[T]) pop_tail() T {
	assert q.head != q.tail, 'Queue underflow'
	q.tail = (q.tail + q.capacity_mask) & q.capacity_mask // -1 without the overflow
	result := q.values[q.tail]
	return result
}
