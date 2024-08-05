#include "TransformComponent.h"
#include "NodeComponent.h"

namespace coluster {
	TransformComponentSystem::TransformComponentSystem(Space& s) : BaseClass(s) {}

	bool TransformComponentSystem::Create(Entity entity, const std::array<float, 16>& data) {
		static_assert(sizeof(Matrix4x4) == sizeof(data), "Matrix size mismatch!");
		Matrix4x4 mat;
		memcpy(&mat, data.data(), sizeof(Matrix4x4));

		return subSystem.insert(entity, TransformComponent(mat));
	}

	void TransformComponentSystem::lua_registar(LuaState lua) {
		BaseClass::lua_registar(lua);

		lua.set_current<&TransformComponentSystem::Create>("Create");
	}
}
