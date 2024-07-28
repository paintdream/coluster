#include "Space.h"

namespace coluster {
	Space::Space(AsyncWorker& worker) noexcept : asyncWorker(worker) {}
	Space::~Space() noexcept {}

	void Space::lua_initialize(LuaState lua, int index) {}

	void Space::lua_finalize(LuaState lua, int index) {}
	void Space::lua_registar(LuaState lua) {
		lua.set_current<&Space::CreateEntity>("CreateEntity");
		lua.set_current<&Space::DeleteEntity>("DeleteEntity");
		lua.set_current<&Space::ClearEntities>("ClearEntities");
		lua.set_current<&Space::TypeNodeComponentSystem>("TypeNodeComponentSystem");
		lua.set_current<&Space::TypeTransformComponentSystem>("TypeTransformComponentSystem");
		lua.set_current<&Space::TypeScriptComponentSystem>("TypeScriptComponentSystem");
		lua.set_current<&Space::TypeDataPipe>("TypeDataPipe");
		lua.set_current<&Space::TypeObjectDict>("TypeObjectDict");
	}
	
	Entity Space::CreateEntity() {
		return entityAllocator.allocate();
	}

	void Space::DeleteEntity(Entity entity) {
		theSystems.remove(entity);
		entityAllocator.free(entity);
	}

	void Space::ClearEntities() {
		theSystems.clear();
		entityAllocator.reset();
	}
}

// implement for sub types
#include "NodeComponent.h"
#include "TransformComponent.h"
#include "ScriptComponent.h"
#include "DataPipe.h"
#include "DataBuffer.h"
#include "ObjectDict.h"

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

	Ref Space::TypeDataPipe(LuaState lua) {
		Ref type = lua.make_type<DataPipe>("DataPipe", std::ref(asyncWorker));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this_t()));
		return type;
	}

	Ref Space::TypeDataBuffer(LuaState lua) {
		Ref type = lua.make_type<DataBuffer>("DataBuffer", std::ref(asyncWorker));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this_t()));
		return type;
	}

	Ref Space::TypeObjectDict(LuaState lua) {
		Ref type = lua.make_type<ObjectDict>("ObjectDict", std::ref(asyncWorker));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this_t()));
		return type;
	}
}

