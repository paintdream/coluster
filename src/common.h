// common.h
// paintdream (paintdream@paintdream.com)
// 2022-6-5

#pragma once

#include "../ref/grid_dispatcher/grid_coroutine.h"
#include "../ref/grid_dispatcher/grid_system.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

namespace coluster {
	static constexpr size_t allocator_block_size = 4096;
	static constexpr size_t allocator_page_size = allocator_block_size * 16; // 64 KB

	using id_t = uint32_t;
	template <typename element_t>
	using allocator_t = grid::grid_block_allocator_t<element_t, allocator_block_size, allocator_page_size>;
	template <typename... args_t>
	using properties_t = grid::grid_system_t<id_t, allocator_t, allocator_block_size, args_t...>;
	using coroutine_t = grid::grid_coroutine_t;
	using coroutine_handle = grid::grid_coroutine_handle;

	template <typename element_t>
	using queue_list_t = grid::grid_queue_list_t<element_t>;

	using scalar = float;
	using vec2 = glm::vec2;
	using vec3 = glm::vec3;
	using vec4 = glm::vec4;
}

