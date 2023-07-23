// PyBridge.h
// PaintDream (paintdream@paintdream.com)
// 2023-05-08
//

#pragma once

#include "../../../src/Coluster.h"

#ifdef PYBRIDGE_EXPORT
	#ifdef __GNUC__
		#define PYBRIDGE_API __attribute__ ((visibility ("default")))
	#else
		#define PYBRIDGE_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define PYBRIDGE_API __attribute__ ((visibility ("default")))
	#else
		#define PYBRIDGE_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif

typedef struct _object PyObject;
namespace coluster {
	class PyBridge : protected Warp, public EnableReadWriteFence {
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

		static Coroutine<RefPtr<Object>> Get(Required<RefPtr<PyBridge>> self, LuaState lua, std::string_view name);
		static Coroutine<RefPtr<Object>> Call(Required<RefPtr<PyBridge>> self, LuaState lua, Required<Object*> callable, std::vector<Object*>&& parameters);
		static Coroutine<RefPtr<Object>> Import(Required<RefPtr<PyBridge>> self, LuaState lua, std::string_view name);
		static Coroutine<RefPtr<Object>> Pack(Required<RefPtr<PyBridge>> self, LuaState lua, Ref&& ref);
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

