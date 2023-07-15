#include "Pipeline.h"
#include "Device.h"
#include "../../../ref/iris/src/iris_common.inl"
using namespace coluster;

extern "C" DEVICE_API int luaopen_device(lua_State * L) {
	LuaState luaState(L);
	auto ref = luaState.make_type<Device>("Device", AutoAsyncWorker());
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref.get());
	luaState.deref(std::move(ref));

	return 1;
}