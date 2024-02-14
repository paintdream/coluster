// Space.h
// PaintDream (paintdream@paintdream.com)
// 2023-07-02
//

#pragma once
#include "../../../src/Coluster.h"

#if !COLUSTER_MONOLITHIC
#ifdef SPACE_EXPORT
	#ifdef __GNUC__
		#define SPACE_API __attribute__ ((visibility ("default")))
	#else
		#define SPACE_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define SPACE_API __attribute__ ((visibility ("default")))
	#else
		#define SPACE_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif
#else
#define SPACE_API
#endif

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

