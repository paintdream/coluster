#include "grid_dispatcher.h"
#include <iostream>
#include <future>
using namespace grid;

static void stack_op();
static void not_pow_two();
static void framed_data();
static void simple_explosion();
static void garbage_collection();
static void graph_dispatch();

int main(void) {
	stack_op();
	not_pow_two();
	framed_data();
	simple_explosion();
	garbage_collection();
	graph_dispatch();

	return 0;
}

void stack_op() {
	static constexpr size_t thread_count = 4;
	static constexpr size_t warp_count = 8;
	grid_async_worker_t<> worker(thread_count);
	using grid_warp_t = grid_warp_t<grid_async_worker_t<>>;
	std::vector<grid_warp_t> warps;
	warps.reserve(warp_count);
	for (size_t i = 0; i < warp_count; i++) {
		warps.emplace_back(worker);
	}
	
	worker.start();
	std::mutex print_mutex;

	std::atomic<size_t> counter;
	counter.store(warp_count, std::memory_order_relaxed);
	for (size_t i = 0; i < warp_count; i++) {
		warps[i].queue_routine_external([&, i]() {
			for (size_t k = 0; k < warp_count; k++) {
				assert(i == grid_warp_t::get_current_warp() - &warps[0]);
				grid_warp_stack_guard<grid_warp_t> guard(warps[k]);
				std::lock_guard<std::mutex> print_guard(print_mutex);
				if (guard) {
					std::cout << "take warp " << k << " based on " << i << " success!" << std::endl;
				} else {
					std::cout << "take warp " << k << " based on " << i << " fail!" << std::endl;
				}
			}

			if (counter.fetch_sub(1, std::memory_order_release) == 1) {
				worker.terminate();
			}
		});
	}

	worker.join();
}

void not_pow_two() {
	struct pos_t {
		pos_t(float xx, float yy, float zz) : x(xx), y(yy), z(zz) {}
		float x, y, z;
	};

	grid_queue_list_t<pos_t> data;
	data.push(pos_t(1, 2, 3));
	data.push(pos_t(1, 2, 3));
	pos_t d = data.top();
	data.pop();
}

void framed_data() {
	std::cout << "[[ demo for grid dispatcher : framed_data ]] " << std::endl;

	grid_queue_list_t<int> data;

	int temp[4] = { 5, 8, 13, 21 };
	for (size_t j = 0; j < 256; j++) {
		data.push(temp, temp + 4);

		// thread 1
		grid_queue_frame_t<decltype(data)> q(data);
		q.push(1);
		q.push(2);
		q.release();
		q.push(3);
		q.push(4);
		q.push(5);
		q.release();
		q.push(6);
		q.release();

		int other[4];
		data.pop(other, other + 4);
		for (size_t k = 0; k < 4; k++) {
			assert(other[k] == temp[k]);
		}

		// thread 2
		int i = 0;
		while (q.acquire()) {
			std::cout << "frame " << i++ << std::endl;

			for (auto&& x : q) {
				std::cout << x << std::endl;
			}
		}
	}
}

template <typename element_t>
using worker_allocator_t = grid_object_allocator_t<element_t>;

