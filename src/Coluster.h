// Coluster.h
// PaintDream (paintdream@paintdream.com)
// 2022-12-31
//

#pragma once

#include <cassert>
#include <span>
#include <string>

#define lua_assert assert
#ifdef _MSC_VER
#pragma warning(disable:4201)
#pragma warning(disable:4324)
#pragma warning(disable:4389)
#pragma warning(disable:4127)
#pragma warning(disable:4100)
#pragma warning(disable:4702)
#endif

#include "../ref/iris/src/iris_coroutine.h"
#include "../ref/iris/src/iris_buffer.h"
#include "../ref/iris/src/iris_lua.h"

#ifdef COLUSTER_EXPORT
	#ifdef __GNUC__
		#define COLUSTER_API __attribute__ ((visibility ("default")))
	#else
		#define COLUSTER_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define COLUSTER_API __attribute__ ((visibility ("default")))
	#else
		#define COLUSTER_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif

namespace coluster {
	using LuaState = iris::iris_lua_t;
	template <typename quantity_t, size_t n>
	using Quota = iris::iris_quota_t<quantity_t, n>;
	template <typename quota_t, typename warp_t, typename async_worker_t>
	using QuotaQueue = iris::iris_quota_queue_t<quota_t, warp_t, async_worker_t>;
	using EnableReadWriteFence = iris::enable_read_write_fence_t<>;
	using EnableInOutFence = iris::enable_in_out_fence_t<>;

	template <typename element_t>
	using QueueList = iris::iris_queue_list_t<element_t, iris::iris_default_block_allocator_t, iris::iris_default_block_allocator_t, true>;
	template <typename element_t>
	using QueueListRelaxed = iris::iris_queue_list_t<element_t, iris::iris_default_block_allocator_t, iris::iris_default_block_allocator_t, false>;
	template <typename interface_t, typename element_t>
	using Pool = iris::iris_pool_t<interface_t, iris::iris_queue_list_t<element_t>>;
	using Cache = iris::iris_cache_t<uint8_t>;

	using Ref = LuaState::ref_t;
	template <typename element_t>
	using RefPtr = LuaState::refptr_t<element_t>;
	template <typename element_t>
	using Required = LuaState::required_t<element_t>;

	enum Priority : size_t {
		Priority_Highest = 0,
		Priority_Normal,
		Priority_Lowest,
		Priority_Count
	};

	enum QuotaType : size_t {
		QuotaType_HostMemory = 0,
		QuotaType_DeviceMemory,
		QuotaType_Count
	};

	struct Warp;
	struct AsyncWorker : iris::iris_async_worker_t<> {
		using Base = iris::iris_async_worker_t<>;
		using MemoryQuota = Quota<size_t, QuotaType_Count>; // Main Memory & Device Memory
		using MemoryQuotaQueue = QuotaQueue<MemoryQuota, Warp, AsyncWorker>;
		AsyncWorker();

		COLUSTER_API MemoryQuotaQueue& GetMemoryQuotaQueue() noexcept;

	protected:
		MemoryQuota memoryQuota;
		MemoryQuotaQueue memoryQuotaQueue;
	};

	struct Warp : iris::iris_warp_t<AsyncWorker> {
		using Base = iris::iris_warp_t<AsyncWorker>;
		
		struct SwitchWarp : iris::iris_switch_t<Warp> {
			using Base = iris::iris_switch_t<Warp>;
			COLUSTER_API explicit SwitchWarp(Warp* target, Warp* other) noexcept;
			COLUSTER_API bool await_ready() const noexcept;
			COLUSTER_API void await_suspend(std::coroutine_handle<> handle);
			COLUSTER_API Warp* await_resume() const noexcept;

		protected:
			lua_State* luaState;
			Warp* luaWarp;
		};

		COLUSTER_API static Warp* get_current_warp() noexcept;
		COLUSTER_API AsyncWorker& get_async_worker() noexcept;
		COLUSTER_API static SwitchWarp Switch(Warp* target, Warp* other = nullptr) noexcept;
		COLUSTER_API explicit Warp(AsyncWorker& asyncWorker) : Base(asyncWorker) { assert(!asyncWorker.is_terminated()); }
		COLUSTER_API void BindLuaCoroutine(void* address) noexcept;
		COLUSTER_API void UnbindLuaCoroutine() noexcept;
		COLUSTER_API void BindLuaRoot(lua_State* L) noexcept;

