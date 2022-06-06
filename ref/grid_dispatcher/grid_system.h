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
#include "grid_dispatcher.h"
#include <array>

namespace grid {
	// a simple entity-component-system
	// thread safety:
	// 1. [no ] insert/insert
	// 2. [no ] remove/remove
	// 3. [no ] insert/iterate
	// 4. [no ] remove/iterate
	// 5. [yes] iterate/iterate
	// 6. [yes] iterate/insert
	// 

	// find type index for given type
	// https://stackoverflow.com/questions/26169198/how-to-get-the-index-of-a-type-in-a-variadic-type-pack
	template <typename target_t, typename... types_t>
	struct fetch_index_impl : std::integral_constant<size_t, 0> {};

	template <typename target_t, typename... types_t>
	struct fetch_index_impl<target_t, target_t, types_t...> : std::integral_constant<size_t, 0> {};

	template <typename target_t, typename next_t, typename... types_t>
	struct fetch_index_impl<target_t, next_t, types_t...> : std::integral_constant<size_t, 1 + fetch_index_impl<target_t, types_t...>::value> {};

	template <size_t n, typename... args_t>
	using locate_type = typename std::tuple_element<n, std::tuple<args_t...>>::type;

	// std::make_index_sequence for C++ 11
	// seq from stackoverflow http://stackoverflow.com/questions/17424477/implementation-c14-make-integer-sequence by xeo
	template <size_t...> struct seq { using type = seq; };
	template <class s1, class s2> struct concat;
	template <size_t... i1, size_t... i2>
	struct concat<seq<i1...>, seq<i2...>> : public seq<i1..., (sizeof...(i1)+i2)...> {};
	template <size_t n> struct gen_seq;
	template <size_t n>
	struct gen_seq : public concat<typename gen_seq<n / 2>::type, typename gen_seq<n - n / 2>::type>::type {};
	template <> struct gen_seq<0> : seq<> {};
	template <> struct gen_seq<1> : seq<0> {};

	template <typename type_t>
	constexpr bool check_duplicated_components_one() { return true; }

	template <typename type_t, typename first_t, typename... check_types_t>
	constexpr bool check_duplicated_components_one() {
		return !std::is_same<type_t, first_t>::value && check_duplicated_components_one<type_t, check_types_t...>();
	}

	template <typename first_t>
	constexpr bool check_duplicated_components() { return true; }

	template <typename first_t, typename second_t, typename... check_types_t>
	constexpr bool check_duplicated_components() {
		return check_duplicated_components_one<first_t, second_t, check_types_t...>() && check_duplicated_components<second_t, check_types_t...>();
	}

	// components_t is not allowed to contain repeated types
	template <typename entity_t, template <typename...> class allocator_t, size_t block_size, typename... components_t>
	class grid_system_t {
	public:
		template <typename target_t>
		struct fetch_index : fetch_index_impl<target_t, components_t...> {};
		static constexpr size_t component_block_size = block_size;

		grid_system_t() {
			// check if there are duplicated types
			static_assert(check_duplicated_components<components_t...>(), "duplicated component detected!");
		}

		// entity-based component insertion
		bool valid(entity_t entity) const noexcept {
			return entity < entity_components.size() && entity_components[entity] != ~(size_t)0;
		}

		// returns true if the existing entity was replaced
		template <typename... elements_t>
		bool insert(entity_t entity, elements_t&&... t) {
			if (entity < entity_components.size()) {
				if (entity_components[entity] != ~(size_t)0) {
					// replace contents only, do not add new entity
					emplace_components<sizeof...(components_t)>(std::forward<elements_t>(t)...);
					return true;
				}
			} else {
				entity_components.resize(entity + 1, ~(size_t)0);
			}

			preserve_entity(placeholder<components_t...>());
			emplace_components<sizeof...(components_t)>(std::forward<elements_t>(t)...);
			entities.emplace(entity);
			entity_components[entity] = entities.end_index();

			return false;
		}

