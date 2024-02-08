// Coluster.h
// PaintDream (paintdream@paintdream.com)
// 2022-12-31
//

#pragma once

#include <cassert>
#include <span>
#include <string>
#include <source_location>

#define lua_assert assert
#ifdef _MSC_VER
#pragma warning(disable:4201)
#pragma warning(disable:4324)
#pragma warning(disable:4389)
#pragma warning(disable:4127)
#pragma warning(disable:4100)
#pragma warning(disable:4702)
#endif

#ifdef COLUSTER_EXPORT
	#ifdef __GNUC__
		#define COLUSTER_CORE_API __attribute__ ((visibility ("default")))
	#else
		#define COLUSTER_CORE_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define COLUSTER_CORE_API __attribute__ ((visibility ("default")))
	#else
		#define COLUSTER_CORE_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif

#if !COLUSTER_MONOLITHIC
#define COLUSTER_API COLUSTER_CORE_API
#else
#define COLUSTER_API
#endif

#define IRIS_SHARED_LIBRARY_DECORATOR COLUSTER_API

#include "../ref/iris/src/iris_coroutine.h"
#include "../ref/iris/src/iris_buffer.h"
#include "../ref/iris/src/iris_lua.h"
#include "../ref/iris/src/iris_tree.h"

namespace coluster {
	using LuaState = iris::iris_lua_t;
	using EnableReadWriteFence = iris::enable_read_write_fence_t<>;
	using EnableInOutFence = iris::enable_in_out_fence_t<>;

	template <typename element_t>
	using QueueList = iris::iris_queue_list_t<element_t, iris::iris_default_block_allocator_t, iris::iris_default_block_allocator_t, true>;
	template <typename element_t>
	using QueueListRelaxed = iris::iris_queue_list_t<element_t, iris::iris_default_block_allocator_t, iris::iris_default_block_allocator_t, false>;
	template <typename interface_t, typename element_t, size_t block_size = iris::default_block_size>
	using Pool = iris::iris_pool_t<interface_t, iris::iris_queue_list_t<element_t>, block_size>;
	using Cache = iris::iris_cache_t<uint8_t>;

	using Ref = LuaState::ref_t;
	template <typename element_t>
	using RefPtr = LuaState::refptr_t<element_t>;
	template <typename element_t>
	using Required = LuaState::required_t<element_t>;

	template <typename tree_key_t>
	using Tree = iris::iris_tree_t<tree_key_t>;
	template <typename element_t, typename prim_type = typename element_t::first_type, size_t element_count = prim_type::size * 2>
	using TreeOverlap = iris::iris_overlap_t<element_t>;

	enum Priority : size_t {
		Priority_Highest = 0,
		Priority_Normal,
		Priority_Lowest,
		Priority_Count
	};

	COLUSTER_API void SetCurrentCoroutineAddress(void* address) noexcept;
	COLUSTER_API void* GetCurrentCoroutineAddress() noexcept;

	template <typename quantity_t, size_t n>
	using Quota = iris::iris_quota_t<quantity_t, n>;
	template <typename quota_t, typename warp_t, typename async_worker_t>
	struct QuotaQueue : iris::iris_quota_queue_t<quota_t, warp_t, async_worker_t> {
		using Base = iris::iris_quota_queue_t<quota_t, warp_t, async_worker_t>;
		QuotaQueue(async_worker_t& worker, quota_t& q) : Base(worker, q) {}

		struct awaitable_t : Base::awaitable_t {
			using BaseAwaitable = typename Base::awaitable_t;
			awaitable_t(QuotaQueue& q, const typename Base::amount_t& m, bool r) noexcept : BaseAwaitable(q, m, r), coroutineAddress(GetCurrentCoroutineAddress()) {
				SetCurrentCoroutineAddress(nullptr);
			}
		
			auto await_resume() noexcept {
				SetCurrentCoroutineAddress(coroutineAddress);
				return BaseAwaitable::await_resume();
			}

		protected:
			void* coroutineAddress;
		};

		awaitable_t guard(const typename Base::amount_t& amount) {
			return awaitable_t(*this, amount, Base::quota.acquire(amount));
		}

		typename Base::amount_t GetAmount() const noexcept {
			return Base::quota.get();
		}
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
		COLUSTER_API void Synchronize(LuaState lua, Warp* warp);
		Warp* GetScriptWarp() const noexcept {
			return scriptWarp.get();
		}

