#include "LuaBridge.h"

namespace coluster {
	LuaBridge::LuaBridge(AsyncWorker& asyncWorker) : Warp(asyncWorker) {
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

		if (deletingObjectRoutineState.exchange(queue_state_t::pending, std::memory_order_release) == queue_state_t::idle) {
			GetWarp().queue_routine_post([this]() {
				LuaState target(state);
				queue_state_t expected;
				do {
					deletingObjectRoutineState.store(queue_state_t::executing, std::memory_order_release);
					while (!deletingObjects.empty()) {
						target.deref(std::move(deletingObjects.top()));
						deletingObjects.pop();
					}

					expected = queue_state_t::executing;
				} while (!deletingObjectRoutineState.compare_exchange_strong(expected, queue_state_t::idle, std::memory_order_release));
			});
		}
	}

	Coroutine<Result<RefPtr<LuaBridge::Object>>> LuaBridge::Get(LuaState lua, std::string_view name) {
		if (status != Status::Ready) {
			co_return ResultError("LuaBridge not ready");
		}

		status = Status::Pending;
		Ref s = lua.get_context<Ref>(LuaState::context_this_t());
		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &GetWarp());
		LuaState target(state);
		Ref ref = target.get_global<Ref>(name);
		co_await Warp::Switch(std::source_location::current(), currentWarp);

		if (status != Status::Invalid) {
			status = Status::Ready;
			co_return lua.make_object<Object>(FetchObjectType(lua, currentWarp, std::move(s)), *this, std::move(ref));
		} else {
			co_return ResultError("LuaBridge invalid");
		}
	}

	Coroutine<Result<RefPtr<LuaBridge::Object>>> LuaBridge::Load(LuaState lua, std::string_view code, std::string_view name) {
		if (status != Status::Ready) {
			co_return ResultError("LuaBridge not ready");
		}

		status = Status::Pending;
		Ref s = lua.get_context<Ref>(LuaState::context_this_t());
		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &GetWarp());
		LuaState target(state);

		auto ret = target.load(code, name);
		co_await Warp::Switch(std::source_location::current(), currentWarp);

		if (!ret) {
			status = Status::Ready;
			lua.deref(std::move(s));
			co_return ResultError("LuaBridge::Load() -> " + ret.message);
		}

		if (status != Status::Invalid) {
			status = Status::Ready;
			co_return lua.make_object<Object>(FetchObjectType(lua, currentWarp, std::move(s)), *this, std::move(ret.value()));
		} else {
			co_return ResultError("LuaBridge invalid");
		}
	}

	Coroutine<Result<StackIndex>> LuaBridge::Call(LuaState lua, Required<Object*> callable, StackIndex parameters) {
		if (status != Status::Ready) {
			co_return ResultError("LuaBridge not ready");
		}

		status = Status::Pending;
		// copy parameters
		lua_State* D = dataExchangeStack;
		lua_State* L = lua.get_state();
		int org = lua_gettop(L);
		int count = org - parameters.index + 1;
		lua_xmove(L, D, count);
		lua_settop(L, org);
		LuaState dataExchange(D);

		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), Warp::get_current_warp(), &GetWarp());
		lua_State* T = state;
		LuaState target(T);

		for (int i = 1; i <= count; i++) {
			dataExchange.native_cross_transfer_variable<true>(target, i);
		}

		lua_pop(D, count);

		// make async call
		co_await Warp::Switch(std::source_location::current(), &GetWarp());
		auto ret = target.native_call(callable.get()->GetRef(), count);

		if (ret) {
			if (ret.value() != 0) {
				// copy return values
				co_await Warp::Switch(std::source_location::current(), currentWarp, &GetWarp());

				for (int i = ret.value(); i > 0; i--) {
					target.native_cross_transfer_variable<true>(dataExchange, -i);
				}
			}

			co_await Warp::Switch(std::source_location::current(), currentWarp);

			lua_settop(T, 0);
			if (status != Status::Invalid) {
				status = Status::Ready;
				co_return StackIndex { dataExchange, ret.value() };
			} else {
				co_return ResultError("LuaBridge invalid");
			}
		} else {
			co_await Warp::Switch(std::source_location::current(), currentWarp);
			lua_settop(T, 0);

			co_return ResultError("LuaBridge::Call() -> " + ret.message);
		}
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

		status = Status::Ready;
	}

	void LuaBridge::lua_finalize(LuaState lua, int index) {
		status = Status::Invalid;

		dataExchangeStack = nullptr;
		lua.deref(std::move(dataExchangeRef));
		get_async_worker().Synchronize(lua, this);
	}

	void LuaBridge::lua_registar(LuaState lua) {
		lua.set_current<&LuaBridge::Load>("Load");
		lua.set_current<&LuaBridge::Get>("Get");
		lua.set_current<&LuaBridge::Call>("Call");
	}
}