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
		lua.set_current<&ScriptComponentSystem::Valid>("Valid");
		lua.set_current<&ScriptComponentSystem::Clear>("Clear");
		lua.set_current<&ScriptComponentSystem::GetObject>("GetObject");
		lua.set_current<&ScriptComponentSystem::SetObject>("SetObject");
	}

	bool ScriptComponentSystem::Create(Entity entity, Ref&& ref) {
		return subSystem.insert(entity, ScriptComponent(std::move(ref)));
	}

	Result<void> ScriptComponentSystem::Delete(Entity entity) {
		if (subSystem.remove(entity)) {
			return {};
		} else {
			return ResultError("Invalid entity!");
		}
	}

	void ScriptComponentSystem::Clear() {
		subSystem.clear();
	}

	bool ScriptComponentSystem::Valid(Entity entity) noexcept {
		return subSystem.valid(entity);
	}

	Result<Ref> ScriptComponentSystem::GetObject(LuaState lua, Entity entity) {
		Ref ref;
		if (subSystem.filter<ScriptComponent>(entity, [&lua, &ref](ScriptComponent& node) noexcept {
			ref = lua.make_value(node.GetObject());
		})) {
			return ref;
		} else {
			return ResultError("Invalid entity!");
		}
	}

	Result<void> ScriptComponentSystem::SetObject(LuaState lua, Entity entity, Ref&& ref) {
		if (subSystem.filter<ScriptComponent>(entity, [&lua, &ref](ScriptComponent& node) noexcept {
			lua.deref(std::move(node.GetObject()));
			node.GetObject() = std::move(ref);
		})) {
			return {};
		} else {
			lua.deref(std::move(ref));
			return ResultError("Invalid entity!");
		}
	}
}
