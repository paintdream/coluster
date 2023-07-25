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

	static thread_local void* CurrentCoroutineAddress = nullptr;

	Warp* Warp::get_current_warp() noexcept {
		return static_cast<Warp*>(Base::get_current_warp());
	}

	AsyncWorker& Warp::get_async_worker() noexcept {
		return Base::get_async_worker();
	}

	Warp::SwitchWarp Warp::Switch(const std::source_location& source, Warp* target, Warp* other) noexcept {
		return SwitchWarp(source, target, other);
	}

	void Warp::BindLuaRoot(lua_State* L) noexcept {
		hostState = L;

		// create cache table
		LuaState::stack_guard_t guard(L);
		LuaState lua(L);
		lua.set_registry(GetCacheKey(), lua.make_table([](LuaState lua) {
			lua.define_metatable(lua.make_table([](LuaState lua) {
				lua.define("__mode", "v");
			}));
		}));

		lua.set_registry(GetBindKey(), lua.make_table([](LuaState lua) {
			lua.define("trace", lua.make_table([](LuaState lua) {
				lua.define_metatable(lua.make_table([](LuaState lua) {
					lua.define("__mode", "k");
				}));
			}));
		}));
	}

	void Warp::UnbindLuaRoot(lua_State* L) noexcept {
		LuaState::stack_guard_t guard(L);
		LuaState lua(L);
		lua.set_registry(GetCacheKey(), nullptr);
		lua.set_registry(GetBindKey(), nullptr);

		hostState = nullptr;
	}

	void Warp::ChainWait(const std::source_location& source, Warp* from, Warp* target, Warp* other) {
		if (from != target || from != other) {
			// printf("$> Warp %p is waiting for Warp: %p\n", from, target);
			// printf("$> Source file: %s:%d\n", source.file_name(), source.line());
			Warp* scriptWarp = from != nullptr ? from->get_async_worker().GetScriptWarp() : target != nullptr ? target->get_async_worker().GetScriptWarp() : other->get_async_worker().GetScriptWarp();
			assert(scriptWarp != nullptr);

			// use raw api for faster operation
			scriptWarp->queue_routine([address = Warp::GetCurrentCoroutineAddress(), source, from, target, other]() {
				Warp* scriptWarp = Warp::get_current_warp();
				lua_State* L = scriptWarp->hostState;
				LuaState::stack_guard_t guard(L);
				lua_pushlightuserdata(L, scriptWarp->GetBindKey());
				lua_rawget(L, LUA_REGISTRYINDEX);
				lua_pushliteral(L, "trace");
				lua_rawget(L, -2);
				lua_pushlightuserdata(L, address);
				lua_rawget(L, LUA_REGISTRYINDEX); // get thread

				if (lua_type(L, -1) != LUA_TNIL) {
					lua_pushfstring(L, "<<Wait>> Warp [%p] ==> Warp [%p][%p] at %s <%s:%d>", from, target, other, source.function_name(), source.file_name(), source.line());
					// fprintf(stdout, "%s\n", lua_tostring(L, -1));
					lua_rawset(L, -3);
					lua_pop(L, 2);
				} else {
					lua_pop(L, 3);
				}
			});
		}
	}

	void Warp::ChainEnter(Warp* from, Warp* target, Warp* other) {
		/*
		if (from != target) {
			printf("$> Warp %p is entered from Warp: %p\n", from, target);
		}*/
	}

	void Warp::SetCurrentCoroutineAddress(void* address) noexcept {
		assert(CurrentCoroutineAddress == nullptr || address == nullptr);
		CurrentCoroutineAddress = address;
	}

	void* Warp::GetCurrentCoroutineAddress() noexcept {
		return CurrentCoroutineAddress;
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

	Warp::SwitchWarp::SwitchWarp(const std::source_location& source, Warp* target_warp, Warp* other_warp) noexcept : Base(target_warp, other_warp), coroutineAddress(Warp::GetCurrentCoroutineAddress()) {
		Warp::ChainWait(source, Base::source, Base::target, Base::other);
		Warp::SetCurrentCoroutineAddress(nullptr);
	}

	bool Warp::SwitchWarp::await_ready() const noexcept {
		return Base::await_ready();
	}

	void Warp::SwitchWarp::await_suspend(std::coroutine_handle<> handle) {
		return Base::await_suspend(handle);
	}

	Warp* Warp::SwitchWarp::await_resume() const noexcept {
		Warp::SetCurrentCoroutineAddress(coroutineAddress);
		Warp* ret = Base::await_resume();
		assert(ret == Base::source);

		Warp::ChainEnter(Base::source, Base::target, Base::other);
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