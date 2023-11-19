#include "Graphic.h"
using namespace coluster;

extern "C" GRAPHIC_API int luaopen_graphic(lua_State * L) {
	return LuaState::forward(L, [](LuaState luaState) {
		return luaState.make_type<Graphic>("Graphic", AutoAsyncWorker());
	});
}