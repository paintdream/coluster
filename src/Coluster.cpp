#include "Coluster.h"
#ifdef _WIN32
#include <windows.h>
#pragma comment(lib, "Rpcrt4.lib")
#else
#include <uuid/uuid.h>
#endif

namespace iris {
	implement_shared_static_instance(coluster::Warp::Base*);
	implement_shared_static_instance(coluster::AsyncWorker::thread_index_t);
	implement_shared_static_instance(coluster::RootAlloator);
}

namespace coluster {
	static constexpr size_t DEFAULT_HOST_MEMORY_BUDGET = 1024 * 1024 * 1024;
	static constexpr size_t DEFAULT_DEVICE_MEMORY_BUDGET = 1024 * 1024 * 1024;

	static thread_local void* CurrentCoroutineAddress = nullptr;
	void SetCurrentCoroutineAddress(void* address) noexcept {
		assert(CurrentCoroutineAddress == nullptr || address == nullptr);
		CurrentCoroutineAddress = address;
	}

	void* GetCurrentCoroutineAddress() noexcept {
		return CurrentCoroutineAddress;
	}

	AsyncWorker::AsyncWorker() : memoryQuota({ DEFAULT_HOST_MEMORY_BUDGET, DEFAULT_DEVICE_MEMORY_BUDGET }), memoryQuotaQueue(*this, memoryQuota) {}
	AsyncWorker::MemoryQuotaQueue& AsyncWorker::GetMemoryQuotaQueue() noexcept {
		return memoryQuotaQueue;
	}

	void AsyncWorker::SetupSharedWarps(size_t count) {
		sharedWarps.resize(count);
		for (size_t i = 0; i < count; i++) {
			sharedWarps[i] = std::make_shared<Warp>(*this);
		}
	}

	void AsyncWorker::Synchronize(LuaState lua, Warp* warp) {
		auto waiter = [] {std::this_thread::sleep_for(std::chrono::milliseconds(50)); };
		if (scriptWarp) {
			assert(Warp::get_current_warp() == scriptWarp.get());
			lua_State* L = lua.get_state();
			while ((warp == nullptr || !warp->join(waiter)) || !scriptWarp->join(waiter) || poll()) {
				scriptWarp->Release();
				poll_delay(static_cast<size_t>(Priority::Count), std::chrono::milliseconds(20));
				scriptWarp->Acquire();
			}
		} else if (warp != nullptr) {
			while (!warp->join(waiter)) {}
		}
	}

	Warp* Warp::get_current_warp() noexcept {
		return static_cast<Warp*>(Base::get_current_warp());
	}

	AsyncWorker& Warp::get_async_worker() noexcept {
		return Base::get_async_worker();
	}

	Warp::SwitchWarp Warp::Switch(const std::source_location& source, Warp* target, Warp* other, bool parallelTarget, bool parallelOther) noexcept {
		return SwitchWarp(source, target, other, parallelTarget, parallelOther);
	}

	lua_State* Warp::GetLuaRoot() const noexcept {
		return rootState;
	}

	void Warp::BindLuaRoot(lua_State* L) noexcept {
		rootState = L;

		// create cache table
		LuaState::stack_guard_t guard(L);
		LuaState lua(L);

		profileTable = lua.make_table([](LuaState lua) {
			lua.set_current("trace", lua.make_table([](LuaState lua) {
				lua.set_current_metatable(lua.make_table([](LuaState lua) {
					lua.set_current("__mode", "k");
				}));
			}));

			lua.set_current("cache", lua.make_table([](LuaState lua) {
				lua.set_current_metatable(lua.make_table([](LuaState lua) {
					lua.set_current("__mode", "v");
				}));
			}));

			lua.set_current("persist", lua.make_table([](LuaState lua) {}));
			lua.set_current("type", lua.make_table([](LuaState lua) {}));
		});
	}

	void Warp::UnbindLuaRoot(lua_State* L) noexcept {
		LuaState::stack_guard_t guard(L);
		LuaState lua(L);
		lua.deref(std::move(profileTable));
		rootState = nullptr;
	}

