// worker.h
// PaintDream (paintdream@paintdream.com)
// 2022-6-14
//

#pragma once

#include "../common.h"
#include <cstdint>
#include <functional>

namespace coluster {
	class worker_t {	
	public:
		virtual void bind_frame_ticker(std::function<void(scalar)>&& frame_ticker) = 0;
		virtual size_t get_current_thread_index() const noexcept = 0;
		virtual size_t get_thread_count() const noexcept = 0;
		virtual void queue(std::function<void()>&& func, size_t priority = 0) = 0;
	};
	
	using warp_t = grid::grid_warp_t<worker_t>;
	using dispatcher_t = grid::grid_dispatcher_t<warp_t>;
}
