// scenario.h
// PaintDream (paintdream@paintdream.com)
// 2022-6-7

#pragma once

#include "common.h"
#include "storage.h"
#include "platform/worker.h"
#include "platform/framework.h"

namespace coluster {
	class scenario_t : public framework_t::listener_t, protected grid::enable_read_write_fence_t<> {
	public:
		scenario_t(worker_t& worker, framework_t& framework) noexcept;
		virtual ~scenario_t();
		coroutine_t logic();

	protected:
		void frame_tick(scalar dtime) override;
		
		bool await_ready() const noexcept;
		void await_suspend(coroutine_handle handle);
		scalar await_resume() noexcept;

	protected:
		worker_t& worker;
		dispatcher_t dispatcher;
		warp_t tick_warp;
		warp_t script_warp;

		framework_t& framework;
		storage_t storage;
		queue_list_t<scalar> pending_frames;
		std::atomic<coroutine_handle> await_handle;
		std::atomic<size_t> exiting;
		std::atomic<size_t> running;
		std::atomic<size_t> tick_fence;
	};
}
