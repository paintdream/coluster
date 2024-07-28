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

	bool TransformComponentSystem::Create(Entity entity, const std::array<float, 16>& data) {
		static_assert(sizeof(Matrix4x4) == sizeof(data), "Matrix size mismatch!");
		Matrix4x4 mat;
		memcpy(&mat, data.data(), sizeof(Matrix4x4));

		return subSystem.insert(entity, TransformComponent(mat));
	}

	Result<void> TransformComponentSystem::Delete(Entity entity) {
		if (!subSystem.valid(entity)) {
			return ResultError("Entity not found!");
		}

		subSystem.remove(entity);
		return {};
	}

	void TransformComponentSystem::Clear() {
		subSystem.clear();
	}

	void TransformComponentSystem::lua_initialize(LuaState lua, int index) {}
	void TransformComponentSystem::lua_finalize(LuaState lua, int index) {}
	void TransformComponentSystem::lua_registar(LuaState lua) {
		lua.set_current<&TransformComponentSystem::Create>("Create");
		lua.set_current<&TransformComponentSystem::Delete>("Delete");
		lua.set_current<&TransformComponentSystem::Clear>("Clear");
	}
}
