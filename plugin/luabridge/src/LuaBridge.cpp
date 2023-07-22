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
	LuaBridge::LuaBridge(AsyncWorker& asyncWorker) : Warp(asyncWorker), dataExchangeStack(nullptr) {
		state = luaL_newstate();
		luaL_openlibs(state);
		BindLuaRoot(state);
	}

	LuaBridge::~LuaBridge() noexcept {
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

	Coroutine<RefPtr<LuaBridge::Object>> LuaBridge::Get(Required<RefPtr<LuaBridge>> s, LuaState lua, std::string_view name) {
		LuaBridge* self = s.get();
		Warp* currentWarp = co_await Warp::Switch(&self->GetWarp());
		LuaState target(self->state);
		Ref ref = target.get_global<Ref>(name);
		co_await Warp::Switch(currentWarp);
		co_return lua.make_object<Object>(self->FetchObjectType(lua, currentWarp, std::move(s.get())), *self, std::move(ref));
	}

	Coroutine<RefPtr<LuaBridge::Object>> LuaBridge::Load(Required<RefPtr<LuaBridge>> s, LuaState lua, std::string_view code) {
		LuaBridge* self = s.get();
		Warp* currentWarp = co_await Warp::Switch(&self->GetWarp());
		LuaState target(self->state);
		Ref ref = target.load(code);
		co_await Warp::Switch(currentWarp);
		co_return lua.make_object<Object>(self->FetchObjectType(lua, currentWarp, std::move(s.get())), *self, std::move(ref));
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

		Warp* currentWarp = co_await Warp::Switch(Warp::get_current_warp(), &GetWarp());
		LuaState target(lua_newthread(state));
		Ref threadRef = Ref(luaL_ref(state, LUA_REGISTRYINDEX));

		for (int i = 1; i <= count; i++) {
			dataExchange.native_cross_transfer_variable<true>(target, i);
		}

		lua_pop(D, count);

		// make async call
		co_await Warp::Switch(&GetWarp());
		int ret = target.native_call(callable.get()->GetRef(), count);

		if (ret != 0) {
			// copy return values
			co_await Warp::Switch(currentWarp, &GetWarp());
			for (int i = ret; i > 0; i--) {
				target.native_cross_transfer_variable<true>(dataExchange, -i);
			}
		}

		target.deref(std::move(threadRef));

		co_await Warp::Switch(currentWarp);
		co_return StackIndex { dataExchange, ret };
	}

	Ref LuaBridge::FetchObjectType(LuaState lua, Warp* warp, Ref&& self) {
		assert(warp != nullptr);
		assert(warp == Warp::get_current_warp());
		assert(warp != this);

		void* key = static_cast<void*>(&state);
		Ref r = warp->GetCacheTable<Ref>(key);
		if (r) {
			lua.deref(std::move(self));
			return r;
		}

		Ref objectTypeRef = lua.make_type<Object>("Object", std::ref(*this), Ref());
		objectTypeRef.set(lua, "__host", std::move(self));
		warp->SetCacheTable(key, objectTypeRef);
		return objectTypeRef;
	}

	void LuaBridge::lua_initialize(LuaState lua, int index) {
		lua_State* L = lua.get_state();
		LuaState::stack_guard_t guard(L);
		dataExchangeStack = lua_newthread(L);
		dataExchangeRef = Ref(luaL_ref(L, LUA_REGISTRYINDEX));
	}

	void LuaBridge::lua_finalize(LuaState lua, int index) {
		GetWarp().join();
		lua.deref(std::move(dataExchangeRef));
		dataExchangeStack = nullptr;
	}

	void LuaBridge::lua_registar(LuaState lua) {
		lua.define<&LuaBridge::Load>("Load");
		lua.define<&LuaBridge::Get>("Get");
		lua.define<&LuaBridge::Call>("Call");
	}
}