#include "Graphic.h"

namespace coluster {
	Graphic::Graphic(AsyncWorker& worker) : asyncWorker(worker) {}
	Graphic::~Graphic() noexcept {}

	void Graphic::lua_initialize(LuaState lua, int index) {}
	void Graphic::lua_finalize(LuaState lua, int index) {}

	void Graphic::lua_registar(LuaState lua) {}
}
