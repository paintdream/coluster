// Graphic.h
// PaintDream (paintdream@paintdream.com)
// 2023-07-02
//

#pragma once

#include "../../../src/Coluster.h"

#if !COLUSTER_MONOLITHIC
#ifdef GRAPHIC_EXPORT
	#ifdef __GNUC__
		#define GRAPHIC_API __attribute__ ((visibility ("default")))
	#else
		#define GRAPHIC_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define GRAPHIC_API __attribute__ ((visibility ("default")))
	#else
		#define GRAPHIC_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif
#else
#define GRAPHIC_API
#endif

namespace coluster {
	class Graphic {
	public:
		Graphic(AsyncWorker& asyncWorker);
		~Graphic() noexcept;

		void lua_initialize(LuaState lua, int index);
		void lua_finalize(LuaState lua, int index);
		static void lua_registar(LuaState lua);

	protected:
		AsyncWorker& asyncWorker;
	};
}