		template <typename... elements_t>
		entity_t append(elements_t&&... t) {
			static_assert(sizeof...(elements_t) == sizeof...(components_t), "incorrect number of elements.");
			// may throw exceptions
			// do not actually push any elements here
			preserve_entity(placeholder<components_t...>());

			entity_t entity;
			if (!free_entities.empty()) {
				entity = free_entities.top();
				free_entities.pop();
				entity_components[entity] = entities.end_index();
			} else {
				entity = static_cast<entity_t>(entity_components.size());
				entity_components.emplace_back(entities.end_index());
			}

			// preserved earlier, so the following code must not throw any exceptions
			emplace_components<sizeof...(components_t)>(std::forward<elements_t>(t)...);
			entities.emplace(entity);

			return entity;
		}

		size_t size() const noexcept {
			return entities.size();
		}

		// get specified component of given entity
		template <typename component_t>
		component_t& get(entity_t entity) noexcept {
			assert(valid(entity));
			return std::get<fetch_index<component_t>::value>(components).get(entity_components[entity]);
		}

		template <typename component_t>
		const component_t& get(entity_t entity) const noexcept {
			assert(valid(entity));
			return std::get<fetch_index<component_t>::value>(components).get(entity_components[entity]);
		}

		// entity-based component removal
		void remove(entity_t entity) {
			assert(valid(entity));
			assert(!entities.empty());
			assert(entity < entity_components.size());

			free_entities.preserve();
			entity_t top_entity = entities.top();

			// swap the top element (component_t, entity_t) with removed one
			if (entity != top_entity) {
				// move!!
				size_t slot = entity_components[entity];
				entity_components[top_entity] = slot;
				swap_components(slot, placeholder<components_t...>());
				entity_components[entity] = ~(size_t)0;
			}

			pop_components(placeholder<components_t...>());
			entities.pop();
			free_entities.emplace(entity);
		}

		// iterate components
		template <typename component_t, typename callable_t>
		void for_each(callable_t&& op) noexcept(noexcept(std::declval<grid_queue_list_t<component_t, block_size, allocator_t>>().for_each(std::forward<callable_t>(op)))) {
			std::get<fetch_index<component_t>::value>(components).for_each(std::forward<callable_t>(op));
		}

		// n is the expected group size
		template <typename component_t, typename warp_t, typename operand_t>
		void for_each_parallel(operand_t&& op, size_t n = grid_queue_list_t<component_t, block_size, allocator_t>::node_t::element_count) {
			auto& target_components = std::get<fetch_index<component_t>::value>(components);
			warp_t* warp = warp_t::get_current_warp();
			assert(warp != nullptr);

			using node_t = typename grid_queue_list_t<component_t, block_size, allocator_t>::node_t;
			if (n <= node_t::element_count) {
				// one node per group, go fast path
				target_components.for_each_queue([warp, op](node_t* p) {
					warp->queue_routine_parallel([p, op]() mutable {
						p->for_each(std::move(op));
					});
				});
			} else {
				// use cache list
				std::vector<node_t*> cache;
				size_t count = 0;
				target_components.for_each_queue([&cache, &count, n, op, warp](node_t* p) {
					cache.emplace_back(p);
					count += p->size();
					if (count >= n) {
						warp->queue_routine_parallel([cache, op]() mutable {
							for (auto&& p : cache) {
								p->for_each(op);
							}
						});

						cache.clear();
						count = 0;
					}
				});

				// dispatch the remaining
				if (count != 0) {
					warp->queue_routine_parallel([cache, op]() mutable {
						for (auto&& p : cache) {
							p->for_each(op);
						}
					});
				}
			}
		}

		template <typename component_t>
		grid_queue_list_t<component_t, block_size, allocator_t>& component() noexcept {
			return std::get<fetch_index<component_t>::value>(components);
		}

		template <typename component_t>
		const grid_queue_list_t<component_t, block_size, allocator_t>& component() const noexcept {
			return std::get<fetch_index<component_t>::value>(components);
		}

		template <typename component_t>
		static constexpr bool has() noexcept {
			return fetch_index<component_t>::value < sizeof...(components_t);
		}

	protected:
		template <size_t i>
		void emplace_components() {}

		template <size_t i, typename first_t, typename... elements_t>
		void emplace_components(first_t&& first, elements_t&&... remaining) {
			std::get<sizeof...(components_t) - i>(components).emplace(std::forward<first_t>(first));
			emplace_components<i - 1>(std::forward<elements_t>(remaining)...);
		}

		template <size_t i>
		void replace_components(size_t id) {}

