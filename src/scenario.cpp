#include "scenario.h"

namespace coluster {
	scenario_t::scenario_t(worker_t& worker_impl, framework_t& framework_impl) noexcept : worker(worker_impl), dispatcher(worker), tick_warp(worker, 0), script_warp(worker, 0), framework(framework_impl) {
		exiting.store(0, std::memory_order_relaxed);
		running.store(0, std::memory_order_relaxed);
		tick_fence.store(0, std::memory_order_release);
		framework.bind_listener(this);

		// start logic loop
		logic().run();
	}

	scenario_t::~scenario_t() {
		framework.bind_listener(nullptr);
		auto guard = write_fence();
		exiting.store(1, std::memory_order_relaxed);

		// exit loop if needed
		coroutine_handle handle = await_handle.exchange(coroutine_handle(), std::memory_order_acq_rel);
		if (handle) {
			handle.resume();
		}

		// join tick()
		if (running.exchange(~(size_t)0, std::memory_order_acquire) == 1) {
			running.wait(0, std::memory_order_release);
		}
	}

	void scenario_t::frame_tick(scalar dtime) {
		auto guard = write_fence();
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
		if (exiting.load(std::memory_order_acquire)) {
			return scalar(-1);
		} else {
			assert(!pending_frames.empty());
			scalar dtime = pending_frames.top();
			pending_frames.pop();

			return dtime;
		}
	}

	coroutine_t scenario_t::logic() {
		size_t status = running.exchange(1, std::memory_order_acquire);
		if (status == 0) {
			while (true) {
				// acquire next frame
				scalar dtime = co_await *this;
				if (dtime < 0) // exit?
					break;

				co_await grid::grid_switch(&script_warp);
				co_await grid::grid_switch(&tick_warp);
			}
		}

		if (status == ~(size_t)0 || running.exchange(0, std::memory_order_release) == ~(size_t)0) {
			running.notify_one();
		}
	}
}
