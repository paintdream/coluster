// worker.h
// PaintDream (paintdream@paintdream.com)
// 2022-6-14
//

#pragma once

#include <cstdint>
#include <functional>

namespace coluster {
	class worker_t {	
	public:
		virtual size_t get_current_thread_index() const noexcept = 0;
		virtual void queue(std::function<void()>&& func, size_t priority = 0) = 0;
	};
}
