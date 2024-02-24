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

		Ref TypeNodeSystem(LuaState lua);
		Entity NewEntity();
		void DeleteEntity(Entity entity);

	protected:
		AsyncWorker& asyncWorker;
		EntityAllocator<Entity> entityAllocator;
		Systems<Entity> theSystems;
	};
}

