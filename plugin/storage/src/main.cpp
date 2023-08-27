#include "Storage.h"
using namespace coluster;

extern "C" STORAGE_API int luaopen_storage(lua_State * L) {
	return LuaState::forward(L, [](LuaState luaState) {
		return luaState.make_type<Storage>("Storage", AutoAsyncWorker());
	});
}