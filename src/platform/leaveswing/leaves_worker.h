// worker.h
// PaintDream (paintdream@paintdream.com)
// 2022-6-14
//

#pragma once

#include "../worker.h"

namespace coluster {
	class leaves_worker_t : public worker_t {
	public:
		size_t get_current_thread_index() const noexcept override;
		void queue(std::function<void()>&& func, size_t priority = 0) override;
	};
}