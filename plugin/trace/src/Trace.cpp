#include "Trace.h"

namespace coluster {
	Trace::Trace(AsyncWorker& asyncWorker) : Warp(asyncWorker) {}
	Trace::~Trace() noexcept {}

	void Trace::lua_initialize(LuaState lua, int index) {}
	void Trace::lua_finalize(LuaState lua, int index) {}
	void Trace::lua_registar(LuaState lua) {}
}
