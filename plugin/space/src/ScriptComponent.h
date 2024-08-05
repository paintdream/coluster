// ScriptComponent.h
// PaintDream (paintdream@paintdream.com)
// 2023-12-11
//

#pragma once
#include "Space.h"
#include "glm/vec4.hpp"
#include "glm/geometric.hpp"

namespace coluster {
	class ScriptComponent {
	public:
		ScriptComponent(Ref&& r) noexcept : ref(std::move(r)) {}
		ScriptComponent(ScriptComponent&& r) noexcept : ref(std::move(r.ref)) {}
		~ScriptComponent() noexcept;

		ScriptComponent& operator = (ScriptComponent&& rhs) {
			ref = std::move(rhs.ref);
			return *this;
		}

		const Ref& GetObject() const noexcept {
			return ref;
		}

		Ref& GetObject() noexcept {
			return ref;
		}

	protected:
		Ref ref;
	};

	class ScriptComponentSystem : public ComponentSystem<ScriptComponent> {
	public:
		ScriptComponentSystem(Space& space);
		static void lua_registar(LuaState lua);

		bool Create(Entity entity, Ref&& ref);
		Result<Ref> GetObject(LuaState lua, Entity entity);
		Result<void> SetObject(LuaState lua, Entity entity, Ref&& ref);
	};
}