		template <size_t i, typename first_t, typename... elements_t>
		void replace_components(size_t id, first_t&& first, elements_t&&... remaining) {
			std::get<sizeof...(components_t) - i>(components).get(id) = std::forward<first_t>(first);
			replace_components<i - 1>(id, std::forward<elements_t>(remaining)...);
		}

		template <typename first_t, typename... elements_t>
		void swap_components(size_t index, placeholder<first_t, elements_t...>) noexcept {
			auto& comp = std::get<sizeof...(elements_t)>(components);
			auto& top = comp.top();
			comp.get(index) = std::move(top);

			swap_components(index, placeholder<elements_t...>());
		}

		void swap_components(size_t& index, placeholder<>) noexcept {}

		template <typename first_t, typename... elements_t>
		void preserve_entity(placeholder<first_t, elements_t...>) {
			std::get<sizeof...(elements_t)>(components).preserve();
			preserve_entity(placeholder<elements_t...>());
		}

		void preserve_entity(placeholder<>) {
			entities.preserve();
		}

		template <typename first_t, typename... elements_t>
		void pop_components(placeholder<first_t, elements_t...>) noexcept {
			std::get<sizeof...(elements_t)>(components).pop();
			pop_components(placeholder<elements_t...>());
		}

		void pop_components(placeholder<>) noexcept {}

	protected:
		std::tuple<grid_queue_list_t<components_t, block_size, allocator_t>...> components;
		std::vector<size_t> entity_components;
		grid_queue_list_t<entity_t, block_size, allocator_t> entities;
		grid_queue_list_t<entity_t, block_size, allocator_t> free_entities;
	};

	template <template <typename...> class allocator_t, typename... subsystems_t>
	class grid_systems_t {
	public:
		grid_systems_t(subsystems_t&... args) : subsystems(args...) {}
		static constexpr size_t block_size = locate_type<0, subsystems_t...>::component_block_size;

		template <typename component_t>
		using component_queue_t = grid_queue_list_t<component_t, block_size, allocator_t>;

		template <size_t system_count, typename... components_t>
		struct component_view {
			using queue_tuple_t = std::tuple<grid_queue_list_t<components_t, block_size, allocator_t>*...>;
			using queue_iterator_t = std::tuple<typename grid_queue_list_t<components_t, block_size, allocator_t>::iterator...>;

			struct iterator {
				using difference_type = ptrdiff_t;
				using value_type = typename std::conditional<sizeof...(components_t) == 1, locate_type<0, components_t...>, std::tuple<std::reference_wrapper<components_t>...>>::type;
				using reference = value_type&;
				using pointer = value_type*;
				using iterator_category = std::input_iterator_tag;

				template <typename... iter_t>
				iterator(component_view& view_host, size_t i, iter_t&&... iter) : host(&view_host), index(i), it(std::forward<iter_t>(iter)...) {}

				template <size_t... s>
				static iterator make_iterator_begin(component_view& view_host, size_t i, queue_tuple_t& sub, seq<s...>) noexcept {
					return iterator(view_host, i, std::get<s>(sub)->begin()...);
				}

				template <size_t... s>
				static iterator make_iterator_end(component_view& view_host, size_t i, queue_tuple_t& sub, seq<s...>) noexcept {
					return iterator(view_host, i, std::get<s>(sub)->end()...);
				}

				iterator& operator ++ () noexcept {
					step();
					return *this;
				}

				iterator operator ++ (int) noexcept {
					iterator r = *this;
					step();

					return r;
				}

				bool operator == (const iterator& rhs) const noexcept {
					return index == rhs.index && std::get<0>(it) == std::get<0>(rhs.it);
				}

				bool operator != (const iterator& rhs) const noexcept {
					return index != rhs.index || std::get<0>(it) != std::get<0>(rhs.it);
				}

				template <size_t... s>
				std::tuple<std::reference_wrapper<components_t>...> make_value(seq<s...>) const noexcept {
					return std::make_tuple<std::reference_wrapper<components_t>...>(*std::get<s>(it)...);
				}

				reference filter_value(std::true_type) const noexcept {
					return *std::get<0>(it);
				}

				std::tuple<std::reference_wrapper<components_t>...> filter_value(std::false_type) const noexcept {
					return make_value(gen_seq<sizeof...(components_t)>());
				}

