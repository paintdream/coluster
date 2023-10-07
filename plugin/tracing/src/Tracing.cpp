#include "Tracing.h"

namespace coluster {
	Tracing::Tracing(AsyncWorker& asyncWorker) : Warp(asyncWorker) {}
	Tracing::~Tracing() noexcept {}

	void Tracing::lua_initialize(LuaState lua, int index) {}
	void Tracing::lua_finalize(LuaState lua, int index) {}
	void Tracing::lua_registar(LuaState lua) {}
}
