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

	ScriptComponentSystem::ScriptComponentSystem(Space& s) : BaseClass(s) {}

	void ScriptComponentSystem::lua_registar(LuaState lua) {
		BaseClass::lua_registar(lua);

		lua.set_current<&ScriptComponentSystem::Create>("Create");
		lua.set_current<&ScriptComponentSystem::GetObject>("GetObject");
		lua.set_current<&ScriptComponentSystem::SetObject>("SetObject");
	}

	bool ScriptComponentSystem::Create(Entity entity, Ref&& ref) {
		return subSystem.insert(entity, ScriptComponent(std::move(ref)));
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