				typename std::conditional<sizeof...(components_t) == 1, reference, value_type>::type operator * () const noexcept {
					return filter_value(std::integral_constant<bool, sizeof...(components_t) == 1>());
				}

				template <typename callable_t>
				void invoke(callable_t&& op) const {
					invoke_impl(std::forward<callable_t>(op), gen_seq<sizeof...(components_t)>());
				}

			protected:
				template <typename callable_t, size_t... s>
				void invoke_impl(callable_t&& op, seq<s...>) const {
					op(*std::get<s>(it)...);
				}

				template <typename first_t>
				static bool reduce(first_t f) noexcept { return f; }
				template <typename first_t, typename... args_t>
				static bool reduce(first_t f, args_t&&...) noexcept { return f; }

				template <size_t... s>
				bool step_impl(seq<s...>) noexcept {
					return reduce(std::get<s>(it).step()...);
				}

				void step() noexcept {
					if (!step_impl(gen_seq<sizeof...(components_t)>())) {
						while (index + 1 < system_count) {
							if (!std::get<0>(host->subcomponents[++index])->empty()) {
								*this = make_iterator_begin(*host, index, host->subcomponents[index], gen_seq<sizeof...(components_t)>());

								return;
							}
						}

						*this = make_iterator_end(*host, index, host->subcomponents[index], gen_seq<sizeof...(components_t)>());
					}
				}

				component_view* host;
				size_t index;
				queue_iterator_t it;
			};

			iterator begin() noexcept {
				for (size_t i = 0; i < subcomponents.size(); i++) {
					if (!std::get<0>(subcomponents[i])->empty()) {
						return iterator::make_iterator_begin(*this, i, subcomponents[i], gen_seq<sizeof...(components_t)>());
					}
				}

				return end();
			}

			iterator end() noexcept {
				return iterator::make_iterator_end(*this, system_count - 1, subcomponents[system_count - 1], gen_seq<sizeof...(components_t)>());
			}

			template <typename callable_t>
			void for_each(callable_t&& op) {
				for_each_impl(std::forward<callable_t>(op), std::integral_constant<bool, sizeof...(components_t) == 1>());
			}

			template <typename callable_t>
			void for_each_system(callable_t&& op) {
				for_each_system_impl(std::forward<callable_t>(op), gen_seq<sizeof...(components_t)>());
			}

			std::array<queue_tuple_t, system_count> subcomponents;

		protected:
			template <typename callable_t>
			void for_each_impl(callable_t&& op, std::true_type) {
				// simple path
				for (size_t i = 0; i < subcomponents.size(); i++) {
					std::get<0>(subcomponents[i])->for_each(std::forward<callable_t>(op));
				}
			}

			template <typename callable_t>
			void for_each_impl(callable_t&& op, std::false_type) {
				// complex path
				for (auto it = begin(); it != end(); ++it) {
					it.invoke(std::forward<callable_t>(op));
				}
			}

			template <typename callable_t, size_t... s>
			void for_each_system_impl(callable_t&& op, seq<s...>) {
				for (size_t i = 0; i < subcomponents.size(); i++) {
					op(*std::get<s>(subcomponents[i])...);
				}
			}
		};

		template <size_t system_count, typename... components_t>
		struct const_component_view {
			using queue_tuple_t = std::tuple<const grid_queue_list_t<components_t, block_size, allocator_t>*...>;
			using queue_iterator_t = std::tuple<typename grid_queue_list_t<components_t, block_size, allocator_t>::const_iterator...>;

			struct iterator {
				using difference_type = ptrdiff_t;
				using value_type = typename std::conditional<sizeof...(components_t) == 1, const locate_type<0, components_t...>, std::tuple<std::reference_wrapper<const components_t>...>>::type;
				using reference = value_type&;
				using pointer = value_type*;
				using iterator_category = std::input_iterator_tag;

				template <typename... iter_t>
				iterator(const_component_view& view_host, size_t i, iter_t&&... iter) : host(&view_host), index(i), it(std::forward<iter_t>(iter)...) {}

				template <size_t... s>
				static iterator make_iterator_begin(const_component_view& view_host, size_t i, queue_tuple_t& sub, seq<s...>) noexcept {
					return iterator(view_host, i, std::get<s>(sub)->begin()...);
				}

