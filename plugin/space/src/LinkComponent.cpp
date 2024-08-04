#include "LinkComponent.h"

namespace coluster {
	LinkComponentSystem::LinkComponentSystem(Space& s) : space(s) {
		space.GetSystems().attach(subSystem);
	}

	LinkComponentSystem::~LinkComponentSystem() noexcept {
		space.GetSystems().detach(subSystem);
	}

	void LinkComponentSystem::lua_initialize(LuaState lua, int index) {}
	void LinkComponentSystem::lua_finalize(LuaState lua, int index) {}
	void LinkComponentSystem::lua_registar(LuaState lua) {
		lua.set_current<&LinkComponentSystem::Create>("Create");
		lua.set_current<&LinkComponentSystem::Delete>("Delete");
		lua.set_current<&LinkComponentSystem::Valid>("Valid");
		lua.set_current<&LinkComponentSystem::Clear>("Clear");
		lua.set_current<&LinkComponentSystem::GetLinkEntity>("GetLinkEntity");
	}

	bool LinkComponentSystem::Create(Entity entity, Entity linkEntity) {
		return subSystem.insert(entity, LinkComponent(linkEntity));
	}

	Result<void> LinkComponentSystem::Delete(Entity entity) {
		if (subSystem.remove(entity)) {
			return {};
		} else {
			return ResultError("Invalid entity!");
		}
	}

	void LinkComponentSystem::Clear() {
		subSystem.clear();
	}

	bool LinkComponentSystem::Valid(Entity entity) noexcept {
		return subSystem.valid(entity);
	}

	Result<Entity> LinkComponentSystem::GetLinkEntity(LuaState lua, Entity entity) {
		Ref ref;
		if (subSystem.filter<LinkComponent>(entity, [&lua, &ref](LinkComponent& node) noexcept {
			ref = lua.make_value(node.GetLinkEntity());
		})) {
			return ref;
		} else {
			return ResultError("Invalid entity!");
		}
	}

	Result<void> LinkComponentSystem::SetLinkEntity(LuaState lua, Entity entity, Entity linkEntity) {
		if (subSystem.filter<LinkComponent>(entity, [&lua, linkEntity](LinkComponent& node) noexcept {
			node.SetLinkEntity(linkEntity);
		})) {
			return {};
		} else {
			return ResultError("Invalid entity!");
		}
	}
}
