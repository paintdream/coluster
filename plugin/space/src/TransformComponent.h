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
		TransformComponent() noexcept;

	protected:
		Matrix4x4 matrix;
	};

	class TransformComponentSystem : public Object {
	public:
		TransformComponentSystem(Space& space);
		~TransformComponentSystem() noexcept;
	
		void lua_initialize(LuaState lua, int index);
		void lua_finalize(LuaState lua, int index);
		static void lua_registar(LuaState lua);

	protected:
		Space& space;
		System<Entity, TransformComponent> subSystem;
	};
}