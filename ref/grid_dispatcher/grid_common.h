/*
Grid-based Task Dispatcher System

This software is a C++ 11 Header-Only reimplementation of core part from project PaintsNow.

The MIT License (MIT)

Copyright (c) 2014-2022 PaintDream

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <algorithm>
#include <memory>
#include <iterator>
#include <cmath>
#include <mutex>
#include <vector>
#include <cstring>
#include <functional>
#include <cstdlib>

namespace grid {
	template <typename atomic_t>
	class write_fence_t {
	public:
		write_fence_t(std::atomic<atomic_t>& var) noexcept : variable(var) {
			assert(variable.exchange(~(atomic_t)0, std::memory_order_acquire) == 0);
		}

		~write_fence_t() {
			assert(variable.exchange(0, std::memory_order_release) == ~(atomic_t)0);
		}

	private:
		std::atomic<atomic_t>& variable;
	};

	template <typename atomic_t>
	write_fence_t<atomic_t> write_fence(std::atomic<atomic_t>& variable) noexcept {
		return write_fence_t<atomic_t>(variable);
	}

	template <typename atomic_t>
	class read_fence_t {
	public:
		read_fence_t(std::atomic<atomic_t>& var) noexcept : variable(var) {
			assert(variable.fetch_add(1, std::memory_order_acquire) != ~(atomic_t)0);
		}

		~read_fence_t() {
			assert(variable.fetch_sub(1, std::memory_order_release) != ~(atomic_t)0);
		}

	private:
		std::atomic<atomic_t>& variable;
	};

	template <typename atomic_t>
	read_fence_t<atomic_t> read_fence(std::atomic<atomic_t>& variable) noexcept {
		return read_fence_t<atomic_t>(variable);
	}

	template <typename target_t, typename source_t>
	target_t verify_cast(source_t&& src) noexcept {
		target_t ret = static_cast<target_t>(src);
		assert(ret == src);
		return ret;
	}

	template <typename... args_t>
	struct placeholder {};

	template <size_t i>
	struct log2 : std::conditional<i / 2 != 0, std::integral_constant<size_t, 1 + log2<i / 2>::value>, std::integral_constant<size_t, 0>>::type {};

	template <>
	struct log2<0> : std::integral_constant<size_t, 0> {}; // let log2(0) == 0, only for template reduction compiling

	template <typename value_t>
	constexpr value_t get_alignment(value_t a) noexcept {
		return a & (~a + 1); // the same as a & -a, but no compiler warnings.
	}

	template <typename element_t, size_t block_size = 4096, template <typename...> class allocator_t = std::allocator, bool fence = true>
	class grid_queue_t {
	public:
		struct alignas(alignof(element_t)) storage_t {
			uint8_t data[sizeof(element_t)];
		};

		static_assert(block_size >= sizeof(element_t), "block_size is too small!");
		static constexpr size_t element_count = block_size / sizeof(element_t);
		static constexpr bool element_count_pow_two = ((size_t)1 << log2<element_count>::value) == element_count;
		static constexpr size_t counter_limit = element_count * ((size_t)1 << (sizeof(size_t) * 8 - 1 - log2<element_count>::value));
		static_assert((counter_limit & ((size_t)1 << (sizeof(size_t) * 8 - 1))) != 0, "not max limit!");
		using type = element_t;
		using node_allocator_t = allocator_t<storage_t>;

		grid_queue_t(const node_allocator_t& alloc, size_t init_count = 0) noexcept(noexcept(std::declval<node_allocator_t>().allocate(1))) : push_count(init_count), pop_count(init_count), ring_buffer(alloc.allocate(element_count)), allocator(alloc) {}

		grid_queue_t(size_t init_count = 0) noexcept(noexcept(std::declval<node_allocator_t>().allocate(1))) : push_count(init_count), pop_count(init_count), ring_buffer(allocator.allocate(element_count)) {
			new (ring_buffer) storage_t();
		}

		grid_queue_t(const grid_queue_t& rhs) = delete;
		grid_queue_t(grid_queue_t&& rhs) noexcept : allocator(rhs.allocator), ring_buffer(rhs.ring_buffer), push_count(rhs.push_count), pop_count(rhs.pop_count) {
			rhs.ring_buffer = nullptr;
		}

		grid_queue_t& operator = (const grid_queue_t& rhs) = delete;
		grid_queue_t& operator = (grid_queue_t&& rhs) noexcept {
			allocator = rhs.allocator;
			ring_buffer = rhs.ring_buffer;
			push_count = rhs.push_count;
			pop_count = rhs.pop_count;

			rhs.ring_buffer = nullptr;
			return *this;
		}

		~grid_queue_t() noexcept {
			if (ring_buffer != nullptr) {
				for_each([](element_t& e) noexcept { e.~element_t(); });
				allocator.deallocate(ring_buffer, element_count);
			}
		}

		template <typename input_element_t>
		element_t* push(input_element_t&& t) noexcept(noexcept(element_t(std::forward<input_element_t>(t)))) {
			if (full()) {
				return nullptr; // this queue is full, push failed
			}

			element_t* result = new (&ring_buffer[push_count % element_count]) element_t(std::forward<input_element_t>(t));

			// place thread_fence here to ensure that change of ring_buffer[push_index]
			//   must be visible to other threads after push_index updates.
			if /* constexpr */ (fence) {
				std::atomic_thread_fence(std::memory_order_release);
			}

			push_count = step_counter(push_count, 1);
			return result;
		}

		element_t& top() noexcept {
			assert(!empty()); // must checked before calling me (memory fence acquire implicited)
			return *reinterpret_cast<element_t*>(&ring_buffer[pop_count % element_count]);
		}

		const element_t& top() const noexcept {
			assert(!empty()); // must checked before calling me (memory fence acquire implicited)
			return *reinterpret_cast<const element_t*>(&ring_buffer[pop_count % element_count]);
		}

		element_t& get(size_t index) noexcept {
			assert(!empty()); // must checked before calling me (memory fence acquire implicited)
			return *reinterpret_cast<element_t*>(&ring_buffer[index % element_count]);
		}

		const element_t& get(size_t index) const noexcept {
			assert(!empty()); // must checked before calling me (memory fence acquire implicited)
			return *reinterpret_cast<const element_t*>(&ring_buffer[index % element_count]);
		}

		template <typename iterator_t>
		iterator_t push(iterator_t from, iterator_t to) noexcept(noexcept(element_t(*from))) {
			if (full()) {
				return from;
			}

			iterator_t org = from;
			size_t windex = push_count % element_count;
			size_t rindex = pop_count % element_count;

			if (rindex <= windex) {
				while (from != to && windex < element_count) {
					new (&ring_buffer[windex++]) element_t(*from++);
				}

				windex = 0;
			}

			while (from != to && windex < rindex) {
				new (&ring_buffer[windex++]) element_t(*from++);
			}

			if /* constexpr */ (fence) {
				std::atomic_thread_fence(std::memory_order_release);
			}

			push_count = step_counter(push_count, from - org);
			return from;
		}

		template <typename iterator_t>
		iterator_t pop(iterator_t from, iterator_t to) noexcept {
			if (empty()) {
				return from;
			}

			iterator_t org = from;
			size_t windex = push_count % element_count;
			size_t rindex = pop_count % element_count;

			if (windex <= rindex) {
				while (from != to && rindex < element_count) {
					element_t& element = *reinterpret_cast<element_t*>(&ring_buffer[rindex++]);
					*from++ = std::move(element);
					element.~element_t();
				}

				rindex = 0;
			}

			while (from != to && rindex < windex) {
				element_t& element = *reinterpret_cast<element_t*>(&ring_buffer[rindex++]);
				*from++ = std::move(element);
				element.~element_t();
			}

			if /* constexpr */ (fence) {
				std::atomic_thread_fence(std::memory_order_release);
			}

			pop_count = step_counter(pop_count, from - org);
			return from;
		}

		void pop() noexcept {
			assert(!empty());  // must checked before calling me (memory fence acquire implicited)
			reinterpret_cast<element_t*>(&ring_buffer[pop_count % element_count])->~element_t();

			if /* constexpr */ (fence) {
				std::atomic_thread_fence(std::memory_order_release);
			}

			pop_count = step_counter(pop_count, 1);
		}

		size_t pop(size_t n) noexcept {
			size_t m = std::min(n, size());
			size_t rindex = pop_count % element_count;
			size_t k = m;
			while (rindex < element_count && k != 0) {
				reinterpret_cast<element_t*>(&ring_buffer[rindex++])->~element_t();
				k--;
			}

			if (k != 0) {
				rindex = 0;
				do {
					reinterpret_cast<element_t*>(&ring_buffer[rindex++])->~element_t();
				} while (--k != 0);
			}

			if /* constexpr */ (fence) {
				std::atomic_thread_fence(std::memory_order_release);
			}

			pop_count = step_counter(pop_count, (ptrdiff_t)m);
			return n - m;
		}

		bool full() const noexcept {
			return step_counter(pop_count, (ptrdiff_t)element_count) == push_count;
		}

		bool empty() const noexcept {
			return pop_count == push_count;
		}

		size_t size() const noexcept {
			ptrdiff_t diff = diff_counter(push_count, pop_count);
			assert(diff >= 0);
			return (size_t)diff;
		}

		size_t pack_size(size_t alignment) const noexcept {
			assert(element_count >= alignment);
			size_t index = push_count + (size_t)(alignment - get_alignment(push_count % element_count)) & (alignment - 1);

			return std::min(std::max(index, pop_count + element_count) - index, element_count - (index % element_count));
		}

		template <typename operation_t>
		typename std::enable_if<std::is_constructible<std::function<void(element_t*, size_t)>, operation_t>::value>::type for_each(operation_t&& op) noexcept(noexcept(op(std::declval<element_t*>(), 0))) {
			if (empty())
				return;

			size_t windex = push_count % element_count;
			size_t rindex = pop_count % element_count;

			if (rindex >= windex) {
				size_t n = element_count - rindex;
				if (n != 0) {
					op(reinterpret_cast<element_t*>(ring_buffer) + rindex, n);
				}

				rindex = 0;
			}

			if (rindex < windex) {
				op(reinterpret_cast<element_t*>(ring_buffer) + rindex, windex - rindex);
			}
		}

		template <typename operation_t>
		typename std::enable_if<std::is_constructible<std::function<void(element_t&)>, operation_t>::value>::type for_each(operation_t&& op) noexcept(noexcept(op(std::declval<element_t&>()))) {
			if (empty())
				return;

			size_t windex = push_count % element_count;
			size_t rindex = pop_count % element_count;

			if (rindex >= windex) {
				while (rindex < element_count) {
					op(*reinterpret_cast<element_t*>(&ring_buffer[rindex++]));
				}

				rindex = 0;
			}

			while (rindex < windex) {
				op(*reinterpret_cast<element_t*>(&ring_buffer[rindex++]));
			}
		}

		template <typename operation_t>
		typename std::enable_if<std::is_constructible<std::function<void(const element_t*, size_t)>, operation_t>::value>::type for_each(operation_t&& op) const noexcept(noexcept(op(std::declval<const element_t*>(), 0))) {
			if (empty())
				return;

			size_t windex = push_count % element_count;
			size_t rindex = pop_count % element_count;

			if (rindex >= windex) {
				size_t n = element_count - rindex;
				if (n != 0) {
					op(reinterpret_cast<const element_t*>(ring_buffer) + rindex, n);
				}

				rindex = 0;
			}

			if (rindex < windex) {
				op(reinterpret_cast<const element_t*>(ring_buffer) + rindex, windex - rindex);
			}
		}

		template <typename operation_t>
		typename std::enable_if<std::is_constructible<std::function<void(const element_t&)>, operation_t>::value>::type for_each(operation_t&& op) const noexcept(noexcept(op(std::declval<const element_t&>()))) {
			if (empty())
				return;

			size_t windex = push_count % element_count;
			size_t rindex = pop_count % element_count;

			if (rindex >= windex) {
				while (rindex < element_count) {
					op(*reinterpret_cast<const element_t*>(&ring_buffer[rindex++]));
				}

				rindex = 0;
			}

			while (rindex < windex) {
				op(*reinterpret_cast<const element_t*>(&ring_buffer[rindex++]));
			}
		}

		element_t* allocate(size_t count, size_t alignment) {
			assert(count >= alignment);
			assert(count <= element_count);
			// make alignment
			size_t push_index = push_count % element_count;
			count += (size_t)(alignment - get_alignment(push_index)) & (alignment - 1);
			if (count > element_count - size()) return nullptr;

			size_t next_index = push_index + count;
			if (count != 1 && next_index > element_count) return nullptr; // non-continous!

			size_t ret_index = push_index;
			for (size_t i = push_index; i != next_index; i++) {
				new (&ring_buffer[i]) element_t();
			}

			if /* constexpr */ (fence) {
				std::atomic_thread_fence(std::memory_order_release);
			}

			push_count = step_counter(push_count, (ptrdiff_t)count);
			return reinterpret_cast<element_t*>(ring_buffer + ret_index);
		}

		void deallocate(size_t count, size_t alignment) noexcept {
			assert(count >= alignment);
			assert(count <= element_count);

			// make alignment
			size_t pop_index = pop_count % element_count;
			count += (size_t)(alignment - get_alignment(pop_index)) & (alignment - 1);
			assert(count <= size());

			size_t next_index = pop_index + count;

			for (size_t i = pop_index; i != next_index; i++) {
				reinterpret_cast<element_t*>(&ring_buffer[i])->~element_t();
			}

			if /* constexpr */ (fence) {
				std::atomic_thread_fence(std::memory_order_release);
			}

			pop_count = step_counter(pop_count, (ptrdiff_t)count);
		}

		void reset(size_t init_count) noexcept {
			if (ring_buffer != nullptr) {
				for_each([](element_t& e) { e.~element_t(); });
			}

			if /* constexpr */ (fence) {
				std::atomic_thread_fence(std::memory_order_release);
			}

			push_count = pop_count = init_count;
		}

		struct iterator {
			using difference_type = ptrdiff_t;
			using value_type = element_t;
			using reference = element_t&;
			using pointer = element_t*;
			using iterator_category = std::forward_iterator_tag;

			iterator(grid_queue_t* q, size_t i) noexcept : queue(q), index(i) {}

			iterator& operator ++ () noexcept {
				index = step_counter(index, 1);
				return *this;
			}

			iterator operator ++ (int) noexcept {
				return iterator(*queue, step_counter(index, 1));
			}

			iterator& operator += (ptrdiff_t count) noexcept {
				index = step_counter(index, count);
				return *this;
			}

			iterator operator + (ptrdiff_t count) const noexcept {
				return iterator(*queue, step_counter(index, count));
			}

			ptrdiff_t operator - (const iterator& it) const noexcept {
				return diff_counter(index, it.index);
			}

			bool operator == (const iterator& rhs) const noexcept {
				return index == rhs.index;
			}

			bool operator != (const iterator& rhs) const noexcept {
				return index != rhs.index;
			}

			element_t* operator -> () const noexcept {
				return reinterpret_cast<element_t*>(&queue->ring_buffer[index % element_count]);
			}

			element_t& operator * () const noexcept {
				return *reinterpret_cast<element_t*>(&queue->ring_buffer[index % element_count]);
			}

			friend struct const_iterator;

		private:
			size_t index;
			grid_queue_t* queue;
		};

		friend struct iterator;

		struct const_iterator {
			using difference_type = ptrdiff_t;
			using value_type = const element_t;
			using reference = const element_t&;
			using pointer = const element_t*;
			using iterator_category = std::forward_iterator_tag;

			const_iterator(const grid_queue_t* q, size_t i) noexcept : queue(q), index(i) {}

			const_iterator& operator ++ () noexcept {
				index = step_counter(index, 1);
				return *this;
			}

			const_iterator operator ++ (int) noexcept {
				return const_iterator(*queue, step_counter(index, 1));
			}

			const_iterator& operator += (ptrdiff_t count) noexcept {
				index = step_counter(index, count);
				return *this;
			}

			const_iterator operator + (ptrdiff_t count) const noexcept {
				return const_iterator(*queue, step_counter(index, count));
			}

			ptrdiff_t operator - (const const_iterator& it) const noexcept {
				return diff_counter(index, it.index);
			}

			bool operator == (const const_iterator& rhs) const noexcept {
				return index == rhs.index;
			}

			bool operator != (const const_iterator& rhs) const noexcept {
				return index != rhs.index;
			}

			const element_t* operator -> () const noexcept {
				return reinterpret_cast<const element_t*>(&queue->ring_buffer[index % element_count]);
			}

			const element_t& operator * () const noexcept {
				return *reinterpret_cast<const element_t*>(&queue->ring_buffer[index % element_count]);
			}

		private:
			size_t index;
			const grid_queue_t* queue;
		};

		friend struct const_iterator;

		iterator begin() noexcept {
			return iterator(this, pop_count);
		}

		iterator end() noexcept {
			return iterator(this, push_count);
		}

		const_iterator begin() const noexcept {
			return const_iterator(this, pop_count);
		}

		const_iterator end() const noexcept {
			return const_iterator(this, push_count);
		}

		size_t begin_index() const noexcept {
			return pop_count;
		}

		size_t end_index() const noexcept {
			return push_count;
		}

	public:
		static size_t step_counter(size_t count, ptrdiff_t delta) {
			size_t result = count + delta;
			if /* constexpr */ (!element_count_pow_two) {
				result = result >= counter_limit ? result - counter_limit : result; // cmov is faster than mod
			}

			return result;
		}

		static ptrdiff_t diff_counter(size_t lhs, size_t rhs) {
			if /* constexpr */ (element_count_pow_two) {
				return (ptrdiff_t)lhs - (ptrdiff_t)rhs;
			} else {
				ptrdiff_t diff = (ptrdiff_t)(lhs + counter_limit - rhs);
				return diff >= counter_limit ? diff - counter_limit : diff;  // cmov is faster than mod
			}
		}

	protected:
		node_allocator_t allocator;
		storage_t* ring_buffer;
		size_t push_count; // write count
		size_t pop_count; // read count
	};

	// chain kfifos to make variant capacity.
	template <typename element_t, size_t block_size = 4096, template <typename...> class allocator_t = std::allocator>
	class grid_queue_list_t {
	public:
		static constexpr size_t element_count = block_size / sizeof(element_t);
		using type = element_t;
		using sub_queue_t = grid_queue_t<element_t, block_size, allocator_t>;
		class node_t : public sub_queue_t {
		public:
			node_t(size_t init_count) : sub_queue_t(init_count), next(nullptr) {}
			node_t* next; // chain next queue
		};
		using node_allocator_t = allocator_t<node_t>;

	public:
		// do not copy this class, only to move
		grid_queue_list_t(const grid_queue_list_t& rhs) = delete;
		grid_queue_list_t& operator = (const grid_queue_list_t& rhs) = delete;

		grid_queue_list_t(const node_allocator_t& allocator) noexcept(noexcept(std::declval<node_allocator_t>().allocate(1))) : node_allocator(allocator), push_head(nullptr), pop_head(nullptr) {
			node_t* p = node_allocator.allocate(1);
			new (p) node_t(0);
			push_head = pop_head = p;
			iterator_counter = element_count;
		}

		grid_queue_list_t() noexcept(noexcept(std::declval<node_allocator_t>().allocate(1))) : push_head(nullptr), pop_head(nullptr) {
			node_t* p = node_allocator.allocate(1);
			new (p) node_t(0);
			push_head = pop_head = p;
			iterator_counter = element_count;
		}

		grid_queue_list_t(grid_queue_list_t&& rhs) noexcept {
			assert(node_allocator == rhs.node_allocator);
			push_head = rhs.push_head;
			pop_head = rhs.pop_head;
			iterator_counter = rhs.iterator_counter;

			rhs.push_head = nullptr;
			rhs.pop_head = nullptr;
		}

		grid_queue_list_t& operator = (grid_queue_list_t&& rhs) noexcept {
			assert(node_allocator == rhs.node_allocator);
			// just swap pointers.
			std::swap(push_head, rhs.push_head);
			std::swap(pop_head, rhs.pop_head);
			std::swap(iterator_counter, rhs.iterator_counter);

			return *this;
		}

		~grid_queue_list_t() {
			if (pop_head != nullptr) {
				node_t* q = pop_head;
				while (q != nullptr) {
					node_t* p = q;
					q = q->next;

					p->~node_t();
					node_allocator.deallocate(p, 1);
				}
			}
		}

		void preserve() noexcept(noexcept(std::declval<node_allocator_t>().allocate(1))) {
			if (push_head->full()) {
				node_t* p = node_allocator.allocate(1);
				new (p) node_t(iterator_counter);
				iterator_counter = node_t::step_counter(iterator_counter, element_count);

				// chain new node_t at head.
				push_head->next = p;
				push_head = p;
			}
		}

		template <typename input_element_t>
		element_t* push(input_element_t&& t) noexcept(noexcept(std::declval<grid_queue_list_t>().preserve()) && noexcept(std::declval<grid_queue_list_t>().emplace(std::forward<input_element_t>(t)))) {
			preserve();
			return emplace(std::forward<input_element_t>(t));
		}

		template <typename iterator_t>
		iterator_t push(iterator_t from, iterator_t to) noexcept(noexcept(std::declval<grid_queue_list_t>().push(*from))) {
			while (true) {
				iterator_t next = push_head->push(from, to);
				if (from == next)
					return next; // complete

				from = next;
				preserve();
			}
		}

		template <typename input_element_t>
		element_t* emplace(input_element_t&& t) noexcept(noexcept(std::declval<node_t>().push(std::forward<input_element_t>(t)))) {
			return push_head->push(std::forward<input_element_t>(t));
		}

		size_t end_index() const noexcept {
			return push_head->end_index();
		}

		size_t begin_index() const noexcept {
			return pop_head->begin_index();
		}

		const element_t& get(size_t index) const noexcept {
			for (const node_t* p = pop_head; p != push_head; p = p->next) {
				if (p->end_index() - index > 0) {
					return p->get(index);
				}
			}

			return push_head->get(index);
		}

		element_t& get(size_t index) noexcept {
			for (node_t* p = pop_head; p != push_head; p = p->next) {
				if (p->end_index() - index > 0) {
					return p->get(index);
				}
			}

			return push_head->get(index);
		}

		template <typename iterator_t>
		iterator_t pop(iterator_t from, iterator_t to) noexcept {
			while (true) {
				iterator_t next = pop_head->pop(from, to);
				if (from == next) {
					if (pop_head->empty() && pop_head != push_head) {
						node_t* p = pop_head;
						pop_head = pop_head->next;

						p->~node_t();
						node_allocator.deallocate(p, 1);
					}

					if (next == to)
						return next;
				}

				from = next;
			}
		}

		element_t& top() noexcept {
			return pop_head->top();
		}

		const element_t& top() const noexcept {
			return pop_head->top();
		}

		void pop() noexcept {
			pop_head->pop();

			// current queue is empty, remove it from list.
			if (pop_head->empty() && pop_head != push_head) {
				node_t* p = pop_head;
				pop_head = pop_head->next;

				p->~node_t();
				node_allocator.deallocate(p, 1);
			}
		}

		size_t pop(size_t n) noexcept {
			while (n != 0) {
				size_t m = std::min(n, pop_head->size());
				pop_head->pop(m);

				// current queue is empty, remove it from list.
				if (pop_head->empty() && pop_head != push_head) {
					node_t* p = pop_head;
					pop_head = pop_head->next;

					p->~node_t();
					node_allocator.deallocate(p, 1);

					n -= m;
				} else {
					break;
				}
			}

			return n;
		}

		bool empty() const noexcept {
			return pop_head->empty();
		}

		size_t size() const noexcept {
			size_t counter = 0;
			// sum up all sub queues
			for (node_t* p = pop_head; p != nullptr; p = p->next) {
				counter += p->size();
			}

			return counter;
		}

		size_t pack_size(size_t alignment) const noexcept {
			size_t v = push_head->pack_size(alignment);
			return v == 0 ? full_pack_size() : v;
		}

		static constexpr size_t full_pack_size() noexcept {
			return element_count;
		}

		element_t* allocate(size_t count, size_t alignment) {
			element_t* address;
			while ((address = push_head->allocate(count, alignment)) == nullptr) {
				if (push_head->next == nullptr) {
					node_t* p = node_allocator.allocate(1);
					new (p) node_t(iterator_counter);
					iterator_counter = node_t::step_counter(iterator_counter, element_count);

					address = p->allocate(count, alignment); // must success
					assert(address != nullptr);

					push_head->next = p;
					push_head = p;
					return address;
				}

				push_head = push_head->next;
			}

			return address;
		}

		void deallocate(size_t size, size_t alignment) noexcept {
			pop_head->deallocate(size, alignment);

			if (pop_head->empty() && pop_head != push_head) {
				node_t* p = pop_head;
				pop_head = pop_head->next;

				p->~node_t();
				node_allocator.deallocate(p, 1);
			}
		}

		void reset(size_t reserved) noexcept {
			node_t* p = push_head = pop_head;
			p->reset(0); // always reserved
			iterator_counter = element_count;

			node_t* q = p;
			p = p->next;

			while (p != nullptr && iterator_counter < reserved) {
				p->reset(iterator_counter);
				iterator_counter = node_t::step_counter(iterator_counter, element_count);
				q = p;
				p = p->next;
			}

			while (p != nullptr) {
				node_t* t = p;
				p = p->next;

				t->~node_t();
				node_allocator.deallocate(t, 1);
			}

			q->next = nullptr;
		}

		template <typename operation_t>
		void for_each(operation_t&& op) noexcept(noexcept(std::declval<node_t>().for_each(std::forward<operation_t>(op)))) {
			for (node_t* p = pop_head; p != nullptr; p = p->next) {
				p->for_each(std::forward<operation_t>(op));
			}
		}

		template <typename operation_t>
		void for_each(operation_t&& op) const noexcept(noexcept(std::declval<node_t>().for_each(std::forward<operation_t>(op)))) {
			for (node_t* p = pop_head; p != nullptr; p = p->next) {
				p->for_each(std::forward<operation_t>(op));
			}
		}

		template <typename operation_t>
		void for_each_queue(operation_t&& op) noexcept(noexcept(op(std::declval<grid_queue_list_t>().pop_head))) {
			for (node_t* p = pop_head; p != nullptr; p = p->next) {
				op(p);
			}
		}

		template <typename operation_t>
		void for_each_queue(operation_t&& op) const noexcept(noexcept(op(std::declval<grid_queue_list_t>().pop_head))) {
			for (const node_t* p = pop_head; p != nullptr; p = p->next) {
				op(p);
			}
		}

		struct iterator {
			using difference_type = ptrdiff_t;
			using value_type = element_t;
			using reference = element_t&;
			using pointer = element_t*;
			using iterator_category = std::forward_iterator_tag;

			iterator(node_t* n, size_t i) noexcept : current_node(n), it(i) {}

			iterator& operator ++ () noexcept {
				step();
				return *this;
			}

			iterator operator ++ (int) noexcept {
				iterator r = *this;
				step();

				return r;
			}

			iterator& operator += (ptrdiff_t count) noexcept {
				iterator p = *this + count;
				*this = std::move(p);
				return *this;
			}

			iterator operator + (ptrdiff_t count) const noexcept {
				node_t* n = current_node;
				size_t sub = it;
				while (true) {
					ptrdiff_t c = node_t::diff_counter(n->end_index(), sub);
					if (count >= c) {
						count -= c;
						node_t* t = n->next;
						if (t == nullptr) {
							return iterator(n, n->end_index());
						}

						n = t;
						sub = t->begin_index();
					} else {
						return iterator(n, node_t::step_counter(n->begin_index(), count));
					}
				}
			}

			ptrdiff_t operator - (const iterator& rhs) const noexcept {
				node_t* t = rhs.current_node;
				size_t sub = rhs.it;
				ptrdiff_t count = 0;

				while (t != current_node) {
					count += node_t::diff_counter(t->end_index(), sub);
					t = t->next;
					sub = t->begin_index();
				}

				count += node_t::diff_counter(it, sub);
				return count;
			}

			bool operator == (const iterator& rhs) const noexcept {
				return /* current_node == rhs.current_node && */ it == rhs.it;
			}

			bool operator != (const iterator& rhs) const noexcept {
				return /* current_node != rhs.current_node || */ it != rhs.it;
			}

			element_t* operator -> () const noexcept {
				return &current_node->get(it);
			}

			element_t& operator * () const noexcept {
				return current_node->get(it);
			}

			bool step() noexcept {
				it = node_t::step_counter(it, 1);
				if (it == current_node->end_index()) {
					node_t* n = current_node->next;
					if (n == nullptr) {
						return false;
					}

					current_node = n;
					it = n->begin_index();
				}

				return true;
			}

			friend struct const_iterator;

		private:
			size_t it;
			node_t* current_node;
		};

		friend struct iterator;

		struct const_iterator {
			using difference_type = ptrdiff_t;
			using value_type = element_t;
			using reference = element_t&;
			using pointer = element_t*;
			using iterator_category = std::forward_iterator_tag;

			const_iterator(const node_t* n, size_t i) noexcept : current_node(n), it(i) {}

			const_iterator& operator ++ () noexcept {
				step();
				return *this;
			}

			const_iterator operator ++ (int) noexcept {
				const_iterator r = *this;
				step();

				return r;
			}

			const_iterator& operator += (ptrdiff_t count) noexcept {
				const_iterator p = *this + count;
				*this = std::move(p);
				return *this;
			}

			const_iterator operator + (ptrdiff_t count) const noexcept {
				node_t* n = current_node;
				size_t sub = it;
				while (true) {
					ptrdiff_t c = node_t::diff_counter(n->end_index(), sub);
					if (count >= c) {
						count -= c;
						node_t* t = n->next;
						if (t == nullptr) {
							return const_iterator(n, n->end_index());
						}

						n = t;
						sub = t->begin_index();
					} else {
						return const_iterator(n, node_t::step_counter(n->begin_index(), count));
					}
				}
			}

			ptrdiff_t operator - (const const_iterator& rhs) const noexcept {
				const node_t* t = rhs.current_node;
				size_t sub = rhs.it;
				ptrdiff_t count = 0;

				while (t != current_node) {
					count += node_t::diff_counter(t->end_index(), sub);
					t = t->next;
					sub = t->begin_index();
				}

				count += node_t::diff_counter(it, sub);
				return count;
			}

			bool operator == (const const_iterator& rhs) const noexcept {
				return /* current_node == rhs.current_node && */ it == rhs.it;
			}

			bool operator != (const const_iterator& rhs) const noexcept {
				return /* current_node != rhs.current_node || */ it != rhs.it;
			}

			const element_t* operator -> () const noexcept {
				return &current_node->get(it);
			}

			const element_t& operator * () const noexcept {
				return current_node->get(it);
			}

			bool step() noexcept {
				it = node_t::step_counter(it, 1);
				if (it == current_node->end_index()) {
					const node_t* n = current_node->next;
					if (n == nullptr) {
						return false;
					}

					current_node = n;
					it = n->begin_index();
				}

				return true;
			}

		private:
			size_t it;
			const node_t* current_node;
		};

		friend struct const_iterator;

		iterator begin() noexcept {
			node_t* p = pop_head;
			return iterator(p, p->begin_index());
		}

		iterator end() noexcept {
			node_t* p = push_head;
			return iterator(p, p->end_index());
		}

		const_iterator begin() const noexcept {
			node_t* p = pop_head;
			return const_iterator(p, (static_cast<const node_t*>(p))->begin_index());
		}

		const_iterator end() const noexcept {
			const node_t* p = push_head;
			return const_iterator(p, p->end_index());
		}

	protected:
		node_t* push_head = nullptr;
		node_t* pop_head = nullptr; // pop_head is always prior to push_head.
		size_t iterator_counter = 0;
		node_allocator_t node_allocator;
	};

	// frame adapter for grid_queue_list_t
	template <typename grid_queue_t, size_t block_size = 4096, template <typename...> class allocator_t = std::allocator>
	class grid_queue_frame_t {
	public:
		grid_queue_frame_t(grid_queue_t& q) noexcept : queue(q), barrier(q.end()) {}

		using iterator = typename grid_queue_t::iterator;
		using const_iterator = typename grid_queue_t::const_iterator;

		iterator begin() noexcept(noexcept(std::declval<grid_queue_t>().begin())) {
			return queue.begin();
		}

		iterator end() noexcept {
			return barrier;
		}

		const_iterator begin() const noexcept(noexcept(std::declval<grid_queue_t>().begin())) {
			return queue.begin();
		}

		const_iterator end() const noexcept {
			return barrier;
		}

		size_t size() const noexcept {
			return static_cast<size_t>(end() - begin());
		}

		template <typename... args_t>
		void push(args_t&&... args) {
			queue.push(std::forward<args_t>(args)...);
		}

		template <typename iterator_t>
		iterator_t pop(iterator_t from, iterator_t to) noexcept {
			return queue.pop(from, to);
		}

		bool acquire() noexcept(noexcept(std::declval<grid_queue_t>().pop(1))) {
			queue.pop(barrier - begin());

			if (!frames.empty()) {
				barrier = frames.top();
				frames.pop();
				return true;
			} else {
				return false;
			}
		}

		void release() noexcept(noexcept(grid_queue_list_t<iterator, block_size>().push(std::declval<iterator>()))) {
			frames.push(queue.end());
		}

	protected:
		grid_queue_t& queue;
		iterator barrier;
		grid_queue_list_t<iterator, block_size, allocator_t> frames;
	};

	// binary find / insert / remove extension of std::vector<> like containers.
	template <typename key_t, typename value_t>
	struct key_value_t : public std::pair<key_t, value_t> {
		using base = std::pair<key_t, value_t>;
		template <typename key_args_t, typename value_args_t>
		key_value_t(key_args_t&& k, value_args_t&& v) : std::pair<key_t, value_t>(std::forward<key_args_t>(k), std::forward<value_args_t>(v)) {}
		key_value_t(const key_t& k) : std::pair<key_t, value_t>(k, value_t()) {}
		key_value_t() {}

		bool operator == (const key_value_t& rhs) const {
			return base::first == rhs.first;
		}

		bool operator < (const key_value_t& rhs) const {
			return base::first < rhs.first;
		}
	};

	template <typename key_t, typename value_t>
	key_value_t<typename std::decay<key_t>::type, typename std::decay<value_t>::type> make_key_value(key_t&& k, value_t&& v) {
		return key_value_t<typename std::decay<key_t>::type, typename std::decay<value_t>::type>(std::forward<key_t>(k), std::forward<value_t>(v));
	}

	template <typename iterator_t, typename value_t, typename pred_t>
	iterator_t binary_find(iterator_t begin, iterator_t end, value_t&& value, const pred_t& pred) {
		if (begin == end) return end;

		typename std::decay<decltype(*begin)>::type element(std::forward<value_t>(value));
		iterator_t it = std::lower_bound(begin, end, element, pred);
		return it != end && !pred(std::move(element), *it) ? it : end;
	}

	template <typename iterator_t, typename value_t>
	iterator_t binary_find(iterator_t begin, iterator_t end, value_t&& value) {
		if (begin == end) return end;

		typename std::decay<decltype(*begin)>::type element(std::forward<value_t>(value));
		iterator_t it = std::lower_bound(begin, end, element);
		return it != end && !(std::move(element) < *it) ? it : end;
	}

	template <typename container_t, typename value_t, typename pred_t>
	typename container_t::iterator binary_insert(container_t& container, value_t&& value, const pred_t& pred) {
		typename container_t::value_type element(std::forward<value_t>(value));
		typename container_t::iterator it = std::upper_bound(container.begin(), container.end(), element, pred);
		typename container_t::iterator ip = it;

		if (it != container.begin() && !pred(*--ip, element)) {
			*ip = std::move(element);
			return ip;
		} else {
			return container.insert(it, std::move(element));
		}
	}

	template <typename container_t, typename value_t>
	typename container_t::iterator binary_insert(container_t& container, value_t&& value) {
		typename container_t::value_type element(std::forward<value_t>(value));
		typename container_t::iterator it = std::upper_bound(container.begin(), container.end(), element);
		typename container_t::iterator ip = it;

		if (it != container.begin() && !(*--ip < element)) {
			*ip = std::move(element);
			return ip;
		} else {
			return container.insert(it, std::move(element));
		}
	}

	template <typename container_t, typename value_t, typename pred_t>
	bool binary_erase(container_t& container, value_t&& value, const pred_t& pred) {
		typename container_t::value_type element(std::forward<value_t>(value));
		typename container_t::iterator it = std::upper_bound(container.begin(), container.end(), element, pred);
		typename container_t::iterator ip = it;

		if (it != container.begin() && !pred(*--ip, std::move(element))) {
			container.erase(ip);
			return true;
		} else {
			return false;
		}
	}

	template <typename container_t, typename value_t>
	bool binary_erase(container_t& container, value_t&& value) {
		typename container_t::value_type element(std::forward<value_t>(value));
		typename container_t::iterator it = std::upper_bound(container.begin(), container.end(), element);
		typename container_t::iterator ip = it;

		if (it != container.begin() && !(*--ip < std::move(element))) {
			container.erase(ip);
			return true;
		} else {
			return false;
		}
	}

	inline uint32_t get_trailing_zeros(uint32_t value) {
		assert(value != 0);
#if defined(_MSC_VER)
		unsigned long index;
		_BitScanForward(&index, value);
		return index;
#else
		return __builtin_ctz(value);
#endif
	}

	inline uint32_t get_trailing_zeros(uint64_t value) {
		assert(value != 0);
#if defined(_MSC_VER)
#if !defined(_M_AMD64)
		uint32_t lowPart = (uint32_t)(value & 0xffffffff);
		return lowPart == 0 ? get_trailing_zeros((uint32_t)((value >> 31) >> 1)) + 32 : get_trailing_zeros(lowPart);
#else
		unsigned long index;
		_BitScanForward64(&index, value);
		return index;
#endif
#else
		return __builtin_ctzll(value);
#endif
	}

	template <typename value_t>
	uint32_t get_trailing_zeros_general(value_t value) {
		if /*constexpr*/ (sizeof(value_t) == sizeof(uint32_t)) {
			return get_trailing_zeros((uint32_t)value);
		} else {
			return get_trailing_zeros((uint64_t)value);
		}
	}

	inline void* alloc_aligned(size_t size, size_t alignment) {
#ifdef _MSC_VER
		return _aligned_malloc(size, alignment);
#else
		void* p = nullptr;
		posix_memalign(&p, alignment, size);
		return p;
#endif
	}

	inline void free_aligned(void* data, size_t size) {
#ifdef _MSC_VER
		_aligned_free(data);
#else
		free(data);
#endif
	}

	// global allocator that allocates memory blocks to local allocators.
	template <size_t byte_count, size_t k>
	class grid_root_allocator_t {
	public:
		grid_root_allocator_t() {}
		~grid_root_allocator_t() {
			assert(blocks.empty());
		}

		enum { bitmap_count = (k + sizeof(size_t) * 8 - 1) / (sizeof(size_t) * 8) };

		void* allocate() {
			// do fast operations in critical section
			do {
				std::lock_guard<std::mutex> guard(lock);
				for (size_t i = 0; i < blocks.size(); i++) {
					block_t& block = blocks[i];
					for (size_t n = 0; n < bitmap_count; n++) {
						size_t& bitmap = block.bitmap[n];
						size_t bit = bitmap + 1;
						bit = bit & (~bit + 1);
						if (bit != 0) {
							size_t index = get_trailing_zeros_general(bit) + n * sizeof(size_t) * 8;
							if (index < byte_count) {
								bitmap |= bit;
								return block.address + (n * sizeof(size_t) * 8 + index) * byte_count;
							}
						}
					}
				}
			} while (false);

			// real allocation, release the critical.
			block_t block;
			block.address = reinterpret_cast<uint8_t*>(alloc_aligned(byte_count * k, byte_count));
			memset(block.bitmap, 0, sizeof(block.bitmap));
			block.bitmap[0] = 1;

			// write result back
			do {
				std::lock_guard<std::mutex> guard(lock);
				blocks.emplace_back(block);
			} while (false);

			return block.address;
		}

		void deallocate(void* p) {
			void* t = nullptr;

			do {
				std::lock_guard<std::mutex> guard(lock);

				// loop to find required one.
				for (size_t i = 0; i < blocks.size(); i++) {
					block_t& block = blocks[i];
					if (p >= block.address && p < block.address + byte_count * k) {
						size_t index = (reinterpret_cast<uint8_t*>(p) - block.address) / byte_count;
						size_t page = index / (sizeof(size_t) * 8);
						size_t offset = index & (sizeof(size_t) * 8 - 1);
						assert(page < bitmap_count);
						size_t& bitmap = block.bitmap[page];
						bitmap &= ~((size_t)1 << offset);

						if (bitmap == 0) {
							size_t n;
							for (n = 0; n < bitmap_count; n++) {
								if (block.bitmap[n] != 0)
									break;
							}

							if (n == bitmap_count) {
								t = block.address;
								blocks.erase(blocks.begin() + i);
							}
						}

						break;
					}
				}
			} while (false);

			if (t != nullptr) {
				// do free
				free_aligned(t, byte_count * k);
			}
		}

		// we are not dll-friendly, as always.
		static grid_root_allocator_t& get() {
			static grid_root_allocator_t instance;
			return instance;
		}

	protected:
		struct block_t {
			uint8_t* address;
			size_t bitmap[bitmap_count];
		};

		std::mutex lock;
		std::vector<block_t> blocks;
	};

	// local allocator, allocate memory with specified alignment requirements.
	// k = element size, m = block size, r = max recycled block count, 0 for not limited, w = control block count
	template <size_t k, size_t m = 4096, size_t r = 8, size_t s = 16, size_t w = 4>
	class grid_allocator_t {
	public:
		enum {
			block_size = m,
			item_count = m / k,
			bits = 8 * sizeof(size_t),
			bitmap_block_size = (item_count + bits - 1) / bits,
			mask = bits - 1
		};

		class control_block_t {
		public:
			grid_allocator_t* allocator;
			control_block_t* next;
			std::atomic<uint32_t> ref_count;
			std::atomic<uint32_t> managed;
			std::atomic<size_t> bitmap[bitmap_block_size];
		};

		enum {
			offset = (sizeof(control_block_t) + k - 1) / k
		};

	public:
		grid_allocator_t() {
			static_assert(item_count / 2 * k > sizeof(control_block_t), "item_count is too small");
			recycle_count.store(0, std::memory_order_relaxed);
			for (size_t n = 0; n < sizeof(control_blocks) / sizeof(control_blocks[0]); n++) {
				control_blocks[n].store(nullptr, std::memory_order_relaxed);
			}

			recycled_head.store(nullptr, std::memory_order_release);
		}

		static grid_root_allocator_t<m, s>& get_root_allocator() {
			return grid_root_allocator_t<m, s>::get();
		}

		~grid_allocator_t() {
			// deallocate all caches
			grid_root_allocator_t<m, s>& allocator = get_root_allocator();

			for (size_t n = 0; n < sizeof(control_blocks) / sizeof(control_blocks[0]); n++) {
				control_block_t* p = (control_block_t*)control_blocks[n].load(std::memory_order_acquire);
				if (p != nullptr) {
					allocator.deallocate(p);
				}
			}

			control_block_t* p = recycled_head.load(std::memory_order_acquire);
			while (p != nullptr) {
				control_block_t* t = p->next;
				allocator.deallocate(p);
				p = t;
			}
		}

		void* allocate() {
			while (true) {
				control_block_t* p = nullptr;
				for (size_t n = 0; p == nullptr && n < sizeof(control_blocks) / sizeof(control_blocks[0]); n++) {
					p = control_blocks[n].exchange(nullptr, std::memory_order_acquire);
				}

				if (p == nullptr) {
					// need a new block
					p = recycled_head.exchange(nullptr, std::memory_order_acquire);
					if (p != nullptr) {
						control_block_t* t = p->next;
						control_block_t* expected = nullptr;
						if (!recycled_head.compare_exchange_strong(expected, t, std::memory_order_release, std::memory_order_relaxed)) {
							while (t != nullptr) {
								control_block_t* q = t->next;
								control_block_t* h = recycled_head.load(std::memory_order_relaxed);
								do {
									t->next = h;
								} while (!recycled_head.compare_exchange_weak(h, t, std::memory_order_release, std::memory_order_relaxed));

								t = q;
							}
						}

						p->next = nullptr;
						assert(p->ref_count.load(std::memory_order_acquire) >= 1);
						recycle_count.fetch_sub(1, std::memory_order_relaxed);
						assert(p->managed.load(std::memory_order_acquire) == 1);
						p->managed.store(0, std::memory_order_release);
					} else {
						p = reinterpret_cast<control_block_t*>(get_root_allocator().allocate());
						memset(p, 0, sizeof(control_block_t));
						p->next = nullptr;
						p->allocator = this;
						p->ref_count.store(1, std::memory_order_relaxed); // newly allocated one, just set it to 1
					}
				} else {
					p->managed.store(0, std::memory_order_release);
				}

				// search for an empty slot
				for (size_t n = 0; n < bitmap_block_size; n++) {
					std::atomic<size_t>& b = p->bitmap[n];
					size_t mask = b.load(std::memory_order_acquire);
					if (mask != ~(size_t)0) {
						size_t bit = get_alignment(mask + 1);
						if (!(b.fetch_or(bit, std::memory_order_relaxed) & bit)) {
							// get index of bitmap
							size_t index = get_trailing_zeros_general(bit) + offset + n * 8 * sizeof(size_t);
							if (index < item_count) {
								p->ref_count.fetch_add(1, std::memory_order_relaxed);
								// add to recycle system if needed
								recycle(p);

								return reinterpret_cast<char*>(p) + index * k;
							}
						}
					}
				}

				// full?
				try_free(p);
			}

			assert(false);
			return nullptr; // never reach here
		}

		static void deallocate(void* ptr) {
			size_t t = reinterpret_cast<size_t>(ptr);
			control_block_t* p = reinterpret_cast<control_block_t*>(t & ~(block_size - 1));
			size_t id = (t - (size_t)p) / k - offset;
			p->allocator->deallocate(p, id);
		}

	protected:
		void try_free(control_block_t* p) {
			assert(p->ref_count.load(std::memory_order_acquire) != 0);
			if (p->ref_count.fetch_sub(1, std::memory_order_release) == 1) {
				for (size_t n = 0; n < sizeof(control_blocks) / sizeof(control_blocks[0]); n++) {
					assert(control_blocks[n].load(std::memory_order_acquire) != p);
				}

				get_root_allocator().deallocate(p);
			}
		}

		void recycle(control_block_t* p) {
			assert(p->ref_count.load(std::memory_order_acquire) != 0);

			// search for recycled
			if (p->managed.load(std::memory_order_acquire) == 0 && recycle_count < r && p->managed.exchange(1, std::memory_order_acquire) == 0) {
				for (size_t n = 0; n < sizeof(control_blocks) / sizeof(control_blocks[0]); n++) {
					control_block_t* expected = nullptr;
					if (control_blocks[n].compare_exchange_weak(expected, p, std::memory_order_release, std::memory_order_relaxed)) {
						return;
					}
				}

				recycle_count.fetch_add(1, std::memory_order_relaxed);

				assert(p->next == nullptr);
				control_block_t* h = recycled_head.load(std::memory_order_relaxed);
				do {
					p->next = h;
				} while (!recycled_head.compare_exchange_weak(h, p, std::memory_order_release, std::memory_order_relaxed));
			} else {
				try_free(p);
			}
		}

		void deallocate(control_block_t* p, size_t id) {
			assert(p->allocator != nullptr);
			p->bitmap[id / bits].fetch_and(~((size_t)1 << (id & mask)));

			recycle(p);
		}

	protected:
		std::atomic<control_block_t*> control_blocks[w];
		std::atomic<control_block_t*> recycled_head;
		std::atomic<size_t> recycle_count;
	};

	template <typename element_t, size_t block_size = 4096>
	class grid_object_allocator_t : public grid_allocator_t<sizeof(element_t), block_size> {
	public:
		using value_type = element_t;
		using pointer = element_t*;
		using const_pointer = const element_t*;
		using reference = element_t&;
		using const_reference = const element_t&;
		using size_type = size_t;
		using difference_type = ptrdiff_t;
		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal = std::false_type;

		template <class morph_t>
		struct rebind { using other = grid_object_allocator_t<morph_t, block_size>; };
		using allocator_t = grid_allocator_t<sizeof(element_t), block_size>;

		element_t* allocate(size_t n) {
			assert(n == 1);
			return reinterpret_cast<element_t*>(allocator_t::allocate());
		}

		template <typename... args_t>
		void construct(element_t* p, args_t&&... args) {
			new (p) element_t(std::forward<args_t>(args)...);
		}

		void destroy(element_t* p) {
			p->~element_t();
		}

		void deallocate(element_t* p, size_t n) {
			assert(n == 1);
			allocator_t::deallocate(p);
		}
	};

	template <typename element_t, size_t block_size = 4096, size_t page_size = block_size * 16, template <typename...> class single_allocator_t = std::allocator>
	class grid_block_allocator_t : public single_allocator_t<element_t> {
	public:
		using value_type = element_t;
		using pointer = element_t*;
		using const_pointer = const element_t*;
		using reference = element_t&;
		using const_reference = const element_t&;
		using size_type = size_t;
		using difference_type = ptrdiff_t;
		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal = std::false_type;

		template <class morph_t>
		struct rebind { using other = grid_block_allocator_t<morph_t, block_size, page_size>; };
		using allocator_t = grid_root_allocator_t<block_size, page_size>;

		element_t* allocate(size_t n) {
			if (n == block_size / sizeof(element_t)) {
				return reinterpret_cast<element_t*>(allocator_t::get().allocate());
			} else {
				assert(n == 1);
				return single_allocator_t<element_t>::allocate(1);
			}
		}

		template <typename... args_t>
		void construct(element_t* p, args_t&&... args) {
			new (p) element_t(std::forward<args_t>(args)...);
		}

		void destroy(element_t* p) {
			p->~type();
		}

		void deallocate(element_t* p, size_t n) {
			if (n == block_size / sizeof(element_t)) {
				allocator_t::get().deallocate(p);
			} else {
				assert(n == 1);
				single_allocator_t<element_t>::deallocate(p, 1);
			}
		}
	};
}

