#include "Util.h"
using namespace coluster;

extern "C" UTIL_API int luaopen_util(lua_State * L) {
	return LuaState::forward(L, [](LuaState luaState) {
		return luaState.make_type<Util>("Util", AutoAsyncWorker());
	});
}