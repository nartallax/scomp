module queue

struct GrowableQueue[T] {
	FixedQueue[T]
}

pub fn new_growable[T](capacity_bits int) GrowableQueue[T] {
	mut result := &GrowableQueue[T]{}
	result.FixedQueue.init(capacity_bits)
	return result
}

fn (mut q GrowableQueue[T]) grow() {
	start_len := q.values.len
	expected_len := (q.capacity_mask + 1) << 1
	for i := 0; q.values.len < expected_len; i++ {
		q.values << q.values[i]
	}
	if (q.tail < q.head) {
		q.head += start_len
	}
	q.capacity_mask = expected_len - 1
}

fn (mut q GrowableQueue[T]) push_tail(value T) {
	if (q.FixedQueue.is_full()) {
		q.grow()
	}
	q.FixedQueue.push_tail(value)
}
