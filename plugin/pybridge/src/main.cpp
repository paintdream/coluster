#include "PyBridge.h"
#include "../../../ref/iris/src/iris_common.inl"
using namespace coluster;

extern "C" PYBRIDGE_API int luaopen_pybridge(lua_State * L) {
	LuaState luaState(L);
	auto ref = luaState.make_type<PyBridge>("PyBridge", AutoAsyncWorker());
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref.get());
	luaState.deref(std::move(ref));

	return 1;
}