// Util.h
// PaintDream (paintdream@paintdream.com)
// 2023-07-02
//

#pragma once

#include "UtilCommon.h"

namespace coluster {
	class Util : public Object {
	public:
		Util(AsyncWorker& asyncWorker) noexcept;
		~Util() noexcept override;

		void lua_initialize(LuaState lua, int index);
		void lua_finalize(LuaState lua, int index);
		static void lua_registar(LuaState lua);

		Ref TypeDataPipe(LuaState lua);
		Ref TypeDataBuffer(LuaState lua);
		Ref TypeObjectDict(LuaState lua);

	protected:
		AsyncWorker& asyncWorker;
	};
}

