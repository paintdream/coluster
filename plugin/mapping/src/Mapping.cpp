#include "Mapping.h"

namespace coluster {
	Mapping::Mapping(AsyncWorker& asyncWorker) : asyncMap(asyncWorker) {}
	Mapping::~Mapping() noexcept {}

	Mapping::Object::Object() noexcept {}
	Mapping::Object::~Object() noexcept {}

	Coroutine<RefPtr<Mapping::Object>> Mapping::Get(LuaState lua, std::string_view name) {
		co_return RefPtr<Object>();
	}

	void Mapping::lua_initialize(LuaState lua, int index) {}
	void Mapping::lua_finalize(LuaState lua, int index) {}

	void Mapping::lua_registar(LuaState lua) {
		lua.set_current<&Mapping::Get>("Get");
	}
}
