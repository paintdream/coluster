// worker.h
// PaintDream (paintdream@paintdream.com)
// 2022-6-14
//

#pragma once

#include "../worker.h"

namespace coluster {
	class leaves_worker_t : public worker_t {
	public:
		void bind_frame_ticker(std::function<void(scalar)>&& frame_ticker) override;
		size_t get_current_thread_index() const noexcept override;
		size_t get_thread_count() const noexcept override;
		void queue(std::function<void()>&& func, size_t priority = 0) override;

	protected:
		static const char* handle_frame_ticker(const char* request, unsigned long& len, void* context);

	protected:
		std::function<void(scalar)> frame_ticker;
	};
}