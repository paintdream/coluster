#include "Mapping.h"
using namespace coluster;

extern "C" MAPPING_API int luaopen_mapping(lua_State * L) {
	return LuaState::forward(L, [](LuaState luaState) {
		return luaState.make_type<Mapping>("Mapping", AutoAsyncWorker());
	});
}