// Mapping.h
// PaintDream (paintdream@paintdream.com)
// 2023-07-02
//

#pragma once

#include "../../../src/Coluster.h"
#include "../../../src/AsyncMap.h"

#ifdef MAPPING_EXPORT
	#ifdef __GNUC__
		#define MAPPING_API __attribute__ ((visibility ("default")))
	#else
		#define MAPPING_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define MAPPING_API __attribute__ ((visibility ("default")))
	#else
		#define MAPPING_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif

namespace coluster {
	class Mapping {
	public:
		using PoolBase = Pool<Mapping, lua_State*, 256>;
		Mapping(AsyncWorker& asyncWorker);
		~Mapping() noexcept;

		class Object {
		public:
			Object() noexcept;
			virtual ~Object() noexcept;
		};

		Coroutine<RefPtr<Object>> Get(LuaState lua, std::string_view path);
		void lua_initialize(LuaState lua, int index);
		void lua_finalize(LuaState lua, int index);
		static void lua_registar(LuaState lua);

	protected:
		AsyncMap<std::string, std::weak_ptr<Object>, std::unordered_map> asyncMap;
	};
}

