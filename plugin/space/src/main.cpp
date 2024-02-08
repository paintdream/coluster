#include "Space.h"
using namespace coluster;

extern "C" SPACE_API int luaopen_space(lua_State * L) {
	return LuaState::forward(L, [](LuaState luaState) {
		return luaState.make_type<Space>("Space", AutoAsyncWorker());
	});
}