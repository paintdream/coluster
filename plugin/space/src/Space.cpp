#include "Space.h"

namespace coluster {
	Space::Space(AsyncWorker& worker) noexcept : asyncWorker(worker) {}
	Space::~Space() noexcept {}

	void Space::lua_initialize(LuaState lua, int index) {}

	void Space::lua_finalize(LuaState lua, int index) {}
	void Space::lua_registar(LuaState lua) {
		lua.set_current<&Space::NewEntity>("NewEntity");
		lua.set_current<&Space::DeleteEntity>("DeleteEntity");
		lua.set_current<&Space::TypeNodeSystem>("TypeNodeSystem");
	}
	
	Entity Space::NewEntity() {
		return entityAllocator.allocate();
	}

	void Space::DeleteEntity(Entity entity) {
		theSystems.remove(entity);
		entityAllocator.free(entity);
	}
}

// implement for sub types
#include "Node.h"

namespace coluster {
	Ref Space::TypeNodeSystem(LuaState lua) {
		Ref type = lua.make_type<NodeSystem>("NodeSystem", std::ref(*this));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this_t()));
		return type;
	}
}

