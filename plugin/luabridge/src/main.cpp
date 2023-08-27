#include "LuaBridge.h"
using namespace coluster;

extern "C" LUABRIDGE_API int luaopen_luabridge(lua_State * L) {
	return LuaState::forward(L, [](LuaState luaState) {
		return luaState.make_type<LuaBridge>("LuaBridge", AutoAsyncWorker());
	});
}