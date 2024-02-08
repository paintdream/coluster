// LuaBridge.h
// PaintDream (paintdream@paintdream.com)
// 2023-07-02
//

#pragma once

#include "../../../src/Coluster.h"

#if !COLUSTER_MONOLITHIC
#ifdef LUABRIDGE_EXPORT
	#ifdef __GNUC__
		#define LUABRIDGE_API __attribute__ ((visibility ("default")))
	#else
		#define LUABRIDGE_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define LUABRIDGE_API __attribute__ ((visibility ("default")))
	#else
		#define LUABRIDGE_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif
#else
#define LUABRIDGE_API
#endif

namespace coluster {
	class LuaBridge : public Object, protected Warp, public Pool<LuaBridge, lua_State*, 256> {
	public:
		using PoolBase = Pool<LuaBridge, lua_State*, 256>;
		LuaBridge(AsyncWorker& asyncWorker);
		~LuaBridge() noexcept override;

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

		struct StackIndex {
			lua_State* dataStack = nullptr;
			int index = 0;
		};

		Coroutine<RefPtr<Object>> Get(LuaState lua, std::string_view name);
		Coroutine<RefPtr<Object>> Load(LuaState lua, std::string_view code);
		Coroutine<StackIndex> Call(LuaState lua, Required<Object*> callable, StackIndex stackIndex);
		void lua_initialize(LuaState lua, int index);
		void lua_finalize(LuaState lua, int index);
		static void lua_registar(LuaState lua);

		template <typename element_t>
		[[nodiscard]] element_t acquire_element();

		template <typename element_t>
		void release_element(element_t element);

	protected:
		void QueueDeleteObject(Ref&& object);
		Ref FetchObjectType(LuaState lua, Warp* warp, Ref&& self);

	protected:
		std::atomic<size_t> deletingObjectRoutineState = queue_state_idle;
		QueueList<Ref> deletingObjects;

	protected:
		lua_State* state = nullptr;
		lua_State* dataExchangeStack = nullptr;
		Ref dataExchangeRef;
	};
}