void simple_explosion(void) {
	static constexpr size_t thread_count = 4;
	static constexpr size_t warp_count = 8;

	using worker_t = grid_async_worker_t<std::thread, size_t, std::function<void()>, worker_allocator_t>;
	using warp_t = grid_warp_t<worker_t>;

	worker_t worker(thread_count);
	grid_async_balancer_t<worker_t> balancer(worker);
	balancer.down();
	balancer.up();

	std::promise<bool> started;

	std::future<bool> future = started.get_future();
	size_t i = worker.get_thread_count();
	worker.append([&future, &started, &worker, i]() mutable {
		// copied from grid_async_worker_t<>::start() thread routine
		try {
			future.get();

			worker_t::get_current() = &worker;
			worker_t::get_current_thread_index_internal() = i;
			std::cout << "[[ external thread running ... ]]" << std::endl;

			while (!worker.is_terminated()) {
				if (!worker.poll(1)) {
					worker.delay(20);
				} else {
					std::cout << "[[ external thread has polled a task ... ]]" << std::endl;
				}
			}

			std::cout << "[[ external thread exited ... ]]" << std::endl;
		} catch (std::bad_alloc&) {
			throw; // by default, terminate
		} catch (std::exception&) {
			throw;
		}
	});

	worker.start();
	started.set_value(true);

	std::vector<warp_t> warps;
	warps.reserve(warp_count);
	for (size_t i = 0; i < warp_count; i++) {
		warps.emplace_back(worker);
	}

	srand((unsigned int)time(nullptr));
	std::cout << "[[ demo for grid dispatcher : simple_explosion ]] " << std::endl;

	static int32_t warp_data[warp_count] = { 0 };
	static constexpr size_t split_count = 4;
	static constexpr size_t terminate_factor = 100;
	static constexpr size_t parallel_factor = 11;
	static constexpr size_t parallel_count = 6;

	std::function<void()> explosion;

	// queue tasks randomly to test if dispatcher could handle them correctly.
	explosion = [&warps, &explosion, &worker]() {
		if (worker.is_terminated())
			return;

		warp_t& current_warp = *warp_t::get_current_warp();
		size_t warp_index = &current_warp - &warps[0];
		warp_data[warp_index]++;

		// simulate working
		std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 40));
		warp_data[warp_index]++;

		if (rand() % terminate_factor == 0) {
			// randomly terminates
			worker.terminate();
		}

		warp_data[warp_index]++;
		// randomly dispatch to warp
		for (size_t i = 0; i < split_count; i++) {
			warps[rand() % warp_count].queue_routine(explosion);
		}

		warp_data[warp_index] -= 3;

		if (rand() % parallel_factor == 0) {
			// read-write lock example: multiple reading blocks writing
			std::shared_ptr<std::atomic<int32_t>> shared_value = std::make_shared<std::atomic<int32_t>>(-0x7fffffff);
			for (size_t i = 0; i < parallel_count; i++) {
				current_warp.queue_routine_parallel([shared_value, warp_index]() {
					// only read operations
					std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 40));
					int32_t v = shared_value->exchange(warp_data[warp_index], std::memory_order_release);
					assert(v == warp_data[warp_index] || v == -0x7fffffff);
				}, 1);
			}
		}
	};

	// invoke explosion from external thread (current thread is external to the threads in thread pool)
	warps[0].queue_routine_external(explosion);
	worker.join();

	// finished!
	warp_t::join(warps.begin(), warps.end());

	std::cout << "after: " << std::endl;
	for (size_t k = 0; k < warp_count; k++) {
		std::cout << "warp " << k << " : " << warp_data[k] << std::endl;
	}
}

void garbage_collection() {
	static constexpr size_t thread_count = 8;
	static constexpr size_t warp_count = 16;
	grid_async_worker_t<> worker(thread_count);
	using grid_warp_t = grid_warp_t<grid_async_worker_t<>>;
	worker.start();

	std::vector<grid_warp_t> warps;
	warps.reserve(warp_count);
	for (size_t i = 0; i < warp_count; i++) {
		warps.emplace_back(worker);
	}

	srand((unsigned int)time(nullptr));
	std::cout << "[[ demo for grid dispatcher : garbage_collection ]] " << std::endl;
	struct node_t {
		size_t warp_index = 0;
		size_t visit_count = 0; // we do not use std::atomic<> here.
		std::vector<size_t> references;
	};

	struct graph_t {
		std::vector<node_t> nodes;
	};

	// randomly initialize connections.
	static constexpr size_t node_count = 4096;
	static constexpr size_t max_node_connection = 5;
	static constexpr size_t extra_node_connection_root = 20;

	graph_t graph;
	graph.nodes.reserve(node_count);

	for (size_t i = 0; i < node_count; i++) {
		node_t node;
		node.warp_index = rand() % warp_count;

		size_t connection = rand() % max_node_connection;
		node.references.reserve(connection);

		for (size_t k = 0; k < connection; k++) {
			node.references.emplace_back(rand() % node_count); // may connected to it self
		}

		graph.nodes.emplace_back(std::move(node));
	}

	// select random root
	size_t root_index = rand() % node_count;

	// ok now let's start collect from root!
	std::function<void(size_t)> collector;
	std::atomic<size_t> collecting_count;
	collecting_count.store(0, std::memory_order_release);

	collector = [&warps, &collector, &worker, &graph, &collecting_count](size_t node_index) {
		grid_warp_t& current_warp = *grid_warp_t::get_current_warp();
		size_t warp_index = &current_warp - &warps[0];

		node_t& node = graph.nodes[node_index];
		assert(node.warp_index == warp_index);

		if (node.visit_count == 0) {
			node.visit_count++;

			for (size_t i = 0; i < node.references.size(); i++) {
				size_t next_node_index = node.references[i];
				size_t next_node_warp = graph.nodes[next_node_index].warp_index;
				collecting_count.fetch_add(1, std::memory_order_acquire);
				warps[next_node_warp].queue_routine(std::bind(collector, next_node_index));
			}
		}

		if (collecting_count.fetch_sub(1, std::memory_order_release) == 1) {
			// all work finished.
			size_t collected_count = 0;
			for (size_t k = 0; k < graph.nodes.size(); k++) {
				node_t& node = graph.nodes[k];
				assert(node.visit_count < 2);
				collected_count += node.visit_count;
				node.visit_count = 0;
			}

			std::cout << "garbage_collection finished. " << collected_count << " of " << graph.nodes.size() << " collected." << std::endl;
			worker.terminate();
		}
	};

	collecting_count.fetch_add(1, std::memory_order_acquire);
	// add more references to root
	for (size_t j = 0; j < extra_node_connection_root; j++) {
		graph.nodes[root_index].references.emplace_back(rand() % node_count);
	}

	// invoke explosion from external thread (current thread is external to the threads in thread pool)
	warps[graph.nodes[root_index].warp_index].queue_routine_external(std::bind(collector, root_index));
	worker.join();

	// finished!
	grid_warp_t::join(warps.begin(), warps.end());
}

