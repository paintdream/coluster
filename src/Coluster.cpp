#include "Coluster.h"
#ifdef _WIN32
#include <windows.h>
#pragma comment(lib, "Rpcrt4.lib")
#else
#include <uuid/uuid.h>
#endif

namespace iris {
	implement_shared_static_instance(coluster::Warp::Base*, COLUSTER_API);
	implement_shared_static_instance(coluster::AsyncWorker::thread_index_t, COLUSTER_API);
	implement_shared_static_instance(coluster::RootAlloator, COLUSTER_API);
}

namespace coluster {
	static constexpr size_t DEFAULT_HOST_MEMORY_BUDGET = 1024 * 1024 * 1024;
	static constexpr size_t DEFAULT_DEVICE_MEMORY_BUDGET = 1024 * 1024 * 1024;

	AsyncWorker::AsyncWorker() : memoryQuota({ DEFAULT_HOST_MEMORY_BUDGET, DEFAULT_DEVICE_MEMORY_BUDGET }), memoryQuotaQueue(*this, memoryQuota) {}
	AsyncWorker::MemoryQuotaQueue& AsyncWorker::GetMemoryQuotaQueue() noexcept {
		return memoryQuotaQueue;
	}

	void AsyncWorker::Synchronize(LuaState lua, Warp* warp) {
		if (scriptWarp) {
			assert(Warp::get_current_warp() == scriptWarp.get());
			lua_State* L = lua.get_state();
			while ((warp == nullptr || !warp->join()) || !scriptWarp->join() || poll()) {
				scriptWarp->Release();
				poll_delay(Priority_Count, 20);
				scriptWarp->Acquire();
			}
		} else if (warp != nullptr) {
			while (!warp->join()) {}
		}
	}

	static thread_local lua_State* CurrentLuaThread = nullptr;
	static thread_local Warp* CurrentLuaWarp = nullptr;

	Warp* Warp::get_current_warp() noexcept {
		return static_cast<Warp*>(Base::get_current_warp());
	}

	AsyncWorker& Warp::get_async_worker() noexcept {
		return Base::get_async_worker();
	}

	lua_State* Warp::GetCurrentLuaThread() noexcept {
		return CurrentLuaThread;
	}

	Warp* Warp::GetCurrentLuaWarp() noexcept {
		return CurrentLuaWarp;
	}

	Warp::SwitchWarp Warp::Switch(Warp* target, Warp* other) noexcept {
		return SwitchWarp(target, other);
	}

	void Warp::BindLuaRoot(lua_State* L) noexcept {
		hostState = L;

		// create cache table
		LuaState::stack_guard_t guard(L);
		lua_pushlightuserdata(L, GetCacheKey());
		lua_newtable(L);
		lua_newtable(L);
		lua_pushliteral(L, "v");
		lua_setfield(L, -2, "__mode");
		lua_setmetatable(L, -2);
		lua_rawset(L, LUA_REGISTRYINDEX);

		// create bind table
		lua_pushlightuserdata(L, GetBindKey());
		lua_newtable(L);
		lua_rawset(L, LUA_REGISTRYINDEX);
	}

	void Warp::UnbindLuaRoot(lua_State* L) noexcept {
		LuaState::stack_guard_t guard(L);
		lua_pushlightuserdata(L, GetBindKey());
		lua_pushnil(L);
		lua_rawset(L, LUA_REGISTRYINDEX);

		lua_pushlightuserdata(L, GetCacheKey());
		lua_pushnil(L);
		lua_rawset(L, LUA_REGISTRYINDEX);

		hostState = nullptr;
	}

	void Warp::ChainWait(Warp* target, Warp* other) {
		// printf("$> Warp %p is waiting for Warp(s): %p and %p\n", this, target, other);
	}

	void Warp::ChainEnter(Warp* from) {
		// printf("$> Warp %p is entered from Warp: %p\n", this, from);
	}

	void Warp::BindLuaCoroutine(void* address) noexcept {
		assert(CurrentLuaThread == nullptr);
		assert(CurrentLuaWarp == nullptr);
		assert(hostState != nullptr);

		lua_State* L = hostState;
		LuaState::stack_guard_t guard(L);
		assert(L != nullptr);
		lua_pushlightuserdata(L, address);
		lua_rawget(L, LUA_REGISTRYINDEX);
		lua_State* T = lua_tothread(L, -1);
		lua_pop(L, 1);

		CurrentLuaThread = T;
		CurrentLuaWarp = this;
	}

	void Warp::UnbindLuaCoroutine() noexcept {
		CurrentLuaThread = nullptr;
		CurrentLuaWarp = nullptr;
	}

	void Warp::Acquire() {
		while (!preempt()) {
			if (!get_async_worker().is_terminated()) {
				get_async_worker().poll_delay(Priority_Highest, 20);
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
			}
		}
	}

	void Warp::Release() {
		yield();
	}

	Warp::SwitchWarp::SwitchWarp(Warp* target_warp, Warp* other_warp) noexcept : Base(target_warp, other_warp), luaState(CurrentLuaThread), luaWarp(CurrentLuaWarp) {
		CurrentLuaThread = nullptr;
		CurrentLuaWarp = nullptr;

		if (Base::source != nullptr) {
			Base::source->ChainWait(Base::target, Base::other);
		}
	}

	bool Warp::SwitchWarp::await_ready() const noexcept {
		return Base::await_ready();
	}

	void Warp::SwitchWarp::await_suspend(std::coroutine_handle<> handle) {
		return Base::await_suspend(handle);
	}

	Warp* Warp::SwitchWarp::await_resume() const noexcept {
		CurrentLuaThread = luaState;
		CurrentLuaWarp = luaWarp;
		Warp* ret = Base::await_resume();

		if (Base::target != nullptr) {
			Base::target->ChainEnter(ret);
		}

		if (Base::other != nullptr) {
			Base::other->ChainEnter(ret);
		}

		return ret;
	}

	Guid Guid::Generate() noexcept {
		Guid guid;
#ifdef _WIN32
		UUID uuid{};
		static_assert(sizeof(Guid) == sizeof(UUID), "Uuid size mismatch!");
		if (::UuidCreate(&uuid) == RPC_S_OK) {
			memcpy(&guid, &uuid, sizeof(uuid));
		}

		return guid;
#else
		uuid_t uuid;
		uuid_generate(uuid);
		static_assert(sizeof(Guid) == sizeof(uuid), "Uuid size mismatch!");
		memcpy(&guid.first, uuid, sizeof(uuid));

		return guid;
#endif
	}
}