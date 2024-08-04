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
		Entity GetLinkEntity() const noexcept {
			return linkEntity;
		}

		void SetLinkEntity(Entity n) noexcept {
			linkEntity = n;
		}

	protected:
		Entity linkEntity;
	};

	class LinkComponentSystem : public Object {
	public:
		LinkComponentSystem(Space& space);
		~LinkComponentSystem() noexcept;
	
		void lua_initialize(LuaState lua, int index);
		void lua_finalize(LuaState lua, int index);
		static void lua_registar(LuaState lua);

		bool Create(Entity entity, Entity linkEntity);
		bool Valid(Entity entity) noexcept;
		Result<void> Delete(Entity entity);
		void Clear();

		Result<Entity> GetLinkEntity(LuaState lua, Entity entity);
		Result<void> SetLinkEntity(LuaState lua, Entity entity, Entity linkEntity);

	protected:
		Space& space;
		System<Entity, LinkComponent> subSystem;
	};
}