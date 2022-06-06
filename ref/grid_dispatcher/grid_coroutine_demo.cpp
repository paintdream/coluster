#include "grid_coroutine.h"
#include <chrono>
using namespace grid;

using warp_t = grid_warp_t<grid_async_worker_t<>>;
using barrier_t = grid_barrier_t<void, grid_async_worker_t<>>;
using barrier_warp_t = grid_barrier_t<warp_t, grid_async_worker_t<>>;
using frame_t = grid_frame_t<void, grid_async_worker_t<>>;
using frame_warp_t = grid_frame_t<warp_t, grid_async_worker_t<>>;
static std::atomic<size_t> pending_count = 0;

grid_coroutine_t example(warp_t::async_worker_t& async_worker, warp_t* warp, int value) {
	if (warp != nullptr) {
		warp_t* current = co_await grid_switch(warp);
		printf("Switch to warp %p\n", warp);
		co_await grid_switch(warp_t::null());
		printf("Detached\n");
		co_await grid_switch(warp);
		printf("Attached\n");
		co_await grid_switch(current);
		assert(current == warp_t::get_current_warp());
	}

	// Step 1: test single await
	co_await grid_awaitable(warp, []() {});
	int v = co_await grid_awaitable(warp, [value]() { return value; });
	warp_t* current = warp_t::get_current_warp();
	printf("Value: %d %p\n", v, warp_t::get_current_warp());

	// Step 2: test multiple await by incrementally construction
	std::function<void()> v1 = [value]() {};
	std::function<void()> v2 = [value]() {};
	std::function<void()> v3 = [value]() {};

	if (warp == warp_t::null()) {
		grid_awaitable_multiple_t<warp_t, std::function<void()>> multiple(async_worker, grid_awaitable(warp, std::move(v1)));
		multiple += grid_awaitable(warp, std::move(v2));
		multiple += grid_awaitable(warp, std::move(v3));
		co_await multiple;
	} else {
		co_await grid_awaitable_parallel(warp, []() {});

		grid_awaitable_multiple_t<warp_t, std::function<void()>> multiple(async_worker, grid_awaitable_parallel(warp, std::move(v1)));
		multiple += grid_awaitable_parallel(warp, std::move(v2));
		multiple += grid_awaitable_parallel(warp, std::move(v3), 1);
		co_await multiple;
	}

	// Step 3: test multiple await by join-like construction
	std::function<int()> v4 = [value]() { return value + 4; };
	std::function<int()> v5 = [value]() { return value + 5; };
	std::vector<int> rets = co_await grid_awaitable_union(async_worker, grid_awaitable(warp, std::move(v4)), grid_awaitable(warp, std::move(v5)));
	printf("Value: (%d, %d) %p\n", rets[0], rets[1], warp_t::get_current_warp());

	if (warp != warp_t::null()) {
		warp_t* current = co_await grid_switch(warp);
		printf("Another switch to warp %p\n", warp);
		co_await grid_switch(current);
		assert(current == warp_t::get_current_warp());
	}

	// if all tests finished, terminate the thread pool and exit the program
	if (pending_count.fetch_sub(1, std::memory_order_release) == 1) {
		async_worker.terminate();
	}
}

grid_coroutine_t example_empty() {
	printf("Empty finished!\n");
	co_return;
}

template <typename barrier_type_t>
grid_coroutine_t example_barrier(barrier_type_t& barrier, int index) {
	printf("Example barrier %d begin running!\n", index);

	if (co_await barrier == 0) {
		printf("Unique barrier!\n");
	}

	co_await barrier;
	printf("Example barrier %d mid running!\n", index);

	co_await barrier;
	printf("Example barrier %d end running!\n", index);
}

template <typename frame_type_t>
grid_coroutine_t example_frame(frame_type_t& frame, int index) {
	printf("Example frame %d begin running!\n", index);

	co_await frame;
	printf("Example frame %d mid running!\n", index);

	co_await frame;
	printf("Example frame %d end running!\n", index);
}

int main(void) {
	static constexpr size_t thread_count = 8;
	static constexpr size_t warp_count = 16;
	grid_async_worker_t<> worker(thread_count);
	worker.start();

	std::vector<warp_t> warps;
	warps.reserve(warp_count);
	for (size_t i = 0; i < warp_count; i++) {
		warps.emplace_back(worker);
	}

	// test for barrier
	barrier_t barrier(worker, 4);
	example_barrier(barrier, 0).run();
	example_barrier(barrier, 1).run();
	example_barrier(barrier, 2).run();
	example_barrier(barrier, 3).run();

	barrier_warp_t barrier_warp(worker, 4);
	warps[0].queue_routine_external([&barrier_warp]() {
		example_barrier(barrier_warp, 5).run();
		example_barrier(barrier_warp, 6).run();
		example_barrier(barrier_warp, 7).run();
		example_barrier(barrier_warp, 8).run();
	});

	// test for frame
	frame_t frame(worker);
	example_frame(frame, 0).run();
	example_frame(frame, 1).run();
	example_frame(frame, 2).run();
	example_frame(frame, 3).run();

	for (size_t k = 0; k < 4; k++) {
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		frame.flush();
	}

	frame_warp_t frame_warp(worker);
	warps[0].queue_routine_external([&frame_warp]() {
		example_frame(frame_warp, 5).run();
		example_frame(frame_warp, 6).run();
		example_frame(frame_warp, 7).run();
		example_frame(frame_warp, 8).run();

		for (size_t k = 0; k < 4; k++) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			frame_warp.flush();
		}
	});

	// initialize pending count with `example` call count
	pending_count.fetch_add(6, std::memory_order_acq_rel);

	// test for running example from an external thread
	example(worker, &warps[0], 1).complete([]() {
		printf("Complete!\n");
	}).run();

	example_empty().complete([]() {
		printf("Complete empty!\n");
	}).join();

	std::atomic<bool> result = false;
	example(worker, warp_t::null(), 2).run(result, true).wait(false, std::memory_order_acquire);

	warps[0].queue_routine_external([&worker, &warps]() {
		// test for running example from an warp
		example(worker, &warps[0], 3).run();
		example(worker, warp_t::null(), 4).run(); // cannot call join() here since warps[0] will be blocked
	});

	// test for running example from thread pool
	worker.queue([&worker, &warps]() {
		// test for running example from an warp
		example(worker, &warps[0], 5).run();
		example(worker, warp_t::null(), 6).join(); // can call join() here since we are NOT in any warp
	});

	worker.join();

	// finished!
	warp_t::join(warps.begin(), warps.end());
	return 0;
}