				template <size_t... s>
				static iterator make_iterator_end(const_component_view& view_host, size_t i, queue_tuple_t& sub, seq<s...>) noexcept {
					return iterator(view_host, i, std::get<s>(sub)->end()...);
				}

				iterator& operator ++ () noexcept {
					step();
					return *this;
				}

				iterator operator ++ (int) noexcept {
					iterator r = *this;
					step();

					return r;
				}

				bool operator == (const iterator& rhs) const noexcept {
					return index == rhs.index && std::get<0>(it) == std::get<0>(rhs.it);
				}

				bool operator != (const iterator& rhs) const noexcept {
					return index != rhs.index || std::get<0>(it) != std::get<0>(rhs.it);
				}

				template <size_t... s>
				std::tuple<std::reference_wrapper<const components_t>...> make_value(seq<s...>) const noexcept {
					return std::make_tuple<std::reference_wrapper<const components_t>...>(*std::get<s>(it)...);
				}

				reference filter_value(std::true_type) const noexcept {
					return *std::get<0>(it);
				}

				std::tuple<std::reference_wrapper<const components_t>...> filter_value(std::false_type) const noexcept {
					return make_value(gen_seq<sizeof...(components_t)>());
				}

				typename std::conditional<sizeof...(components_t) == 1, reference, value_type>::type operator * () const noexcept {
					return filter_value(std::integral_constant<bool, sizeof...(components_t) == 1>());
				}

				template <typename callable_t>
				void invoke(callable_t&& op) const {
					invoke_impl(std::forward<callable_t>(op), gen_seq<sizeof...(components_t)>());
				}

			protected:
				template <typename callable_t, size_t... s>
				void invoke_impl(callable_t&& op, seq<s...>) const {
					op(*std::get<s>(it)...);
				}

				template <typename first_t>
				static bool reduce(first_t f) noexcept { return f; }
				template <typename first_t, typename... args_t>
				static bool reduce(first_t f, args_t&&...) noexcept { return f; }

				template <size_t... s>
				bool step_impl(seq<s...>) noexcept {
					return reduce(std::get<s>(it).step()...);
				}

				void step() noexcept {
					if (!step_impl(gen_seq<sizeof...(components_t)>())) {
						while (index + 1 < system_count) {
							if (!std::get<0>(host->subcomponents[++index])->empty()) {
								*this = make_iterator_begin(*host, index, host->subcomponents[index], gen_seq<sizeof...(components_t)>());

								return;
							}
						}

						*this = make_iterator_end(*host, index, host->subcomponents[index], gen_seq<sizeof...(components_t)>());
					}
				}

				const_component_view* host;
				size_t index;
				queue_iterator_t it;
			};

			iterator begin() noexcept {
				for (size_t i = 0; i < subcomponents.size(); i++) {
					if (!std::get<0>(subcomponents[i])->empty()) {
						return iterator::make_iterator_begin(*this, i, subcomponents[i], gen_seq<sizeof...(components_t)>());
					}
				}

				return end();
			}

			iterator end() noexcept {
				return iterator::make_iterator_end(*this, system_count - 1, subcomponents[system_count - 1], gen_seq<sizeof...(components_t)>());
			}


			template <typename callable_t>
			void for_each(callable_t&& op) {
				for_each_impl(std::forward<callable_t>(op), std::integral_constant<bool, sizeof...(components_t) == 1>());
			}

			template <typename callable_t>
			void for_each_system(callable_t&& op) {
				for_each_system_impl(std::forward<callable_t>(op), gen_seq<sizeof...(components_t)>());
			}

			std::array<queue_tuple_t, system_count> subcomponents;

		protected:
			template <typename callable_t>
			void for_each_impl(callable_t&& op, std::true_type) {
				// simple path
				for (size_t i = 0; i < subcomponents.size(); i++) {
					std::get<0>(subcomponents[i])->for_each(std::forward<callable_t>(op));
				}
			}

			template <typename callable_t>
			void for_each_impl(callable_t&& op, std::false_type) {
				// complex path
				for (auto it = begin(); it != end(); ++it) {
					it.invoke(std::forward<callable_t>(op));
				}
			}

