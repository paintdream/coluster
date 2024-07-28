#include "ScriptComponent.h"

namespace coluster {
	ScriptComponent::~ScriptComponent() noexcept {
		if (ref) {
			Warp* currentWarp = Warp::get_current_warp();
			assert(currentWarp != nullptr);
			lua_State* L = currentWarp->GetLuaRoot();
			assert(L != nullptr);
			LuaState(L).deref(std::move(ref));
		}
	}

	ScriptComponentSystem::ScriptComponentSystem(Space& s) : space(s) {
		space.GetSystems().attach(subSystem);
	}

	ScriptComponentSystem::~ScriptComponentSystem() noexcept {
		space.GetSystems().detach(subSystem);
	}

	void ScriptComponentSystem::lua_initialize(LuaState lua, int index) {}
	void ScriptComponentSystem::lua_finalize(LuaState lua, int index) {}
	void ScriptComponentSystem::lua_registar(LuaState lua) {
		lua.set_current<&ScriptComponentSystem::Create>("Create");
		lua.set_current<&ScriptComponentSystem::Delete>("Delete");
		lua.set_current<&ScriptComponentSystem::Clear>("Clear");
		lua.set_current<&ScriptComponentSystem::GetObject>("GetObject");
		lua.set_current<&ScriptComponentSystem::SetObject>("SetObject");
	}

	bool ScriptComponentSystem::Create(Entity entity, Ref&& ref) {
		return subSystem.insert(entity, ScriptComponent(std::move(ref)));
	}

	Result<void> ScriptComponentSystem::Delete(Entity entity) {
		if (!subSystem.valid(entity)) {
			return ResultError("Invalid entity!");
		}

		subSystem.remove(entity);
		return {};
	}

	void ScriptComponentSystem::Clear() {
		subSystem.clear();
	}

	Result<Ref> ScriptComponentSystem::GetObject(LuaState lua, Entity entity) {
		if (!subSystem.valid(entity)) {
			return ResultError("Invalid entity!");
		}

		Ref ref;

		subSystem.for_entity<ScriptComponent>(entity, [&lua, &ref](ScriptComponent& node) noexcept {
			ref = lua.make_value(node.GetObject());
		});

		return ref;
	}

	Result<void> ScriptComponentSystem::SetObject(LuaState lua, Entity entity, Ref&& ref) {
		if (!subSystem.valid(entity)) {
			return ResultError("Invalid entity!");
		}

		subSystem.for_entity<ScriptComponent>(entity, [&lua, &ref](ScriptComponent& node) noexcept {
			lua.deref(std::move(node.GetObject()));
			node.GetObject() = std::move(ref);
		});

		return {};
	}
}
