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

	Warp* Warp::get_current_warp() noexcept {
		return static_cast<Warp*>(Base::get_current_warp());
	}

	AsyncWorker& Warp::get_async_worker() noexcept {
		return Base::get_async_worker();
	}

	lua_State* Warp::GetCurrentLuaThread() noexcept {
		return CurrentLuaThread;
	}

	void Warp::SetCurrentLuaThread(lua_State* L) noexcept {
		CurrentLuaThread = L;
	}

	Warp::SwitchWarp Warp::Switch(const std::source_location& source, Warp* target, Warp* other) noexcept {
		return SwitchWarp(source, target, other);
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

	void Warp::ChainWait(const std::source_location& source, Warp* from, Warp* target) {
		/*
		if (from != target) {
			printf("$> Warp %p is waiting for Warp: %p\n", from, target);
			printf("$> Source file: %s:%d\n", source.file_name(), source.line());
			lua_State* L = CurrentLuaThread;
			if (L != nullptr) {
				LuaState::stack_guard_t guard(L);
				luaL_traceback(L, L, "$> ", 0);
				printf("%s\n", lua_tostring(L, -1));
				lua_pop(L, 1);
			}
		}*/
	}

	void Warp::ChainEnter(Warp* from, Warp* target) {
		/*
		if (from != target) {
			printf("$> Warp %p is entered from Warp: %p\n", from, target);
		}*/
	}

	void Warp::BindLuaCoroutine(void* address) noexcept {
		assert(CurrentLuaThread == nullptr);
		assert(hostState != nullptr);

		lua_State* L = hostState;
		LuaState::stack_guard_t guard(L);
		assert(L != nullptr);
		lua_pushlightuserdata(L, address);
		lua_rawget(L, LUA_REGISTRYINDEX);
		lua_State* T = lua_tothread(L, -1);
		lua_pop(L, 1);

		CurrentLuaThread = T;
	}

	void Warp::UnbindLuaCoroutine() noexcept {
		CurrentLuaThread = nullptr;
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

	Warp::SwitchWarp::SwitchWarp(const std::source_location& source, Warp* target_warp, Warp* other_warp) noexcept : Base(target_warp, other_warp), luaState(CurrentLuaThread) {
		Warp::ChainWait(source, Base::source, Base::target != nullptr ? Base::target : Base::other);

		if (Base::target != Base::other && Base::target != nullptr && Base::other != nullptr) {
			Warp::ChainWait(source, Base::source, Base::other);
		}

		CurrentLuaThread = nullptr;
	}

	bool Warp::SwitchWarp::await_ready() const noexcept {
		return Base::await_ready();
	}

	void Warp::SwitchWarp::await_suspend(std::coroutine_handle<> handle) {
		return Base::await_suspend(handle);
	}

	Warp* Warp::SwitchWarp::await_resume() const noexcept {
		CurrentLuaThread = luaState;
		Warp* ret = Base::await_resume();
		assert(ret == Base::source);

		Warp::ChainEnter(Base::source, Base::target != nullptr ? Base::target : Base::other);

		if (Base::target != Base::other && Base::target != nullptr && Base::other != nullptr) {
			Warp::ChainEnter(Base::source, Base::other);
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