#include "Space.h"

namespace coluster {
	Space::Space(AsyncWorker& asyncWorker) {}
	Space::~Space() noexcept {}

	void Space::lua_initialize(LuaState lua, int index) {}
	void Space::lua_finalize(LuaState lua, int index) {}
	void Space::lua_registar(LuaState lua) {}
}
