#include "PyBridge.h"
#define PY_SSIZE_T_CLEAN
#include <Python.h>

namespace coluster {
	struct PyGILGuard {
		PyGILGuard() {
			state = PyGILState_Ensure();
		}

		~PyGILGuard() noexcept {
			PyGILState_Release(state);
		}

	private:
		PyGILState_STATE state;
	};

	PyBridge::Object::Object(PyBridge& bri, PyObject* obj) noexcept : bridge(bri), object(obj) {}
	PyBridge::Object::~Object() noexcept {
		if (object != nullptr) {
			bridge.QueueDeleteObject(object);
		}
	}

	PyObject* PyBridge::Object::GetPyObject() const noexcept {
		return object == nullptr ? Py_None : object;
	}

	PyBridge::PyBridge(AsyncWorker& asyncWorker) : Warp(asyncWorker) {}
	PyBridge::~PyBridge() noexcept {}

	void PyBridge::QueueDeleteObject(PyObject* object) {
		deletingObjects.push(object);

		if (deletingObjectRoutineState.exchange(queue_state_pending, std::memory_order_release) == queue_state_idle) {
			GetWarp().queue_routine([this]() {
				PyGILGuard guard;
				size_t expected;
				do {
					deletingObjectRoutineState.store(queue_state_executing, std::memory_order_release);
					while (!deletingObjects.empty()) {
						Py_DECREF(deletingObjects.top());
						deletingObjects.pop();
					}

					expected = queue_state_executing;
				} while (!deletingObjectRoutineState.compare_exchange_strong(expected, queue_state_idle, std::memory_order_release));
			});
		}
	}

	void PyBridge::lua_initialize(LuaState lua, int index) {}
	void PyBridge::lua_finalize(LuaState lua, int index) {
		isFinalizing = true;
		get_async_worker().Synchronize(lua, this);
		isFinalizing = false;
	}

	void PyBridge::lua_registar(LuaState lua) {
		lua.set_current<&PyBridge::Call>("Call");
		lua.set_current<&PyBridge::Import>("Import");
		lua.set_current<&PyBridge::Get>("Get");
	}

	Coroutine<RefPtr<PyBridge::Object>> PyBridge::Get(LuaState lua, std::string_view name) {
		Ref s = lua.get_context<Ref>(LuaState::context_this_t());
		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &GetWarp());
		PyObject* object = nullptr;
		do {
			PyGILGuard guard;
			PyObject* globals = PyEval_GetGlobals();
			object = PyDict_GetItemString(globals, name.data());
			Py_DECREF(globals);
		} while (false);

