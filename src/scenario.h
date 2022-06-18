// scenario.h
// PaintDream (paintdream@paintdream.com)
// 2022-6-7

#pragma once

#include "common.h"
#include "storage.h"
#include "platform/worker.h"

namespace coluster {
	class scenario_t {
	public:
		scenario_t(worker_t& worker) noexcept;
		coroutine_t logic();

	protected:
		void frame_ticker(scalar dtime);
		
		bool await_ready() const noexcept;
		void await_suspend(coroutine_handle handle);
		scalar await_resume() noexcept;

	protected:
		worker_t& worker;
		dispatcher_t dispatcher;
		warp_t tick_warp;
		warp_t script_warp;
		storage_t storage;
		queue_list_t<scalar> pending_frames;
		std::atomic<coroutine_handle> await_handle;
	};
}
