#include "Trace.h"
using namespace coluster;

extern "C" TRACE_API int luaopen_tracing(lua_State * L) {
	return LuaState::forward(L, [](LuaState luaState) {
		return luaState.make_type<Trace>("Trace", AutoAsyncWorker());
	});
}