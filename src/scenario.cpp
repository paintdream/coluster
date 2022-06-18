#include "scenario.h"

namespace coluster {
	scenario_t::scenario_t(worker_t& worker_impl) noexcept : worker(worker_impl), dispatcher(worker), tick_warp(worker, 0), script_warp(worker, 0) {
		worker.bind_frame_ticker([this](scalar dtime) { frame_ticker(dtime); });
	}

	void scenario_t::frame_ticker(scalar dtime) {
		pending_frames.push(dtime);

		coroutine_handle handle = await_handle.exchange(coroutine_handle(), std::memory_order_acq_rel);
		if (handle) {
			handle.resume();
		}
	}

	bool scenario_t::await_ready() const noexcept {
		return !pending_frames.empty();
	}

	void scenario_t::await_suspend(coroutine_handle handle) {
		await_handle.store(std::move(handle), std::memory_order_release);
	}

	scalar scenario_t::await_resume() noexcept {
		assert(!pending_frames.empty());
		scalar dtime = pending_frames.top();
		pending_frames.pop();

		return dtime;
	}

	coroutine_t scenario_t::logic() {
		while (true) {
			scalar dtime = co_await *this;
			if (dtime < 0)
				break;

			co_await grid::grid_switch(&script_warp);
			co_await grid::grid_switch(&tick_warp);
		}
	}
}
