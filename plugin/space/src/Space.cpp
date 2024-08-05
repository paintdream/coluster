#include "Space.h"

namespace coluster {
	Space::Space(AsyncWorker& worker) noexcept : asyncWorker(worker) {}
	Space::~Space() noexcept {}

	void Space::lua_initialize(LuaState lua, int index) {}

	void Space::lua_finalize(LuaState lua, int index) {}
	void Space::lua_registar(LuaState lua) {
		lua.set_current<&Space::Create>("CreateEntity");
		lua.set_current<&Space::Delete>("DeleteEntity");
		lua.set_current<&Space::Clear>("ClearEntities");
		lua.set_current<&Space::TypeNodeComponentSystem>("TypeNodeComponentSystem");
		lua.set_current<&Space::TypeTransformComponentSystem>("TypeTransformComponentSystem");
		lua.set_current<&Space::TypeScriptComponentSystem>("TypeScriptComponentSystem");
		lua.set_current<&Space::TypeLinkComponentSystem>("TypeScriptComponentSystem");
	}
	
	Entity Space::Create() {
		return entityAllocator.allocate();
	}

	void Space::Delete(Entity entity) {
		theSystems.remove(entity);
		entityAllocator.free(entity);
	}

	void Space::Clear() {
		theSystems.clear();
		entityAllocator.reset();
	}
}

// implement for sub types
#include "NodeComponent.h"
#include "TransformComponent.h"
#include "ScriptComponent.h"
#include "LinkComponent.h"

namespace coluster {
	Ref Space::TypeNodeComponentSystem(LuaState lua) {
		Ref type = lua.make_type<NodeComponentSystem>("NodeComponentSystem", std::ref(*this));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this_t()));
		return type;
	}

	Ref Space::TypeTransformComponentSystem(LuaState lua) {
		Ref type = lua.make_type<TransformComponentSystem>("TransformComponentSystem", std::ref(*this));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this_t()));
		return type;
	}

	Ref Space::TypeScriptComponentSystem(LuaState lua) {
		Ref type = lua.make_type<ScriptComponentSystem>("ScriptComponentSystem", std::ref(*this));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this_t()));
		return type;
	}

	Ref Space::TypeLinkComponentSystem(LuaState lua) {
		Ref type = lua.make_type<LinkComponentSystem>("LinkComponentSystem", std::ref(*this));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this_t()));
		return type;
	}
}

