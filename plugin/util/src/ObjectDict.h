// ObjectDict.h
// PaintDream (paintdream@paintdream.com)
// 2024-1-5
//

#pragma once

#include "../../../src/Coluster.h"
#include "AsyncMap.h"

namespace coluster {
	class ObjectDict : public Object, protected EnableInOutFence {
	public:
		ObjectDict(AsyncWorker& asyncWorker);
		ObjectDict(ObjectDict&& rhs) = delete;
		ObjectDict(const ObjectDict& rhs) = delete;
		~ObjectDict() noexcept override;
		static void lua_registar(LuaState lua);
		void lua_initialize(LuaState lua, int index) noexcept;

		using MapType = AsyncMap<std::string, RefPtr<Object>, std::unordered_map>;
		MapType& GetMap() noexcept { return objectDictMap; }
		Coroutine<void> Set(std::string_view key, RefPtr<Object>&& object);
		Coroutine<RefPtr<Object>> Get(LuaState lua, std::string_view key);

	protected:
		MapType objectDictMap;
	};
}
