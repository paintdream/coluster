// PyBridge.h
// PaintDream (paintdream@paintdream.com)
// 2023-05-08
//

#pragma once

#include "PyBridgeCommon.h"

typedef struct _object PyObject;
typedef struct _ts PyThreadState;

namespace coluster {
	class PyBridge : public Object, protected Warp {
	public:
		PyBridge(AsyncWorker& asyncWorker);
		~PyBridge() noexcept;

		enum class Status : uint8_t {
			Invalid,
			Ready,
			Pending
		};

		Warp& GetWarp() noexcept { return *this; }

		class Object {
		public:
			Object(PyBridge& bridge, PyObject* object) noexcept;
			~Object() noexcept;

			Object(const Object& rhs) noexcept = delete;
			Object(Object&& rhs) noexcept : bridge(rhs.bridge), object(rhs.object) {
				rhs.object = nullptr;
			}

			Object& operator = (const Object& rhs) = delete;
			Object& operator = (Object&& rhs) noexcept {
				assert(&bridge == &rhs.bridge);
				rhs.object = std::exchange(object, rhs.object);
				return *this;
			}

			PyObject* GetPyObject() const noexcept;

		protected:
			PyBridge& bridge;
			PyObject* object;
		};

		struct StackIndex {
			int index = 0;
		};

		Coroutine<RefPtr<Object>> Get(LuaState lua, std::string_view name);
		Coroutine<RefPtr<Object>> Call(LuaState lua, Required<Object*> callable, StackIndex parameters);
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
		std::atomic<queue_state_t> deletingObjectRoutineState = queue_state_t::idle;
		QueueList<PyObject*> deletingObjects;
		Ref dataExchangeRef;
		lua_State* dataExchangeStack = nullptr;
		Status status = Status::Invalid;
		PyThreadState* threadState = nullptr;
	};
}

