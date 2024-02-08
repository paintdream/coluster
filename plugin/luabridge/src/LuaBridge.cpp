#include "LuaBridge.h"

namespace iris {
	template <>
	struct iris_lua_convert_t<coluster::LuaBridge::StackIndex> {
		static constexpr bool value = true;
		static coluster::LuaBridge::StackIndex from_lua(lua_State* L, int index) {
			return coluster::LuaBridge::StackIndex { L, index };
		}

		static int to_lua(lua_State* L, coluster::LuaBridge::StackIndex&& stackIndex) {
			if (stackIndex.index != 0) {
				lua_xmove(stackIndex.dataStack, L, stackIndex.index);
				assert(lua_gettop(stackIndex.dataStack) == 0);
			}

			return stackIndex.index;
		}
	};
}

namespace coluster {
	LuaBridge::LuaBridge(AsyncWorker& asyncWorker) : Warp(asyncWorker) {
		state = luaL_newstate();
		luaL_openlibs(state);
		BindLuaRoot(state);
	}

	LuaBridge::~LuaBridge() noexcept {
		PoolBase::clear();
		assert(dataExchangeStack == nullptr);
		UnbindLuaRoot(state);
		lua_close(state);
	}

	LuaBridge::Object::Object(LuaBridge& br, Ref&& r) noexcept : bridge(br), ref(std::move(r)) {}
	LuaBridge::Object::~Object() noexcept {
		if (ref) {
			bridge.QueueDeleteObject(std::move(ref));
		}
	}

	void LuaBridge::QueueDeleteObject(Ref&& ref) {
		deletingObjects.push(std::move(ref));

		if (deletingObjectRoutineState.exchange(queue_state_pending, std::memory_order_release) == queue_state_idle) {
			GetWarp().queue_routine_post([this]() {
				LuaState target(state);
				size_t expected;
				do {
					deletingObjectRoutineState.store(queue_state_executing, std::memory_order_release);
					while (!deletingObjects.empty()) {
						target.deref(std::move(deletingObjects.top()));
						deletingObjects.pop();
					}

					expected = queue_state_executing;
				} while (!deletingObjectRoutineState.compare_exchange_strong(expected, queue_state_idle, std::memory_order_release));
			});
		}
	}

	Coroutine<RefPtr<LuaBridge::Object>> LuaBridge::Get(LuaState lua, std::string_view name) {
		Ref s = lua.get_context<Ref>(LuaState::context_this_t());
		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &GetWarp());
		LuaState target(state);
		Ref ref = target.get_global<Ref>(name);
		co_await Warp::Switch(std::source_location::current(), currentWarp);

		if (dataExchangeStack != nullptr) {
			co_return lua.make_object<Object>(FetchObjectType(lua, currentWarp, std::move(s)), *this, std::move(ref));
		} else {
			lua.deref(std::move(s));
			lua.deref(std::move(ref));
			co_return RefPtr<Object>();
		}
	}

	Coroutine<RefPtr<LuaBridge::Object>> LuaBridge::Load(LuaState lua, std::string_view code) {
		Ref s = lua.get_context<Ref>(LuaState::context_this_t());
		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &GetWarp());
		LuaState target(state);
		Ref ref = target.load(code);
		co_await Warp::Switch(std::source_location::current(), currentWarp);

		if (dataExchangeStack != nullptr) {
			co_return lua.make_object<Object>(FetchObjectType(lua, currentWarp, std::move(s)), *this, std::move(ref));
		} else {
			lua.deref(std::move(s));
			lua.deref(std::move(ref));
			co_return RefPtr<Object>();
		}
	}

	Coroutine<LuaBridge::StackIndex> LuaBridge::Call(LuaState lua, Required<Object*> callable, StackIndex parameters) {
		// copy parameters
		lua_State* D = dataExchangeStack;
		lua_State* L = lua.get_state();
		int org = lua_gettop(L);
		int count = org - parameters.index + 1;
		lua_xmove(L, D, count);
		lua_settop(L, org);
		LuaState dataExchange(D);

		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), Warp::get_current_warp(), &GetWarp());
		lua_State* T = acquire();
		LuaState target(T);

		for (int i = 1; i <= count; i++) {
			dataExchange.native_cross_transfer_variable<true>(target, i);
		}

		lua_pop(D, count);

		// make async call
		co_await Warp::Switch(std::source_location::current(), &GetWarp());
		int ret = target.native_call(callable.get()->GetRef(), count);

		if (ret != 0) {
			// copy return values
			co_await Warp::Switch(std::source_location::current(), currentWarp, &GetWarp());
			for (int i = ret; i > 0; i--) {
				target.native_cross_transfer_variable<true>(dataExchange, -i);
			}
		}

		release(std::move(T));

		co_await Warp::Switch(std::source_location::current(), currentWarp);
		co_return StackIndex { dataExchange, ret };
	}

	Ref LuaBridge::FetchObjectType(LuaState lua, Warp* warp, Ref&& self) {
		assert(warp != nullptr);
		assert(warp == Warp::get_current_warp());
		assert(warp != this);

		void* key = static_cast<void*>(this);
		Ref cache = std::move(*warp->GetProfileTable().get(lua, "cache"));
		auto r = cache.get(lua, key);
		if (r) {
			lua.deref(std::move(self));
			lua.deref(std::move(cache));
			return std::move(*r);
		}

		Ref objectTypeRef = lua.make_type<Object>("Object", std::ref(*this), Ref());
		objectTypeRef.set(lua, "__host", std::move(self));
		cache.set(lua, key, objectTypeRef);
		lua.deref(std::move(cache));

		return objectTypeRef;
	}

	void LuaBridge::lua_initialize(LuaState lua, int index) {
		lua_State* L = lua.get_state();
		LuaState::stack_guard_t guard(L);
		dataExchangeRef = lua.make_thread([&](LuaState lua) {
			dataExchangeStack = lua.get_state();
		});
	}

	void LuaBridge::lua_finalize(LuaState lua, int index) {
		dataExchangeStack = nullptr;
		get_async_worker().Synchronize(lua, this);
		lua.deref(std::move(dataExchangeRef));
	}

	void LuaBridge::lua_registar(LuaState lua) {
		lua.set_current<&LuaBridge::Load>("Load");
		lua.set_current<&LuaBridge::Get>("Get");
		lua.set_current<&LuaBridge::Call>("Call");
	}

	template <>
	lua_State* LuaBridge::acquire_element<lua_State*>() {
		lua_State* L = lua_newthread(state);
		lua_pushboolean(state, 1);
		lua_rawset(state, LUA_REGISTRYINDEX);

		return L;
	}

	template <>
	void LuaBridge::release_element(lua_State* element) {
		lua_pushthread(element);
		lua_pushnil(element);
		lua_rawset(element, LUA_REGISTRYINDEX);
	}
}