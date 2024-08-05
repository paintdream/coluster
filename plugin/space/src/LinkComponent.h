// LinkComponent.h
// PaintDream (paintdream@paintdream.com)
// 2023-12-11
//

#pragma once
#include "Space.h"

namespace coluster {
	class LinkComponent {
	public:
		LinkComponent(Entity n) noexcept : linkEntity(n) {}

		Entity GetLink() const noexcept {
			return linkEntity;
		}

		void SetLink(Entity n) noexcept {
			linkEntity = n;
		}

	protected:
		Entity linkEntity;
	};

	class LinkComponentSystem : public ComponentSystem<LinkComponent> {
	public:
		LinkComponentSystem(Space& space);
		static void lua_registar(LuaState lua);

		bool Create(Entity entity, Entity n);
		Result<Entity> GetLink(LuaState lua, Entity entity);
		Result<void> SetLink(LuaState lua, Entity entity, Entity linkEntity);
	};
}