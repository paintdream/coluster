#include "Tracing.h"
using namespace coluster;

extern "C" TRACING_API int luaopen_tracing(lua_State * L) {
	return LuaState::forward(L, [](LuaState luaState) {
		return luaState.make_type<Tracing>("Tracing", AutoAsyncWorker());
	});
}