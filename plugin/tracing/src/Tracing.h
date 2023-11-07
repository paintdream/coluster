// Tracing.h
// PaintDream (paintdream@paintdream.com)
// 2023-07-02
//

#pragma once

#include "../../../src/Coluster.h"
#include "../../../src/AsyncMap.h"

#if !COLUSTER_MONOLITHIC
#ifdef TRACING_EXPORT
	#ifdef __GNUC__
		#define TRACING_API __attribute__ ((visibility ("default")))
	#else
		#define TRACING_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define TRACING_API __attribute__ ((visibility ("default")))
	#else
		#define TRACING_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif
#else
#define TRACING_API
#endif

namespace coluster {
	class Tracing : protected Warp {
	public:
		Tracing(AsyncWorker& asyncWorker);
		~Tracing() noexcept;

		void lua_initialize(LuaState lua, int index);
		void lua_finalize(LuaState lua, int index);
		static void lua_registar(LuaState lua);
	};
}