	void Warp::ChainWait(const std::source_location& source, Warp* from, Warp* target, Warp* other) {
		if (from != target || from != other) {
			// printf("$> Warp %p is waiting for Warp: %p\n", from, target);
			// printf("$> Source file: %s:%d\n", source.file_name(), source.line());
			Warp* scriptWarp = from != nullptr ? from->get_async_worker().GetScriptWarp() : target != nullptr ? target->get_async_worker().GetScriptWarp() : other->get_async_worker().GetScriptWarp();
			assert(scriptWarp != nullptr);

			// use raw api for faster operation
			scriptWarp->queue_routine([address = GetCurrentCoroutineAddress(), source, from, target, other]() {
				Warp* scriptWarp = Warp::get_current_warp();
				lua_State* L = scriptWarp->rootState;
				LuaState::stack_guard_t guard(L);
				LuaState lua(L);

				Ref trace = std::move(*scriptWarp->GetProfileTable().get(lua, "trace"));

				lua_rawgeti(L, LUA_REGISTRYINDEX, trace.get_ref_value());
				lua_pushlightuserdata(L, address);
				lua_rawget(L, LUA_REGISTRYINDEX); // get thread

				if (lua_type(L, -1) != LUA_TNIL) {
					lua_pushfstring(L, "<<Wait>> Warp [%p] ==> Warp [%p][%p] at %s <%s:%d>", from, target, other, source.function_name(), source.file_name(), source.line());
					// fprintf(stdout, "%s\n", lua_tostring(L, -1));
					lua_rawset(L, -3);
					lua_pop(L, 1);
				} else {
					lua_pop(L, 2);
				}

				lua.deref(std::move(trace));
			});
		}
	}

	void Warp::ChainEnter(Warp* from, Warp* target, Warp* other) {
		if (from != target || from != other) {
			Warp* scriptWarp = from != nullptr ? from->get_async_worker().GetScriptWarp() : target != nullptr ? target->get_async_worker().GetScriptWarp() : other->get_async_worker().GetScriptWarp();
			assert(scriptWarp != nullptr);

			// use raw api for faster operation
			scriptWarp->queue_barrier();
			scriptWarp->queue_routine([address = GetCurrentCoroutineAddress(), from, target, other]() {
				Warp* scriptWarp = Warp::get_current_warp();
				lua_State* L = scriptWarp->rootState;
				LuaState::stack_guard_t guard(L);
				LuaState lua(L);

				Ref trace = scriptWarp->GetProfileTable().get(lua, "trace").value();
				lua_rawgeti(L, LUA_REGISTRYINDEX, trace.get_ref_value());
				lua_pushlightuserdata(L, address);
				lua_rawget(L, LUA_REGISTRYINDEX); // get thread

				if (lua_type(L, -1) != LUA_TNIL) {
					lua_pushfstring(L, "<<Running>> Warp [%p] ==> Warp [%p][%p]", from, target, other);
					// fprintf(stdout, "%s\n", lua_tostring(L, -1));
					lua_rawset(L, -3);
					lua_pop(L, 1);
				} else {
					lua_pop(L, 2);
				}

				lua.deref(std::move(trace));
			});
		}
	}

	void Warp::Acquire() {
		while (!preempt()) {
			if (!get_async_worker().is_terminated()) {
				get_async_worker().poll_delay(static_cast<size_t>(Priority::Highest), std::chrono::milliseconds(20));
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
			}
		}
	}

	void Warp::Release() {
		yield();
	}

	Warp::SwitchWarp::SwitchWarp(const std::source_location& source, Warp* target_warp, Warp* other_warp, bool parallelTarget, bool parallelOther) noexcept : Base(target_warp, other_warp, parallelTarget, parallelOther), coroutineAddress(GetCurrentCoroutineAddress()) {
		Warp::ChainWait(source, Base::source, Base::target, Base::other);
		SetCurrentCoroutineAddress(nullptr);
	}

	bool Warp::SwitchWarp::await_ready() const noexcept {
		return Base::await_ready();
	}

	void Warp::SwitchWarp::await_suspend(std::coroutine_handle<> handle) {
		return Base::await_suspend(handle);
	}

	Warp* Warp::SwitchWarp::await_resume() const noexcept {
		SetCurrentCoroutineAddress(coroutineAddress);
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
			std::memcpy(&guid, &uuid, sizeof(uuid));
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
	
	Object::Object() noexcept {}
	Object::~Object() noexcept {}

	Warp* Object::GetObjectWarp() const noexcept {
		return nullptr;
	}
}