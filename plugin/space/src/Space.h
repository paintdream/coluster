// Space.h
// PaintDream (paintdream@paintdream.com)
// 2023-07-02
//

#pragma once

#include "SpaceCommon.h"

namespace coluster {
	using Entity = uint32_t;
	class Space : public Object {
	public:
		Space(AsyncWorker& asyncWorker) noexcept;
		~Space() noexcept override;

		void lua_initialize(LuaState lua, int index);
		void lua_finalize(LuaState lua, int index);
		static void lua_registar(LuaState lua);

		Systems<Entity>& GetSystems() noexcept {
			return theSystems;
		}

		Ref TypeNodeComponentSystem(LuaState lua);
		Ref TypeTransformComponentSystem(LuaState lua);
		Ref TypeScriptComponentSystem(LuaState lua);
		Ref TypeLinkComponentSystem(LuaState lua);

		Entity CreateEntity();
		void DeleteEntity(Entity entity);
		void ClearEntities();

	protected:
		AsyncWorker& asyncWorker;
		EntityAllocator<Entity> entityAllocator;
		Systems<Entity> theSystems;
	};
}