void graph_dispatch() {
	static constexpr size_t thread_count = 8;
	static constexpr size_t warp_count = 16;
	grid_async_worker_t<> worker(thread_count);
	using grid_warp_t = grid_warp_t<grid_async_worker_t<>>;
	worker.start();

	std::vector<grid_warp_t> warps;
	warps.reserve(warp_count);
	for (size_t i = 0; i < warp_count; i++) {
		warps.emplace_back(worker);
	}

	std::cout << "[[ demo for grid dispatcher : graph_dispatch ]] " << std::endl;

	size_t count = 2048;
	std::atomic<size_t> task_count;
	task_count.store(0, std::memory_order_release);

	grid_dispatcher_t<grid_warp_t> dispatcher(worker, [&count, &worker, &task_count](grid_dispatcher_t<grid_warp_t>& dispatcher) {
		assert(task_count.load(std::memory_order_acquire) == 0);
		if (--count == 0) {
			worker.terminate();
		} else {
			std::cout << "-------------------------------" << std::endl;
			task_count.fetch_add(4, std::memory_order_release);
			dispatcher.flush();
		}
	});

	std::atomic<bool> mark;
	mark.store(false, std::memory_order_release);

	size_t d = dispatcher.queue_routine(&warps[2], [&mark, &task_count]() {
		assert(mark.exchange(false, std::memory_order_release));
		task_count.fetch_sub(1, std::memory_order_release);
		std::cout << "Warp 2 task [4]" << std::endl;
	});

	size_t a = dispatcher.queue_routine(&warps[0], [&task_count]() {
		task_count.fetch_sub(1, std::memory_order_release);
		std::cout << "Warp 0 task [1]" << std::endl;
	});

	size_t b = dispatcher.queue_routine(&warps[1], [&dispatcher, &mark, d, &task_count]() {
		assert(!mark.exchange(true, std::memory_order_acq_rel));
		task_count.fetch_sub(1, std::memory_order_release);
		dispatcher.suspend(d);
		std::cout << "Warp 1 task [2]" << std::endl;
		dispatcher.resume(d);
	});

	dispatcher.order(a, b);
	// dispatcher.order(b, a); // trigger validate assertion

	size_t c = dispatcher.queue_routine(grid_warp_t::null(), [&task_count]() {
		task_count.fetch_sub(1, std::memory_order_release);
		std::cout << "Warp nil task [3]" << std::endl;
	});
	dispatcher.order(b, c);
	// dispatcher.order(c, a);// trigger validate assertion
	dispatcher.order(b, d);

	worker.queue([&dispatcher, &task_count]() {
		task_count.fetch_add(4, std::memory_order_release);
		dispatcher.flush();
	});

	worker.join();

	// finished!
	grid_warp_t::join(warps.begin(), warps.end());
}

