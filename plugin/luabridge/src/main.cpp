#include "LuaBridge.h"
#include "../../../ref/iris/src/iris_common.inl"
using namespace coluster;

extern "C" LUABRIDGE_API int luaopen_luabridge(lua_State * L) {
	LuaState luaState(L);
	auto ref = luaState.make_type<LuaBridge>("LuaBridge", AutoAsyncWorker());
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref.get());
	luaState.deref(std::move(ref));

	return 1;
}