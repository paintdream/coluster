/*
Grid-based Task Dispatcher System

This software is a C++ 11 Header-Only reimplementation of core part from project PaintsNow.

The MIT License (MIT)

copyright (c) 2014-2022 PaintDream

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

#include "grid_common.h"
#include <string>
#include <cstring>

namespace grid {
	template <class element_t, size_t storage_size = (sizeof(element_t*) * 4 - sizeof(size_t)) / sizeof(element_t)>
	class buffer_t {
	public:
		enum : size_t {
			ext_store_mask = ((size_t)1 << (sizeof(size_t) * 8 - 1)), // has external storage?
			data_view_mask = ((size_t)1 << (sizeof(size_t) * 8 - 2)) // is data view?
		};

		using iterator = element_t*;
		using const_iterator = const element_t*;
		using value_type = element_t;

		buffer_t() noexcept : size(0) {
			static_assert(storage_size >= 3 * sizeof(size_t) / sizeof(element_t), "must has stock storage of at least 3 pointer size.");
			static_assert(std::is_trivially_constructible<element_t>::value, "must be trivially constructible.");
			static_assert(std::is_trivially_destructible<element_t>::value, "must be trivially destructible.");
		}

		buffer_t(size_t init_size) noexcept(noexcept(std::declval<buffer_t>().resize(init_size))) : size(0) {
			if (init_size != 0) {
				resize(init_size);
			}
		}

		buffer_t(const element_t* p, size_t init_size) noexcept(noexcept(std::declval<buffer_t>().resize(init_size))) : size(0) {
			if (init_size != 0) {
				resize(init_size);
				memcpy(get_data(), p, init_size * sizeof(element_t));
			}
		}

		buffer_t(const buffer_t& rhs) noexcept(noexcept(std::declval<buffer_t>().copy(rhs))) : size(0) {
			copy(rhs);
		}

		~buffer_t() {
			if (is_managed_storage()) {
				assert(buffer != nullptr);
				free(buffer);
			}
		}

		iterator begin() noexcept {
			return get_data();
		}

		const_iterator begin() const noexcept {
			return get_data();
		}

		iterator end() noexcept {
			return begin() + get_size();
		}

		const_iterator end() const noexcept {
			return begin() + get_size();
		}

		iterator insert(iterator pos, const element_t& e) noexcept(noexcept(std::declval<buffer_t>().append(e))) {
			assert(!is_view_storage());
			size_t offset = pos - begin();
			push(e);
			iterator ptr = begin() + offset;

			if (offset != get_size()) {
				iterator last = end();
				for (iterator it = begin() + offset + 1; it != last; ++it) {
					*it = *(it - 1);
				}

				*ptr = e;
			}

			return ptr;
		}

		void erase(iterator pos) noexcept(noexcept(std::declval<buffer_t>().resize(1))) {
			assert(!is_view_storage());
			assert(pos != end());

			iterator last = end();
			for (iterator it = pos; it + 1 < last; ++it) {
				*it = *(it + 1);
			}

			pop();
		}

		void clear() noexcept {
			if (is_managed_storage()) {
				assert(buffer != nullptr);
				free(buffer);
			}

			size = 0;
		}

		buffer_t& operator = (const buffer_t& rhs) noexcept(noexcept(std::declval<buffer_t>().copy(rhs))) {
			copy(rhs);
			return *this;
		}

		buffer_t(buffer_t&& rhs) noexcept {
			memcpy(this, &rhs, sizeof(*this));
			rhs.size = 0;
		}

		buffer_t& operator = (buffer_t&& rhs) noexcept {
			if (this != &rhs) {
				clear();

				memcpy(this, &rhs, sizeof(*this));
				rhs.size = 0;
			}

			return *this;
		}

		static buffer_t& null() noexcept {
			static buffer_t empty;
			return empty;
		}

		// danger! be aware at your own risk!
		static buffer_t make_view(element_t* data, size_t length) noexcept {
			buffer_t buffer;
			buffer.size = length | (ext_store_mask | data_view_mask);
			buffer.buffer = data;
			buffer.next = nullptr;
			buffer.tail = nullptr;

			return buffer;
		}

		static const buffer_t make_view(const element_t* data, size_t length) noexcept {
			return make_view(const_cast<element_t*>(data), length);
		}

		static buffer_t make_view(const_iterator from, const_iterator to) noexcept {
			return make_view(from, to - from);
		}

		const buffer_t view() const noexcept {
			assert(!is_view_storage());
			return make_view(get_data(), get_size());
		}

		buffer_t view() noexcept {
			assert(!is_view_storage());
			return make_view(get_data(), get_size());
		}

		bool test(size_t offset) const noexcept {
			assert(offset < get_size() * sizeof(element_t) * 8);
			return !!(get_data()[offset / (sizeof(element_t) * 8)] & (1u << (offset % (sizeof(element_t) * 8))));
		}

		void set(size_t offset) noexcept {
			assert(offset < get_size() * sizeof(element_t) * 8);
			get_data()[offset / (sizeof(element_t) * 8)] |= (1u << (offset % (sizeof(element_t) * 8)));
		}

		bool is_managed_storage() const noexcept { return (size & (data_view_mask | ext_store_mask)) == ext_store_mask; }
		bool is_view_storage() const noexcept { return !!(size & data_view_mask); }
		bool is_stock_storage() const noexcept { return !(size & ext_store_mask); }
		size_t get_size() const noexcept { assert(size <= storage_size || (size & ~ext_store_mask) > storage_size); return size & ~(ext_store_mask | data_view_mask); }
		const element_t* get_data() const noexcept { return is_stock_storage() ? stock_storage : buffer; }
		element_t* get_data() noexcept { return is_stock_storage() ? stock_storage : buffer; }

		size_t get_view_size() const noexcept {
			if (is_view_storage()) {
				const buffer_t* p = this;
				size_t s = 0;
				while (p != nullptr) {
					s += p->get_size();
					p = p->next;
				}

				return s;
			} else {
				return get_size();
			}
		}

		// copy data from continous region (`ptr`, `size`) to this buffer starting at `offset` with `repeat` count
		void copy(size_t offset, const element_t* ptr, size_t size, size_t repeat = 1) noexcept {
			if (is_view_storage()) { // parted? copy by segments
				buffer_t* p = this;
				assert(offset + size * repeat <= get_view_size());
				while (repeat-- != 0) {
					size_t k = 0;
					while (p != nullptr && k < size) {
						size_t len = p->get_size();
						// copy part
						if (offset < len) {
							size_t r = std::min(len - offset, size - k);
							memcpy(p->get_data() + offset, ptr + k, r * sizeof(element_t));
							k += r;
							offset = 0;
						} else {
							offset -= len;
						}

						// lookup for next segment
						p = p->next;
					}
				}
			} else {
				// continous, go plain copy
				element_t* buffer = get_data();
				while (repeat-- != 0) {
					memcpy(buffer + offset, ptr, size * sizeof(element_t));
					offset += size;
				}
			}
		}

		// copy data from another buffer to this buffer starting at `offset` with `repeat` count
		void copy(size_t dst_offset, const buffer_t& buffer, size_t repeat = 1) noexcept {
			if (buffer.is_view_storage()) { // source is data view
				if (is_view_storage()) { // target is data view
					assert(get_view_size() >= dst_offset + buffer.get_view_size() * repeat);
					buffer_t* p = this;

					while (repeat-- != 0) {
						assert(p != nullptr);  // must got enough space
						// select minimal segment
						const buffer_t* q = &buffer;
						size_t src_size = q->get_size();
						const element_t* src = q->get_data();
						size_t dst_size = p->get_size();
						element_t* dst = p->get_data();
						size_t src_offset = 0;

						do {
							// copy segment data
							if (dst_offset < dst_size) {
								size_t r = std::min(dst_size - dst_offset, src_size - src_offset);
								memcpy(dst + dst_offset, src + src_offset, r * sizeof(element_t));
								dst_offset += r;
								src_offset += r;

								if (dst_offset >= dst_size) {
									p = p->next;
									if (p != nullptr) {
										dst_size = p->get_size();
										dst = p->get_data();
										dst_offset = 0;
									}
								}

								if (src_offset >= src_size) {
									q = q->next; // step source if needed
									if (q != nullptr) {
										src_size = q->get_size();
										src = q->get_data();
										src_offset = 0;
									}
								}
							} else {
								dst_offset -= dst_size;
								p = p->next; // step target if needed
							}
						} while (p != nullptr && q != nullptr);
					}
				} else {
					// only source is data view
					assert(get_size() >= dst_offset + buffer.get_view_size() * repeat);
					element_t* target = get_data() + dst_offset;

					while (repeat-- != 0) {
						const buffer_t* p = &buffer;

						while (p != nullptr) {
							size_t size = p->get_size();
							memcpy(target, p->get_data(), size * sizeof(element_t));
							target += size;
							p = p->next; // step source
						}
					}
				}
			} else {
				// only target is data view, go another version of copy
				copy(dst_offset, buffer.get_data(), buffer.get_size(), repeat);
			}
		}

		bool empty() const noexcept { return size == 0; }

		bool operator == (const buffer_t& rhs) const noexcept {
			assert(is_view_storage() == rhs.is_view_storage());
			assert(!is_view_storage() || (next == nullptr && rhs.next == nullptr));
			if (size != rhs.size) return false;
			if (size == 0) return true;

			return memcmp(get_data(), rhs.get_data(), get_size() * sizeof(element_t)) == 0;
		}

		bool operator < (const buffer_t& rhs) const noexcept {
			assert(is_view_storage() == rhs.is_view_storage());
			assert(!is_view_storage() || (next == nullptr && rhs.next == nullptr));
			if (size == 0) {
				return rhs.size != 0;
			} else {
				bool less = size < rhs.size;
				// select common range
				size_t min_size = (less ? size : rhs.size) & (~(ext_store_mask | data_view_mask));
				int result = memcmp(get_data(), rhs.get_data(), min_size * sizeof(element_t));
				return result != 0 ? result < 0 : less;
			}
		}

		const element_t& operator [] (size_t index) const noexcept {
			assert(index < get_size());
			return get_data()[index];
		}

		element_t& operator [] (size_t index) noexcept {
			assert(index < get_size());
			return get_data()[index];
		}

		void resize(size_t s, const element_t& init) noexcept(noexcept(std::declval<buffer_t>().resize(s))) {
			size_t org_size = get_size();
			resize(s);

			if (s > org_size) {
				element_t* ptr = get_data();
				std::fill(ptr + org_size, ptr + s, init);
			}
		}

		void resize(size_t s) {
			assert(!is_view_storage());
			if (is_stock_storage()) {
				if (s > storage_size) { // out of bound
					element_t* new_buffer = reinterpret_cast<element_t*>(malloc(s * sizeof(element_t)));
					if (new_buffer == nullptr) {
						throw std::bad_alloc();
					}

					memcpy(new_buffer, stock_storage, get_size() * sizeof(element_t));
					buffer = new_buffer;
					size = s | ext_store_mask;
				} else {
					size = s;
				}
			} else {
				if (s > storage_size) {
					if (s > get_size()) {
						element_t* new_buffer = reinterpret_cast<element_t*>(realloc(buffer, s * sizeof(element_t)));
						if (new_buffer == nullptr) {
							throw std::bad_alloc();
						}

						buffer = new_buffer;
					}

					size = s | ext_store_mask;
				} else {
					// shrink
					element_t* org_buffer = buffer;
					memcpy(stock_storage, org_buffer, s * sizeof(element_t));
					free(org_buffer);

					size = s;
				}
			}
		}

		void swap(buffer_t& rhs) noexcept {
			std::swap(size, rhs.size);
			for (size_t i = 0; i < storage_size; i++) {
				std::swap(stock_storage[i], rhs.stock_storage[i]);
			}
		}

		buffer_t& append(const buffer_t& rhs) noexcept(noexcept(std::declval<buffer_t>().append(rhs.get_data(), rhs.get_size()))) {
			if (empty()) {
				*this = rhs;
				return *this;
			} else if (is_view_storage()) {
				// concat buffer, not copying
				assert(rhs.is_view_storage());
				buffer_t* p = this;

				while (true) {
					size_t cur_size = p->get_size();
					if (cur_size == 0) {
						*p = rhs;
						return *this;
					} else if (rhs.buffer == p->buffer + cur_size && p->next == nullptr) { // continuous?
						p->size += rhs.get_size();
						p->next = rhs.next;
						tail = rhs.tail == nullptr ? tail : rhs.tail;
						return *this;
					} else {
						if (p->tail == nullptr) {
							assert(p->next == nullptr);
							p->next = const_cast<buffer_t*>(&rhs);
							tail = rhs.tail == nullptr ? p->next : rhs.tail;
							return *this;
						} else {
							p = p->tail;
						}
					}
				}

				// never reach here
				return *this;
			} else {
				// must copy here
				assert(!rhs.is_view_storage() || rhs.next == nullptr);
				return append(rhs.get_data(), rhs.get_size());
			}
		}

		// plain data appending
		buffer_t& append(const element_t* buffer, size_t append_size) noexcept(noexcept(std::declval<buffer_t>().resize(append_size))) {
			if (append_size != 0) {
				size_t org_size = get_size();
				resize(org_size + append_size);
				memcpy(get_data() + org_size, buffer, append_size * sizeof(element_t));
			}

			return *this;
		}

		void push(const element_t& element) noexcept(noexcept(std::declval<buffer_t>().append(std::declval<const element_t*>(), 1))) {
			append(&element, 1);
		}

		void pop() noexcept(noexcept(std::declval<buffer_t>().resize(1))) {
			resize(get_size() - 1);
		}

		// plain data assignment
		buffer_t& assign(const element_t* buffer, size_t n) noexcept(noexcept(std::declval<buffer_t>().resize(n))) {
			resize(n);
			if (n != 0) {
				memcpy(get_data(), buffer, n * sizeof(element_t));
			}

			return *this;
		}

	protected:
		// plain data copying
		void copy(const buffer_t& rhs) noexcept(noexcept(std::declval<buffer_t>().resize(rhs.get_size()))) {
			if (rhs.is_view_storage()) {
				clear();
				memcpy(this, &rhs, sizeof(rhs));
			} else {
				size_t s = rhs.get_size();
				resize(s);
				memcpy(get_data(), rhs.get_data(), s * sizeof(element_t));
			}
		}

		size_t size;
		union {
			struct {
				element_t* buffer;
				buffer_t* next;
				buffer_t* tail;
			};
			element_t stock_storage[storage_size];
		};
	};

	using bytes_t = buffer_t<uint8_t>;

	template <class element_t, size_t block_size = 4096, template <typename...> class allocator_t = std::allocator>
	class cache_t {
	public:
		buffer_t<element_t> allocate(size_t size, size_t alignment = 16) {
			size_t pack = allocator.pack_size(alignment);
			static_assert(alignof(buffer_t<element_t>) % sizeof(element_t) == 0, "buffer_t<element_t> must be aligned at least sizeof(element_t).");
			const size_t head_count = sizeof(buffer_t<element_t>) / sizeof(element_t);

			if (size > pack) {
				element_t* slice = allocator.allocate(pack, alignment);
				buffer_t<element_t> head = buffer_t<element_t>::make_view(slice, pack);
				buffer_t<element_t>* p = &head;
				size -= pack;
				pack = allocator.full_pack_size() - head_count;
				alignment = std::max(alignment, (size_t)(alignof(buffer_t<element_t>) / sizeof(element_t)));

				while (size != 0) {
					size_t alloc_count = std::min(size, pack);
					slice = allocator.allocate(alloc_count + head_count, alignment);
					buffer_t<element_t>* next = new (slice) buffer_t<element_t>();
					*next = buffer_t<element_t>::make_view(slice + head_count, pack);
					size -= alloc_count;

					p->append(*next);
					p = next;
				}

				return head;
			} else {
				return buffer_t<element_t>::make_view(allocator.allocate(size, alignment), size);
			}
		}

		static constexpr size_t full_pack_size() {
			return grid_queue_list_t<element_t, block_size>::full_pack_size();
		}

		element_t* allocate_linear(size_t size, size_t alignment = 16) {
			return allocator.allocate(size, alignment);
		}

		void link(buffer_t<element_t>& from, const buffer_t<element_t>& to) {
			if (from.empty()) {
				from = to;
			} else {
				assert(from.is_view_storage() && to.is_view_storage());
				buffer_t<element_t> storage = allocate(sizeof(buffer_t<element_t>) / sizeof(element_t), alignof(buffer_t<element_t>) / sizeof(element_t));
				from.append(*new (storage.get_data()) buffer_t<element_t>(to));
			}
		}

		void reset() noexcept {
			allocator.reset(~(size_t)0);
		}

		void clear() noexcept {
			allocator.reset(0);
		}

	protected:
		grid_queue_list_t<element_t, block_size, allocator_t> allocator;
	};

	using bytes_cache_t = cache_t<uint8_t>;

	template <class element_t, class base_t = uint8_t, size_t block_size = 4096>
	struct cache_allocator_t {
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
		struct rebind { using other = cache_allocator_t<morph_t, base_t, block_size>; };
		using allocator_t = cache_t<base_t, block_size>;
		allocator_t* allocator;

		cache_allocator_t(allocator_t* alloc) noexcept : allocator(alloc) {}
		cache_allocator_t(const cache_allocator_t& al) noexcept : allocator(al.allocator) {}

#ifdef _MSC_VER
		// maybe a bug of vc (debug), just add these lines to make compiler happy
		template <class morph_t>
		cache_allocator_t(const cache_allocator_t<morph_t, base_t, block_size>& rhs) noexcept : allocator(rhs.allocator) {}
#endif

		template <class morph_t>
		bool operator == (const morph_t& rhs) const noexcept {
			return allocator == rhs.allocator;
		}

		template <class morph_t>
		bool operator != (const morph_t& rhs) const noexcept {
			return allocator != rhs.allocator;
		}

		pointer address(reference x) const noexcept {
			return &x;
		};

		const_pointer address(const_reference x) const noexcept {
			return &x;
		}

		size_type max_size() const {
			return ~(size_type)0 / sizeof(element_t);
		}

		void construct(pointer p) {
			new ((void*)p) element_t();
		}

		void construct(pointer p, const_reference val) {
			new ((void*)p) element_t(val);
		}

		template <class morph_t, class... args_t>
		void construct(morph_t* p, args_t&&... args) {
			new ((void*)p) morph_t(std::forward<args_t>(args)...);
		}

		template <class morph_t>
		void destroy(morph_t* p) noexcept {
			((morph_t*)p)->~morph_t();
		}

		pointer allocate(size_type n, const void* hint = nullptr) {
			assert(allocator != nullptr);
			static_assert(sizeof(element_t) % sizeof(base_t) == 0, "must be aligned.");
			size_t count = n * sizeof(element_t) / sizeof(base_t);
			if (count <= allocator_t::full_pack_size()) {
				return reinterpret_cast<pointer>(allocator->allocate_linear(count, alignof(element_t)));
			} else {
				return reinterpret_cast<pointer>(::operator new(sizeof(element_t) * n));
			}
		}

		void deallocate(element_t* p, size_t n) noexcept {
			size_t count = n * sizeof(element_t) / sizeof(base_t);
			if (count <= allocator_t::full_pack_size()) {
				// do not deallocate in cache allocator.
			} else {
				::operator delete(p);
			}
		}
	};
}

