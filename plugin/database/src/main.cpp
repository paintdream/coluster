#include "Database.h"
#include "../../../ref/iris/src/iris_common.inl"
using namespace coluster;

extern "C" DATABASE_API int luaopen_database(lua_State * L) {
	return LuaState::forward(L, [](LuaState luaState) {
		return luaState.make_type<Database>("Database", AutoAsyncWorker());
	});
}