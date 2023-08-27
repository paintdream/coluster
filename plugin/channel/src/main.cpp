#include "Channel.h"
using namespace coluster;

extern "C" CHANNEL_API int luaopen_channel(lua_State* L) {
	return LuaState::forward(L, [](LuaState luaState) {
		return luaState.make_type<Channel>("Channel", AutoAsyncWorker());
	});
}