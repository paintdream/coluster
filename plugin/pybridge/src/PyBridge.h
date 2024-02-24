// PyBridge.h
// PaintDream (paintdream@paintdream.com)
// 2023-05-08
//

#pragma once

#include "PyBridgeCommon.h"

typedef struct _object PyObject;
namespace coluster {
	class PyBridge : public Object, protected Warp {
	public:
		PyBridge(AsyncWorker& asyncWorker);
		~PyBridge() noexcept;

		Warp& GetWarp() noexcept { return *this; }

		class Object {
		public:
			Object(PyBridge& bridge, PyObject* object) noexcept;
			~Object() noexcept;

			PyObject* GetPyObject() const noexcept;

		protected:
			PyBridge& bridge;
			PyObject* object;
		};

		Coroutine<RefPtr<Object>> Get(LuaState lua, std::string_view name);
		Coroutine<RefPtr<Object>> Call(LuaState lua, Required<Object*> callable, std::vector<Object*>&& parameters);
		Coroutine<RefPtr<Object>> Import(LuaState lua, std::string_view name);
		Coroutine<RefPtr<Object>> Pack(LuaState lua, Ref&& ref);
		Coroutine<Ref> Unpack(LuaState lua, RefPtr<Object>&& object);
		void lua_initialize(LuaState lua, int index);
		void lua_finalize(LuaState lua, int index);
		static void lua_registar(LuaState lua);

	protected:
		void QueueDeleteObject(PyObject* object);
		PyObject* PackObject(LuaState lua, int index);
		void UnpackObject(LuaState lua, PyObject* object);
		Ref FetchObjectType(LuaState lua, Warp* warp, Ref&& self);

	protected:
		std::atomic<size_t> deletingObjectRoutineState = queue_state_idle;
		QueueList<PyObject*> deletingObjects;
		bool isFinalizing = false;
	};
}

