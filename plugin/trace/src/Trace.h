// Trace.h
// PaintDream (paintdream@paintdream.com)
// 2023-07-02
//

#pragma once

#include "TraceCommon.h"

namespace coluster {
	class Trace : public Object, protected Warp {
	public:
		Trace(AsyncWorker& asyncWorker);
		~Trace() noexcept override;

		void lua_initialize(LuaState lua, int index);
		void lua_finalize(LuaState lua, int index);
		static void lua_registar(LuaState lua);
	};
}

