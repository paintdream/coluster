#include "LinkComponent.h"

namespace coluster {
	LinkComponentSystem::LinkComponentSystem(Space& s) : BaseClass(s) {}

	void LinkComponentSystem::lua_registar(LuaState lua) {
		BaseClass::lua_registar(lua);

		lua.set_current<&LinkComponentSystem::Create>("Create");
		lua.set_current<&LinkComponentSystem::GetLink>("GetLink");
		lua.set_current<&LinkComponentSystem::SetLink>("SetLink");
	}

	bool LinkComponentSystem::Create(Entity entity, Entity n) {
		return subSystem.insert(entity, LinkComponent(n));
	}

	Result<Entity> LinkComponentSystem::GetLink(LuaState lua, Entity entity) {
		Ref ref;
		if (subSystem.filter<LinkComponent>(entity, [&lua, &ref](LinkComponent& node) noexcept {
			ref = lua.make_value(node.GetLink());
		})) {
			return ref;
		} else {
			return ResultError("Invalid entity!");
		}
	}

	Result<void> LinkComponentSystem::SetLink(LuaState lua, Entity entity, Entity linkEntity) {
		if (subSystem.filter<LinkComponent>(entity, [&lua, linkEntity](LinkComponent& node) noexcept {
			node.SetLink(linkEntity);
		})) {
			return {};
		} else {
			return ResultError("Invalid entity!");
		}
	}
}
