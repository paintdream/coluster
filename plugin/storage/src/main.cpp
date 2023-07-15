#include "Storage.h"
#include "../../../ref/iris/src/iris_common.inl"
using namespace coluster;

extern "C" STORAGE_API int luaopen_storage(lua_State * L) {
	LuaState luaState(L);
	auto ref = luaState.make_type<Storage>("Storage", AutoAsyncWorker());
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref.get());
	luaState.deref(std::move(ref));

	return 1;
}