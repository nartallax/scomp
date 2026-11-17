module pool

struct PoolTest {
mut:
	value int
}

fn test_pool_simple() {
	mut p := pool.new[PoolTest](2)
	mut first := p.acquire_fixed()
	first.value = 1
	mut second := p.acquire_fixed()
	second.value = 2
	mut third := p.acquire_growable()
	third.value = 3
	assert first.value == 1
	assert second.value == 2
	assert third.value == 3

	p.release_fixed(second)
	fourth := p.acquire_fixed()
	assert fourth.value == 2

	mut fifth := p.acquire_growable()
	fifth.value = 5
	assert first.value == 1
	assert fourth.value == 2
	assert third.value == 3

	p.release_growable(fifth)
	p.release_growable(fourth)
	p.release_growable(third)
	p.release_growable(first)
}
