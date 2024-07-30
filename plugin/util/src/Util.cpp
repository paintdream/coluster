#include "Util.h"

namespace coluster {
	Util::Util(AsyncWorker& worker) noexcept : asyncWorker(worker) {}
	Util::~Util() noexcept {}

	void Util::lua_initialize(LuaState lua, int index) {}

	void Util::lua_finalize(LuaState lua, int index) {}
	void Util::lua_registar(LuaState lua) {
		lua.set_current<&Util::TypeDataPipe>("TypeDataPipe");
		lua.set_current<&Util::TypeDataBuffer>("TypeDataBuffer");
		lua.set_current<&Util::TypeObjectDict>("TypeObjectDict");
	}
}

// implement for sub types
#include "DataPipe.h"
#include "DataBuffer.h"
#include "ObjectDict.h"

namespace coluster {
	Ref Util::TypeDataPipe(LuaState lua) {
		Ref type = lua.make_type<DataPipe>("DataPipe", std::ref(asyncWorker));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this_t()));
		return type;
	}

	Ref Util::TypeDataBuffer(LuaState lua) {
		Ref type = lua.make_type<DataBuffer>("DataBuffer", std::ref(asyncWorker));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this_t()));
		return type;
	}

	Ref Util::TypeObjectDict(LuaState lua) {
		Ref type = lua.make_type<ObjectDict>("ObjectDict", std::ref(asyncWorker));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this_t()));
		return type;
	}
}