			template <typename callable_t, size_t... s>
			void for_each_system_impl(callable_t&& op, seq<s...>) {
				for (size_t i = 0; i < subcomponents.size(); i++) {
					op(*std::get<s>(subcomponents[i])...);
				}
			}
		};

		template <size_t n, typename target_t, typename system_t>
		struct count_components : std::integral_constant<size_t, (system_t::template has<typename std::tuple_element<n - 1, target_t>::type>() ? 1 : 0) + count_components<n - 1, target_t, system_t>::value> {};

		template <typename target_t, typename system_t>
		struct count_components<0, target_t, system_t> : std::integral_constant<size_t, 0> {};

		template <typename target_t, typename... types_t>
		struct count_match : std::integral_constant<size_t, 0> {};

		template <typename target_t, typename next_t, typename... types_t>
		struct count_match<target_t, next_t, types_t...> : std::integral_constant<size_t, 
			// must have all components required
			(count_components<std::tuple_size<target_t>::value, target_t, next_t>::value == std::tuple_size<target_t>::value)
			+ count_match<target_t, types_t...>::value> {};

		template <bool fill, size_t i, size_t n, typename components_tuple_t, typename view_t>
		struct fill_view_impl {
			template <typename system_t, size_t... s>
			static void execute_impl(view_t& view, system_t& sys, seq<s...>) {
				view.subcomponents[n] = std::make_tuple(&sys.template component<typename std::tuple_element<s, components_tuple_t>::type>()...);
			}

			template <typename sub_t>
			static void execute(view_t& view, sub_t& subsystems) noexcept {
				execute_impl(view, std::get<i>(subsystems), gen_seq<std::tuple_size<components_tuple_t>::value>());
			}
		};

		template <size_t i, size_t n, typename components_tuple_t, typename view_t>
		struct fill_view_impl<false, i, n, components_tuple_t, view_t> {
			template <typename sub_t>
			static void execute(view_t& view, sub_t& subsystems) noexcept {}
		};

		template <size_t i, size_t n, typename components_tuple_t, typename view_t>
		struct fill_view {
			template <typename sub_t>
			static void execute(view_t& view, sub_t& subsystems) noexcept {
				constexpr bool fill = count_components<std::tuple_size<components_tuple_t>::value, components_tuple_t, locate_type<i - 1, subsystems_t...>>::value == std::tuple_size<components_tuple_t>::value;
				fill_view_impl<fill, i - 1, n, components_tuple_t, view_t>::execute(view, subsystems);
				fill_view<i - 1, n + fill, components_tuple_t, view_t>::execute(view, subsystems);
			}
		};

		template <size_t n, typename components_tuple_t, typename view_t>
		struct fill_view<0, n, components_tuple_t, view_t> {
			template <typename sub_t>
			static void execute(view_t& view, sub_t& subsystems) noexcept {}
		};

		template <typename... components_t>
		component_view<count_match<std::tuple<components_t...>, subsystems_t...>::value, components_t...> components() noexcept {
			static_assert(check_duplicated_components<components_t...>(), "duplicated components detected!");
			constexpr size_t system_count = count_match<std::tuple<components_t...>, subsystems_t...>::value;
			static_assert(system_count != 0, "specified component types not found. use constexpr has() before calling me!");
			component_view<system_count, components_t...> view;
			fill_view<sizeof...(subsystems_t), 0, std::tuple<components_t...>, component_view<system_count, components_t...>>::execute(view, subsystems);
			return view;
		}

		template <typename... components_t>
		const_component_view<count_match<std::tuple<components_t...>, subsystems_t...>::value, components_t...> components() const noexcept {
			static_assert(check_duplicated_components<components_t...>(), "duplicated components detected!");
			constexpr size_t system_count = count_match<std::tuple<components_t...>, subsystems_t...>::value;
			static_assert(system_count != 0, "specified component types not found. use constexpr has() before calling me!");
			const_component_view<system_count, components_t...> view;
			fill_view<sizeof...(subsystems_t), 0, std::tuple<components_t...>, const_component_view<system_count, components_t...>>::execute(view, subsystems);
			return view;
		}

		template <typename... components_t>
		static constexpr bool has() noexcept {
			return check_duplicated_components<components_t...>() && count_match<std::tuple<components_t...>, subsystems_t...>::value != 0;
		}

	protected:
		std::tuple<subsystems_t&...> subsystems;
	};
}

