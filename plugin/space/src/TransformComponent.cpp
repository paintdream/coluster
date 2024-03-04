#include "TransformComponent.h"
#include "NodeComponent.h"

namespace coluster {
	TransformComponent::TransformComponent() noexcept {}

	TransformComponentSystem::TransformComponentSystem(Space& s) : space(s) {
		space.GetSystems().attach(subSystem);
	}

	TransformComponentSystem::~TransformComponentSystem() noexcept {
		space.GetSystems().detach(subSystem);
	}

	void TransformComponentSystem::lua_initialize(LuaState lua, int index) {}
	void TransformComponentSystem::lua_finalize(LuaState lua, int index) {}
	void TransformComponentSystem::lua_registar(LuaState lua) {
	}
}
