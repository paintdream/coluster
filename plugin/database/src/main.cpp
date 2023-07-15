#include "Database.h"
#include "../../../ref/iris/src/iris_common.inl"
using namespace coluster;

extern "C" DATABASE_API int luaopen_database(lua_State * L) {
	LuaState luaState(L);
	auto ref = luaState.make_type<Database>("Database", AutoAsyncWorker());
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref.get());
	luaState.deref(std::move(ref));

	return 1;
}