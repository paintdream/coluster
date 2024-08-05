// TransformComponent.h
// PaintDream (paintdream@paintdream.com)
// 2023-12-11
//

#pragma once
#include "Space.h"
#include "glm/mat4x4.hpp"
#include "glm/geometric.hpp"

namespace coluster {
	using Matrix4x4 = glm::mat4x4;
	class TransformComponent {
	public:
		TransformComponent(const Matrix4x4& mat) noexcept : matrix(mat) {}

	protected:
		Matrix4x4 matrix;
	};

	class TransformComponentSystem : public ComponentSystem<TransformComponent> {
	public:
		TransformComponentSystem(Space& space);
		static void lua_registar(LuaState lua);
		bool Create(Entity entity, const std::array<float, 16>& data);
	};
}