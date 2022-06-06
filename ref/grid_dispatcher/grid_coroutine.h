/*
Grid-based Task Dispatcher System

This software is a C++ 20 Header-Only reimplementation of Kernel.h from Project PaintsNow.

The MIT License (MIT)

Copyright (c) 2014-2022 PaintDream

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#pragma once

#include "grid_dispatcher.h"

// C++20 coroutine support
#if defined(__clang__)
#include <experimental/coroutine>
#else
#include <coroutine>
#endif

namespace grid {
#if defined(__clang__)
	template <typename type>
	using coroutine_handle = std::experimental::coroutine_handle<type>;
	using suspend_never = std::experimental::suspend_never;
	using suspend_always = std::experimental::suspend_always;
#else
	template <typename type>
	using coroutine_handle = std::coroutine_handle<type>;
	using suspend_never = std::suspend_never;
	using suspend_always = std::suspend_always;
#endif

	// standard coroutine interface settings
	struct grid_coroutine_t {
		struct promise_type {
			grid_coroutine_t get_return_object() noexcept {
				return grid_coroutine_t(coroutine_handle<promise_type>::from_promise(*this));
			}

			constexpr suspend_always initial_suspend() noexcept { return suspend_always(); }
			constexpr suspend_never final_suspend() noexcept { return suspend_never(); }

			void return_void() noexcept {
				if (completion)
					completion();
			}

			void unhandled_exception() noexcept { return std::terminate(); }
			std::function<void()> completion;
		};

		grid_coroutine_t(coroutine_handle<promise_type>&& h) : handle(std::move(h)) {}
		grid_coroutine_t(const grid_coroutine_t& rhs) = delete;
		grid_coroutine_t(grid_coroutine_t&& rhs) noexcept : handle(std::move(rhs.handle)) {
			rhs.handle = coroutine_handle<promise_type>();
		}

		grid_coroutine_t& operator = (const grid_coroutine_t& rhs) = delete;
		grid_coroutine_t& operator = (grid_coroutine_t&& rhs) noexcept {
			std::swap(handle, rhs.handle);
			rhs.handle = coroutine_handle<promise_type>();
			return *this;
		}

		~grid_coroutine_t() {
			assert(!handle); // must call run() or join() before destruction
		}

		template <typename func_t>
		grid_coroutine_t& complete(func_t&& func) noexcept {
			assert(handle);
			handle.promise().completion = std::move(func);
			return *this;
		}

		void run() noexcept(noexcept(std::declval<coroutine_handle<promise_type>>().resume())) {
			assert(handle);
			handle.resume();
			handle = coroutine_handle<promise_type>();
		}

		template <typename type>
		std::atomic<type>& run(std::atomic<type>& variable, type value) {
			assert(handle);

			complete([&variable, value]() {
				variable.store(value, std::memory_order_relaxed);
				variable.notify_one();
			});

			run();
			return variable; // chain result
		}

		void join() {
			std::atomic<size_t> variable = 0;
			run(variable, (size_t)1).wait(0, std::memory_order_acquire);
		}

	protected:
		coroutine_handle<promise_type> handle;
	};

	using grid_coroutine_handle = coroutine_handle<grid_coroutine_t::promise_type>;

	// awaitable object, can be used by:
	// co_await grid_awaitable_t(...);
	template <typename warp_type_t, typename func_type_t>
	struct grid_awaitable_t {
		using warp_t = warp_type_t;
		using func_t = func_type_t;
		using return_t = std::invoke_result_t<func_t>;

		// constructed from a given target warp and routine function
		// notice that we do not initialize `caller` here, let `await_suspend` do
		template <class callable_t>
		grid_awaitable_t(warp_t* target_warp, callable_t&& f, size_t p) noexcept : target(target_warp), parallel(p), func(std::forward<callable_t>(f)) {
			assert(target_warp != nullptr || parallel == ~(size_t)0);
		}

		// always suspended
		bool await_ready() const noexcept {
			return false;
		}

		void resume_one(grid_coroutine_handle handle) {
			// return to caller's warp
			if (caller != nullptr) {
				// notice that the condition `caller != target` holds
				// so we can use `post` to skip self-queueing check
				caller->queue_routine_post([this, handle = std::move(handle)]() mutable noexcept(noexcept(handle.resume())) {
					handle.resume();
				});
			} else {
				// otherwise dispatch to thread pool
				// notice that we mustn't call handle.resume() directly
				// since it may blocks execution of current warp
				target->get_async_worker().queue([handle = std::move(handle)]() mutable noexcept(noexcept(handle.resume())) {
					handle.resume();
				});
			}
		}

		void await_suspend(grid_coroutine_handle handle) {
			caller = warp_t::get_current_warp();

			// the same warp, execute at once!
			// even they are both nullptr
			if (target == caller) {
				if constexpr (std::is_void_v<return_t>) {
					func();
				} else {
					ret = func(); // auto moved here
				}

				handle.resume(); // resume coroutine directly.
			} else if (target == nullptr) {
				// targeting to thread pool with no warp context
				caller->get_async_worker().queue([this, handle = std::move(handle)]() mutable {
					if constexpr (std::is_void_v<return_t>) {
						func();
					} else {
						ret = func();
					}

					// return to caller's warp
					// notice that we are running in one thread of our thread pool, so just use queue_routine
					caller->queue_routine([this, handle = std::move(handle)]() mutable noexcept(noexcept(handle.resume())) {
						handle.resume();
					});
				});
			} else {
				if (parallel == ~(size_t)0) {
					// targeting to a valid warp
					// prepare callback first
					auto callback = [this, handle = std::move(handle)]() mutable noexcept(noexcept(func()) && noexcept(std::declval<grid_awaitable_t>().resume_one(handle))) {
						if constexpr (std::is_void_v<return_t>) {
							func();
						} else {
							ret = func();
						}

						resume_one(handle);
					};

					// if we are in an external thread, post to thread pool first
					// otherwise post it directly
					if (target->get_async_worker().get_current_thread_index() != ~(size_t)0) {
						target->queue_routine_post(std::move(callback));
					} else {
						target->queue_routine_external(std::move(callback));
					}
				} else {
					target->suspend();

					typename warp_t::suspend_guard_t guard(target);
					target->get_async_worker().queue([this, handle = std::move(handle)]() mutable noexcept(noexcept(func()) && noexcept(std::declval<grid_awaitable_t>().resume_one(handle)) && noexcept(target->resume())) {
						typename warp_t::suspend_guard_t guard(target);
						if constexpr (std::is_void_v<return_t>) {
							func();
						} else {
							ret = func();
						}

						guard.cleanup();
						target->resume();

						resume_one(handle);
					}, parallel);

					guard.cleanup();
				}
			}
		}

		return_t await_resume() noexcept {
			if constexpr (!std::is_void_v<return_t>) {
				return std::move(ret);
			}
		}

		struct void_t {};
		warp_t* caller;
		warp_t* target;
		size_t parallel;
		func_t func;
		std::conditional_t<std::is_void_v<return_t>, void_t, return_t> ret;
	};

	// simple wrapper for constructing an awaitable object
	template <typename warp_t, typename grid_func_t>
	auto grid_awaitable(warp_t* target_warp, grid_func_t&& func) noexcept {
		return grid_awaitable_t<warp_t, std::decay_t<grid_func_t>>(target_warp, std::forward<grid_func_t>(func), ~(size_t)0);
	}

	// simple wrapper for constructing an awaitable object in parallel
	template <typename warp_t, typename grid_func_t>
	auto grid_awaitable_parallel(warp_t* target_warp, grid_func_t&& func, size_t priority = 0) noexcept {
		assert(priority != ~(size_t)0);
		return grid_awaitable_t<warp_t, std::decay_t<grid_func_t>>(target_warp, std::forward<grid_func_t>(func), priority);
	}

	// an awaitable proxy for combining multiple awaitable objects
	template <typename warp_t, typename grid_func_t>
	struct grid_awaitable_multiple_t {
		struct void_t {};
		using return_t = std::invoke_result_t<grid_func_t>;
		using return_multiple_t = std::conditional_t<std::is_void_v<return_t>, void_t, std::vector<return_t>>;
		using return_multiple_declare_t = std::conditional_t<std::is_void_v<return_t>, void, std::vector<return_t>>;
		using async_worker_t = typename warp_t::async_worker_t;

		struct awaitable_t {
			awaitable_t(warp_t* t, grid_func_t&& f, size_t p) : target(t), parallel(p), func(std::move(f)) {}

			warp_t* target;
			size_t parallel;
			grid_func_t func;
		};

		grid_awaitable_multiple_t(async_worker_t& worker) noexcept : caller(warp_t::get_current_warp()), async_worker(worker) {}

		void initialize_args() noexcept {}

		template <typename element_t, typename... args_t>
		void initialize_args(element_t&& first, args_t&&... args) {
			*this += std::forward<element_t>(first);
			initialize_args(std::forward<args_t>(args)...);
		}

		// can be initialized with series of awaitables
		// `pending_count` is not necessarily initialized here
		template <typename... args_t>
		grid_awaitable_multiple_t(async_worker_t& worker, args_t&&... args) : caller(warp_t::get_current_warp()), async_worker(worker) {
			awaitables.reserve(sizeof...(args));
			initialize_args(std::forward<args_t>(args)...);
		}

		// just make visual studio linter happy
		// atomic variables are not movable.
		grid_awaitable_multiple_t(grid_awaitable_multiple_t&& rhs) noexcept : caller(rhs.caller), async_worker(rhs.async_worker), awaitables(std::move(rhs.awaitables)), returns(rhs.returns) {}

		grid_awaitable_multiple_t& operator = (grid_awaitable_multiple_t&& rhs) noexcept {
			grid_awaitable_multiple_t t(std::move(rhs));
			std::swap(*this, t);
			return *this;
		}

		grid_awaitable_multiple_t& operator += (grid_awaitable_t<warp_t, grid_func_t>&& arg) {
			awaitables.emplace_back(awaitable_t(arg.target, std::move(arg.func), arg.parallel));
			return *this;
		}

		bool await_ready() const noexcept {
			return false;
		}

		void resume_one(grid_coroutine_handle handle) {
			assert(async_worker.get_current_thread_index() != ~(size_t)0);

			// if all sub-awaitables finished, then resume coroutine
			if (pending_count.fetch_sub(1, std::memory_order_acquire) == 1) {
				warp_t* warp = warp_t::get_current_warp();
				if (warp == caller) {
					// last finished one is the one who invoke coroutine
					// resume at once!
					handle.resume();
				} else {
					if (caller != nullptr) {
						// caller is a valid warp, post to it
						caller->queue_routine_post([this, handle = std::move(handle)]() mutable noexcept(noexcept(handle.resume())) {
							handle.resume();
						});
					} else {
						// caller is not a valid warp, post to thread pool
						async_worker.queue([this, handle = std::move(handle)]() mutable noexcept(noexcept(handle.resume())) {
							handle.resume();
						});
					}
				}
			}
		}

		void await_suspend(grid_coroutine_handle handle) {
			if constexpr (!std::is_void_v<return_t>) {
				returns.resize(awaitables.size());
			}

			assert(!current_handle); // can only be called once!
			current_handle = handle;
			// prepare pending counter here!
			pending_count.store(awaitables.size(), std::memory_order_release);

			for (size_t i = 0; i < awaitables.size(); i++) {
				awaitable_t& awaitable = awaitables[i];
				warp_t* target = awaitable.target;
				if (target == nullptr) {
					// target is thread pool
					async_worker.queue([this, i]() mutable {
						if constexpr (std::is_void_v<return_t>) {
							awaitables[i].func();
						} else {
							returns[i] = awaitables[i].func();
						}

						resume_one(current_handle);
					});
				} else {
					if (awaitable.parallel == ~(size_t)0) {
						// target is a valid warp
						// prepare callback
						auto callback = [this, i]() mutable {
							if constexpr (std::is_void_v<return_t>) {
								awaitables[i].func();
							} else {
								returns[i] = awaitables[i].func();
							}

							resume_one(current_handle);
						};

						// if we are in an external thread, post to thread pool first
						// otherwise post it directly
						if (async_worker.get_current_thread_index() != ~(size_t)0) {
							target->queue_routine_post(std::move(callback));
						} else {
							target->queue_routine_external(std::move(callback));
						}
					} else {
						target->suspend();

						typename warp_t::suspend_guard_t guard(target);
						async_worker.queue([this, i]() mutable {
							warp_t* target = awaitables[i].target;
							typename warp_t::suspend_guard_t guard(target);
							if constexpr (std::is_void_v<return_t>) {
								awaitables[i].func();
							} else {
								returns[i] = awaitables[i].func();
							}

							guard.cleanup();
							target->resume();
							resume_one(current_handle); // cleanup must happened before resume_one()!
						});

						guard.cleanup();
					}
				}
			}
		}

		// return all values by moving semantic
		return_multiple_declare_t await_resume() noexcept {
			if constexpr (!std::is_void_v<return_t>) {
				return std::move(returns);
			}
		}

	protected:
		warp_t* caller;
		async_worker_t& async_worker;
		std::atomic<size_t> pending_count;
		grid_coroutine_handle current_handle;
		std::vector<awaitable_t> awaitables;
		return_multiple_t returns;
	};

	// wrapper for joining multiple awaitables together
	template <typename async_worker_t, typename awaitable_t, typename... args_t>
	auto grid_awaitable_union(async_worker_t& worker, awaitable_t&& first, args_t&&... args) {
		return grid_awaitable_multiple_t<typename awaitable_t::warp_t, typename awaitable_t::func_t>(worker, std::forward<awaitable_t>(first), std::forward<args_t>(args)...);
	}

	template <typename warp_t>
	struct grid_switch_t {
		using async_worker_t = typename warp_t::async_worker_t;

		grid_switch_t(warp_t* warp) noexcept : source(warp_t::get_current_warp()), target(warp) {
			assert(target != nullptr || source != nullptr);
		}

		bool await_ready() const noexcept {
			return source == target;
		}

		void await_suspend(grid_coroutine_handle handle) {
			if (target == nullptr) {
				source->get_async_worker().queue([handle = std::move(handle)]() mutable noexcept(noexcept(handle.resume())) {
					handle.resume();
				});
			} else {
				if (target->get_async_worker().get_current_thread_index() != ~(size_t)0) {
					target->queue_routine_post([handle = std::move(handle)]() mutable noexcept(noexcept(handle.resume())) {
						handle.resume();
					});
				} else {
					target->queue_routine_external([handle = std::move(handle)]() mutable noexcept(noexcept(handle.resume())) {
						handle.resume();
					});
				}
			}
		}

		warp_t* await_resume() noexcept {
			return source;
		}

	protected:
		warp_t* source;
		warp_t* target;
	};

	template <typename warp_t>
	auto grid_switch(warp_t* target) noexcept {
		return grid_switch_t<warp_t>(target);
	}

	template <typename warp_t, typename async_worker_t>
	struct grid_sync_t {
	protected:
		grid_sync_t(async_worker_t& worker) : async_worker(worker) {}

		struct info_base_warp_t {
			grid_coroutine_handle handle;
			warp_t* warp;
		};

		struct info_base_t {
			grid_coroutine_handle handle;
		};

		using info_t = std::conditional_t<std::is_same_v<warp_t, void>, info_base_t, info_base_warp_t>;

		void dispatch(info_t&& info) {
			if constexpr (std::is_same_v<warp_t, void>) {
				async_worker.queue([handle = std::move(info.handle)]() mutable noexcept(noexcept(info.handle.resume())) {
					handle.resume();
				});
			} else {
				warp_t* target = info.warp;
				if (target == nullptr) {
					async_worker.queue([handle = std::move(info.handle)]() mutable noexcept(noexcept(info.handle.resume())) {
						handle.resume();
					});
				} else if (target->get_async_worker().get_current_thread_index() != ~(size_t)0) {
					target->queue_routine_post([handle = std::move(info.handle)]() mutable noexcept(noexcept(info.handle.resume())) {
						handle.resume();
					});
				} else {
					target->queue_routine_external([handle = std::move(info.handle)]() mutable noexcept(noexcept(info.handle.resume())) {
						handle.resume();
					});
				}
			}
		}

		async_worker_t& async_worker;
	};

	template <typename warp_t, typename async_worker_t>
	struct grid_barrier_t : public grid_sync_t<warp_t, async_worker_t> {
	public:
		grid_barrier_t(async_worker_t& worker, size_t yield_max_count) : grid_sync_t<warp_t, async_worker_t>(worker), yield_max(yield_max_count) {
			handles.resize(yield_max);
			resume_count.store(0, std::memory_order_relaxed);
			yield_count.store(0, std::memory_order_relaxed);
		}

		bool await_ready() const noexcept {
			return false;
		}

		void await_suspend(grid_coroutine_handle handle) {
			size_t index = yield_count.fetch_add(1, std::memory_order_acquire);
			assert(index < yield_max);
			auto& info = handles[index];
			info.handle = std::move(handle);

			if constexpr (!std::is_same_v<warp_t, void>) {
				info.warp = warp_t::get_current_warp();
			}

			if (index + 1 == yield_max) {
				yield_count.store(0, std::memory_order_relaxed);
				resume_count.store(0, std::memory_order_release);

				for (size_t i = 0; i < handles.size(); i++) {
					grid_sync_t<warp_t, async_worker_t>::dispatch(std::move(handles[i]));
				}
			}
		}

		size_t await_resume() noexcept {
			return resume_count.fetch_add(1, std::memory_order_acquire); // first resume!
		}

	protected:
		using info_t = typename grid_sync_t<warp_t, async_worker_t>::info_t;
		size_t yield_max;
		std::atomic<size_t> resume_count;
		std::atomic<size_t> yield_count;
		std::vector<info_t> handles;
	};

	// specify warp_t = void for warp-ignored dispatch
	template <typename warp_t, typename async_worker_t>
	auto grid_barrier(async_worker_t& worker, size_t yield_max_count) {
		return grid_barrier_t<warp_t, async_worker_t>(worker, yield_max_count);
	}

	template <typename warp_t, typename async_worker_t>
	struct grid_frame_t : public grid_sync_t<warp_t, async_worker_t> {
		grid_frame_t(async_worker_t& worker) : grid_sync_t<warp_t, async_worker_t>(worker) {}

		bool await_ready() const noexcept {
			return false;
		}

		void await_suspend(grid_coroutine_handle handle) {
			queue(std::move(handle));
		}

		void flush() {
			queue(grid_coroutine_handle());

			while (true) {
				info_t info = std::move(frame_coroutine_handles.top());
				frame_coroutine_handles.pop();

				if (info.handle) {
					grid_sync_t<warp_t, async_worker_t>::dispatch(std::move(info));
				} else {
					break;
				}
			}
		}

		void await_resume() noexcept {}

	protected:
		using info_t = typename grid_sync_t<warp_t, async_worker_t>::info_t;
		void queue(grid_coroutine_handle&& handle) {
			info_t info;
			info.handle = std::move(handle);
			if constexpr (!std::is_same_v<warp_t, void>) {
				info.warp = warp_t::get_current_warp();
			}

			std::lock_guard<std::mutex> guard(frame_mutex);
			frame_coroutine_handles.push(std::move(info));
		}

	protected:
		std::mutex frame_mutex;
		grid_queue_list_t<info_t> frame_coroutine_handles;
	};
}

