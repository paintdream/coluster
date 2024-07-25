// LuaBridge.h
// PaintDream (paintdream@paintdream.com)
// 2023-07-02
//

#pragma once

#include "LuaBridgeCommon.h"

namespace coluster {
	class LuaBridge : public Object, protected Warp {
	public:
		using PoolBase = Pool<LuaBridge, lua_State*, 256>;
		LuaBridge(AsyncWorker& asyncWorker);
		~LuaBridge() noexcept override;

		enum class Status : uint8_t {
			Invalid,
			Ready,
			Pending
		};

		Warp& GetWarp() noexcept { return *this; }

		class Object {
		public:
			Object(LuaBridge& bridge, Ref&& r) noexcept;
			~Object() noexcept;

			Ref& GetRef() noexcept { return ref; }

		protected:
			LuaBridge& bridge;
			Ref ref;
		};

		Coroutine<Result<RefPtr<Object>>> Get(LuaState lua, std::string_view name);
		Coroutine<Result<RefPtr<Object>>> Load(LuaState lua, std::string_view code, std::string_view name);
		Coroutine<Result<StackIndex>> Call(LuaState lua, Required<Object*> callable, StackIndex stackIndex);
		void lua_initialize(LuaState lua, int index);
		void lua_finalize(LuaState lua, int index);
		static void lua_registar(LuaState lua);

	protected:
		void QueueDeleteObject(Ref&& object);
		Ref FetchObjectType(LuaState lua, Warp* warp, Ref&& self);

	protected:
		std::atomic<queue_state_t> deletingObjectRoutineState = queue_state_t::idle;
		QueueList<Ref> deletingObjects;

	protected:
		lua_State* state = nullptr;
		lua_State* dataExchangeStack = nullptr;
		Ref dataExchangeRef;
		Status status = Status::Invalid;
	};
}

