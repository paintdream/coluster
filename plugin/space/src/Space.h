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

		Entity Create();
		void Delete(Entity entity);
		void Clear();

	protected:
		AsyncWorker& asyncWorker;
		EntityAllocator<Entity> entityAllocator;
		Systems<Entity> theSystems;
	};
	
	template <typename Component>
	class ComponentSystem : public Object {
	public:
		using BaseClass = ComponentSystem;
		ComponentSystem(Space& s) noexcept : space(s) {
			space.GetSystems().attach(subSystem);
		}

		~ComponentSystem() override {
			space.GetSystems().detach(subSystem);
		}

		static void lua_registar(LuaState lua) {
			lua.set_current<&ComponentSystem::Delete>("Delete");
			lua.set_current<&ComponentSystem::Valid>("Valid");
			lua.set_current<&ComponentSystem::Clear>("Clear");
		}

		bool Valid(Entity entity) noexcept {
			return subSystem.valid(entity);
		}

		Result<void> Delete(Entity entity) {
			if (subSystem.remove(entity)) {
				return {};
			} else {
				return ResultError("Invalid entity!");
			}
		}

		void Clear() {
			subSystem.clear();
		}

	protected:
		Space& space;
		System<Entity, Component> subSystem;
	};
}

