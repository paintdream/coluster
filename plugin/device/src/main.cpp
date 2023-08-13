#include "Pipeline.h"
#include "Device.h"
#include "../../../ref/iris/src/iris_common.inl"
using namespace coluster;

extern "C" DEVICE_API int luaopen_device(lua_State * L) {
	return LuaState::forward(L, [](LuaState luaState) {
		return luaState.make_type<Device>("Device", AutoAsyncWorker());
	});
}