		co_await Warp::Switch(std::source_location::current(), currentWarp);
		if (!isFinalizing) {
			co_return lua.make_object<Object>(FetchObjectType(lua, currentWarp, std::move(s)), *this, object);
		} else {
			lua.deref(std::move(s));
			co_return RefPtr<Object>();
		}
	}

	Coroutine<RefPtr<PyBridge::Object>> PyBridge::Import(LuaState lua, std::string_view name) {
		Ref s = lua.get_context<Ref>(LuaState::context_this_t());
		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &GetWarp());
		PyObject* object = nullptr;
		do {
			PyGILGuard guard;
			object = PyImport_AddModule(name.data());
		} while (false);

		co_await Warp::Switch(std::source_location::current(), currentWarp);
		if (!isFinalizing) {
			co_return lua.make_object<Object>(FetchObjectType(lua, currentWarp, std::move(s)), *this, object);
		} else {
			lua.deref(std::move(s));
			co_return RefPtr<Object>();
		}
	}

	Coroutine<RefPtr<PyBridge::Object>> PyBridge::Call(LuaState lua, Required<Object*> callable, std::vector<Object*>&& params) {
		Ref s = lua.get_context<Ref>(LuaState::context_this_t());
		std::vector<Object*> parameters = std::move(params);
		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &GetWarp());

		PyObject* object = nullptr;
		do {
			PyGILGuard guard;
			// make parameter tuple
			PyObject* tuple = PyTuple_New(parameters.size());
			for (size_t i = 0; i < parameters.size(); i++) {
				PyObject* arg = parameters[i]->GetPyObject();
				Py_INCREF(arg);
				PyTuple_SET_ITEM(tuple, i, arg);
			}

			object = PyObject_CallObject(callable.get()->GetPyObject(), tuple);
			Py_DECREF(tuple);
		} while (false);

		co_await Warp::Switch(std::source_location::current(), currentWarp);
		if (!isFinalizing) {
			co_return lua.make_object<Object>(FetchObjectType(lua, currentWarp, std::move(s)), *this, object);
		} else {
			lua.deref(std::move(s));
			co_return RefPtr<Object>();
		}
	}

	Coroutine<RefPtr<PyBridge::Object>> PyBridge::Pack(LuaState lua, Ref&& ref) {
		Ref s = lua.get_context<Ref>(LuaState::context_this_t());
		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), Warp::get_current_warp(), &GetWarp());

		PyObject* object = nullptr;
		do {
			PyGILGuard guard;
			lua_State* L = lua.get_state();
			LuaState::stack_guard_t stackGuard(L);

			lua_rawgeti(L, LUA_REGISTRYINDEX, ref.get());
			object = PackObject(lua, -1);
			lua_pop(L, 1);
		} while (false);

		co_await Warp::Switch(std::source_location::current(), currentWarp);
		if (!isFinalizing) {
			co_return lua.make_object<Object>(FetchObjectType(lua, currentWarp, std::move(s)), *this, object);
		} else {
			lua.deref(std::move(s));
			co_return RefPtr<Object>();
		}
	}

	Ref PyBridge::FetchObjectType(LuaState lua, Warp* warp, Ref&& self) {
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

		Ref objectTypeRef = lua.make_type<Object>("Object", std::ref(*this), nullptr);
		objectTypeRef.set(lua, "__host", std::move(self));
		cache.set(lua, key, objectTypeRef);
		lua.deref(std::move(cache));

		return objectTypeRef;
	}

	Coroutine<Ref> PyBridge::Unpack(LuaState lua, RefPtr<Object>&& object) {
		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), Warp::get_current_warp(), &GetWarp());
		lua_State* L = lua.get_state();

		do {
			PyGILGuard guard;
			UnpackObject(lua, object->GetPyObject());
		} while (false);

		co_await Warp::Switch(std::source_location::current(), currentWarp);
		co_return Ref(luaL_ref(L, LUA_REGISTRYINDEX));
	}

	PyObject* PyBridge::PackObject(LuaState lua, int index) {
		lua_State* L = lua.get_state();
		LuaState::stack_guard_t guard(L);
		int type = lua_type(L, index);
		switch (type) {
			case LUA_TBOOLEAN:
			{
				return PyBool_FromLong(lua.native_get_variable<bool>(index));
			}
			case LUA_TLIGHTUSERDATA:
			{
				Py_INCREF(Py_None);
				return Py_None;
			}
			case LUA_TNUMBER:
			{
#if LUA_VERSION_NUM <= 502
				return PyFloat_FromDouble(lua.native_get_variable<lua_Number>(index));
#else
				if (lua_isinteger(L, index)) {
					return PyLong_FromLongLong(lua.native_get_variable<lua_Integer>(index));
				} else {
					return PyFloat_FromDouble(lua.native_get_variable<lua_Number>(index));
				}
#endif
			}
			case LUA_TSTRING:
			{
				std::string_view view = lua.native_get_variable<std::string_view>(index);
#if PY_MAJOR_VERSION < 3
				return PyString_FromStringAndSize(view.data(), view.size());
#else
				return PyUnicode_FromStringAndSize(view.data(), view.size());
#endif
			}
			case LUA_TTABLE:
			{
				int absindex = lua_absindex(L, index);
				size_t len = static_cast<size_t>(lua_rawlen(L, absindex));
				if (len != 0) {
					// Convert to list 
					PyObject* listObject = PyList_New(len);

					for (size_t i = 0; i < len; i++) {
						lua_rawgeti(L, absindex, static_cast<int>(i));
						PyList_SET_ITEM(listObject, i, PackObject(lua, -1));

						lua_pop(L, 1);
					}

					return listObject;
				} else {
					PyObject* dictObject = PyDict_New();
					lua_pushnil(L);

					while (lua_next(L, absindex) != 0) {
						PyObject* key = PackObject(lua, -2);
						PyObject* value = PackObject(lua, -1);
						PyDict_SetItem(dictObject, key, value);
						Py_DECREF(key);
						Py_DECREF(value);

						lua_pop(L, 1);
					}

					return dictObject;
				}
			}
			case LUA_TFUNCTION:
			case LUA_TUSERDATA:
			case LUA_TTHREAD:
			{
				Py_INCREF(Py_None);
				return Py_None;
			}
			default:
			{
				Py_INCREF(Py_None);
				return Py_None;
			}
		}
	}

	void PyBridge::UnpackObject(LuaState lua, PyObject* object) {
		lua_State* L = lua.get_state();
		LuaState::stack_guard_t guard(L, 1);
		PyTypeObject* type = object->ob_type;

		if (object == Py_None) {
			lua.native_push_variable(nullptr);
			if (type == &PyBool_Type) {
				lua.native_push_variable(object != Py_False);
			} else if (type == &PyLong_Type) {
				lua.native_push_variable(PyLong_AsLongLong(object));
			} else if (type == &PyFloat_Type) {
				lua.native_push_variable(PyFloat_AsDouble(object));
#if PY_MAJOR_VERSION < 3 
			} else if (type == &PyString_Type) {
				Py_ssize_t size = 0;
				char* s = nullptr;
				if (PyString_AsStringAndSize(object, &s, &size)) {
					lua.native_push_variable(std::string_view(s, size));
				} else {
					lua.native_push_variable(nullptr);
				}
#else
			} else if (type == &PyUnicode_Type) {
				Py_ssize_t size = 0;
				const char* s = PyUnicode_AsUTF8AndSize(object, &size);
				lua.native_push_variable(std::string_view(s, size));
			} else if (type == &PyByteArray_Type) {
				Py_ssize_t size = PyByteArray_Size(object);
				const char* s = PyByteArray_AsString(object);
				lua.native_push_variable(std::string_view(s, size));
#endif
			} else if (type == &PyTuple_Type) {
				Py_ssize_t size = PyTuple_GET_SIZE(object);
				lua_createtable(L, static_cast<int>(size), 0);

				for (Py_ssize_t i = 0; i < size; i++) {
					UnpackObject(lua, PyTuple_GetItem(object, i));
					lua_rawseti(L, -2, static_cast<int>(i));
				}
			} else if (type == &PyList_Type) {
				Py_ssize_t size = PyList_GET_SIZE(object);
				lua_createtable(L, static_cast<int>(size), 0);

				for (Py_ssize_t i = 0; i < size; i++) {
					UnpackObject(lua, PyList_GetItem(object, i));
					lua_rawseti(L, -2, static_cast<int>(i));
				}
			} else if (type == &PyDict_Type) {
				PyObject* key, * value;
				Py_ssize_t pos = 0;

				while (PyDict_Next(object, &pos, &key, &value)) {
					UnpackObject(lua, key);
					UnpackObject(lua, value);
					lua_rawset(L, -3);
				}
			} else {
				lua.native_push_variable(nullptr);
			}
		}
	}
}

