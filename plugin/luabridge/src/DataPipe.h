// DataPipe.h
// PaintDream (paintdream@paintdream.com)
// 2024-1-5
//

#pragma once

#include "LuaBridge.h"

namespace coluster {
	class DataPipe : public Object, protected EnableInOutFence {
	public:
		~DataPipe() noexcept override;
		static void lua_registar(LuaState lua);

		void Push(std::string_view data);
		std::string Pop(LuaState state);
		bool Empty() const noexcept;

	protected:
		QueueList<uint8_t> dataQueueList;
	};
}