	protected:
		void ChainWait(Warp* target, Warp* other);
		void ChainEnter(Warp* from);
		lua_State* hostState = nullptr;
	};

	using RootAlloator = std::remove_reference_t<decltype(coluster::AsyncWorker::task_allocator_t::get_root_allocator())>;

	template <typename return_t = void>
	struct Coroutine : iris::iris_coroutine_t<return_t> {
		using Base = iris::iris_coroutine_t<return_t>;
		template <typename promise_type>
		explicit Coroutine(std::coroutine_handle<promise_type>&& h) : Base(std::move(h)), currentWarp(Warp::get_current_warp()) {}
		Coroutine(iris::iris_coroutine_t<return_t>&& co) : Base(std::move(co)), currentWarp(Warp::get_current_warp()) {}

		template <typename func_t>
		Coroutine& complete(func_t&& func) noexcept {
			currentWarp->BindLuaCoroutine(Base::get_handle().address());

			if constexpr (!std::is_void_v<return_t>) {
				Base::complete([
#ifdef _DEBUG
					currentWarp = currentWarp, 
#endif
				func = std::forward<func_t>(func)](return_t&& value) mutable {
					Warp* warp = Warp::get_current_warp();
					warp->UnbindLuaCoroutine();
#ifdef _DEBUG
					assert(currentWarp == warp);
#endif
					func(std::move(value));
				});
			} else {
				Base::complete([
#ifdef _DEBUG
					currentWarp = currentWarp, 
#endif
				func = std::forward<func_t>(func)]() mutable {
					Warp* warp = Warp::get_current_warp();
					warp->UnbindLuaCoroutine();
#ifdef _DEBUG
					assert(currentWarp == warp);
#endif
					func();
				});
			}

			return *this;
		}

		void run() noexcept(noexcept(Base::run())) {
			Base::run();
			currentWarp->UnbindLuaCoroutine();
		}

	protected:
		Warp* currentWarp;
	};

	template <typename return_t = void>
	using CoroutineHandle = std::coroutine_handle<return_t>;
	using AsyncEvent = iris::iris_event_t<Warp, AsyncWorker>;
	using AsyncBarrier = iris::iris_barrier_t<Warp, AsyncWorker>;
	using AsyncFrame = iris::iris_frame_t<Warp, AsyncWorker>;	

	struct AutoAsyncWorker : LuaState::require_base_t {
		struct Holder {
			operator bool() const noexcept {
				return Warp::get_current_warp() != nullptr;
			}

			operator AutoAsyncWorker () const noexcept {
				return AutoAsyncWorker();
			}
		};

		using internal_type_t = Holder;
		Holder get() const noexcept {
			return Holder();
		}

		operator Holder () const noexcept {
			return get();
		}

		operator AsyncWorker& () const noexcept {
			Warp* currentWarp = Warp::get_current_warp();
			assert(currentWarp != nullptr);
			return currentWarp->get_async_worker();
		}
	};

	struct Guid : public std::pair<uint64_t, uint64_t> {
		Guid(uint64_t a = 0, uint64_t b = 0) noexcept : std::pair<uint64_t, uint64_t>(a, b) {}
		static Guid Generate() noexcept;
	};
}


namespace iris {
	declare_shared_static_instance(coluster::Warp::Base*, COLUSTER_API);
	declare_shared_static_instance(coluster::AsyncWorker::thread_index_t, COLUSTER_API);
	declare_shared_static_instance(coluster::RootAlloator, COLUSTER_API);

	template <>
	struct iris_lua_convert_t<coluster::AutoAsyncWorker::Holder> {
		static constexpr bool value = true;
		static coluster::AutoAsyncWorker from_lua(lua_State* L, int index) {
			return coluster::AutoAsyncWorker();
		}

		static int to_lua(lua_State* L, coluster::AutoAsyncWorker) {
			lua_pushnil(L);
			return 1;
		}
	};
}

namespace std {
	template <>
	struct hash<coluster::Guid> {
		size_t operator () (const coluster::Guid& key) const noexcept {
			return static_cast<size_t>(key.second);
		}
	};
}

