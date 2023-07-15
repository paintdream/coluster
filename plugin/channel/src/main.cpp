#include "Channel.h"
#include "../../../ref/iris/src/iris_common.inl"
using namespace coluster;

extern "C" CHANNEL_API int luaopen_channel(lua_State * L) {
	LuaState luaState(L);
	auto ref = luaState.make_type<Channel>("Channel", AutoAsyncWorker());
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref.get());
	luaState.deref(std::move(ref));

	return 1;
}