		const std::vector<std::shared_ptr<Warp>>& GetSharedWarps() const noexcept {
			return sharedWarps;
		}

	protected:
		void SetupSharedWarps(size_t count);

		std::unique_ptr<Warp> scriptWarp;
		std::vector<std::shared_ptr<Warp>> sharedWarps;
		MemoryQuota memoryQuota;
		MemoryQuotaQueue memoryQuotaQueue;
	};

	struct Warp : iris::iris_warp_t<AsyncWorker> {
		using Base = iris::iris_warp_t<AsyncWorker>;
		
		struct SwitchWarp : iris::iris_switch_t<Warp> {
			using Base = iris::iris_switch_t<Warp>;
			COLUSTER_API explicit SwitchWarp(const std::source_location& source, Warp* target, Warp* other) noexcept;
			COLUSTER_API bool await_ready() const noexcept;
			COLUSTER_API void await_suspend(std::coroutine_handle<> handle);
			COLUSTER_API Warp* await_resume() const noexcept;

		protected:
			void* coroutineAddress;
		};

		COLUSTER_API static Warp* get_current_warp() noexcept;
		COLUSTER_API static SwitchWarp Switch(const std::source_location& source, Warp* target, Warp* other = nullptr) noexcept;
		COLUSTER_API AsyncWorker& get_async_worker() noexcept;
		COLUSTER_API explicit Warp(AsyncWorker& asyncWorker) : Base(asyncWorker) { assert(!asyncWorker.is_terminated()); }
		COLUSTER_API void BindLuaRoot(lua_State* L) noexcept;
		COLUSTER_API void UnbindLuaRoot(lua_State* L) noexcept;
		COLUSTER_API void Acquire();
		COLUSTER_API void Release();

		const Ref& GetProfileTable() const noexcept {
			return profileTable;
		}

		COLUSTER_API static void ChainWait(const std::source_location& source, Warp* from, Warp* target, Warp* other);
		COLUSTER_API static void ChainEnter(Warp* from, Warp* target, Warp* other);

	protected:
		lua_State* hostState = nullptr;
		Ref profileTable;
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
			SetCurrentCoroutineAddress(Base::get_handle().address());

			if constexpr (!std::is_void_v<return_t>) {
				Base::complete([
#ifdef _DEBUG
					currentWarp = currentWarp, 
#endif
				func = std::forward<func_t>(func)](return_t&& value) mutable {
					SetCurrentCoroutineAddress(nullptr);
#ifdef _DEBUG
					assert(currentWarp == Warp::get_current_warp());
#endif
					func(std::move(value));
				});
			} else {
				Base::complete([
#ifdef _DEBUG
					currentWarp = currentWarp, 
#endif
				func = std::forward<func_t>(func)]() mutable {
					SetCurrentCoroutineAddress(nullptr);
#ifdef _DEBUG
					assert(currentWarp == Warp::get_current_warp());
#endif
					func();
				});
			}

			return *this;
		}

		void run() {
			Base::run();
			SetCurrentCoroutineAddress(nullptr);
		}

	protected:
		Warp* currentWarp;
	};

	template <typename return_t = void>
	using CoroutineHandle = std::coroutine_handle<return_t>;
	using AsyncEvent = iris::iris_event_t<Warp, AsyncWorker>;
	using AsyncBarrier = iris::iris_barrier_t<Warp, AsyncWorker>;
	template <typename element_t>
	using AsyncPipe = iris::iris_pipe_t<element_t, Warp>;

	struct AutoAsyncWorker : LuaState::required_base_t {
		struct Holder {
			operator bool() const noexcept {
				Warp* warp = Warp::get_current_warp();
				return warp != nullptr && !warp->get_async_worker().is_terminated();
			}

			operator AutoAsyncWorker () const noexcept {
				return AutoAsyncWorker();
			}
		};

		using required_type_t = Holder;
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

	class Object {
	public:
		COLUSTER_API Object() noexcept;
		COLUSTER_API virtual ~Object() noexcept;
		COLUSTER_API virtual Warp* GetObjectWarp() const noexcept;
	};
}

namespace iris {
	declare_shared_static_instance(coluster::Warp::Base*);
	declare_shared_static_instance(coluster::AsyncWorker::thread_index_t);
	declare_shared_static_instance(coluster::RootAlloator);

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

