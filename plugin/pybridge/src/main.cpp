#include "PyBridge.h"
using namespace coluster;

extern "C" PYBRIDGE_API int luaopen_pybridge(lua_State * L) {
	return LuaState::forward(L, [](LuaState luaState) {
		return luaState.make_type<PyBridge>("PyBridge", AutoAsyncWorker());
	});
}
