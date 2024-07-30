#include "ObjectDict.h"

namespace coluster {
	void ObjectDict::lua_registar(LuaState lua) {
		lua.set_current<&ObjectDict::Set>("Set");
		lua.set_current<&ObjectDict::Get>("Get");
	}

	ObjectDict::ObjectDict(AsyncWorker& asyncWorker) : objectDictMap(asyncWorker) {}
	ObjectDict::~ObjectDict() noexcept {}

	void ObjectDict::lua_initialize(LuaState lua, int index) noexcept {}

	Coroutine<void> ObjectDict::Set(std::string_view key, RefPtr<Object>&& object) {
		Warp* current = Warp::get_current_warp();
		co_await objectDictMap.Set(std::move(key), std::move(object));
		co_await Warp::Switch(std::source_location::current(), current);
	}

	Coroutine<RefPtr<Object>> ObjectDict::Get(LuaState lua, std::string_view key) {
		Warp* current = Warp::get_current_warp();
		RefPtr<Object>* result = co_await objectDictMap.Get(std::move(key));
		co_await Warp::Switch(std::source_location::current(), current);

		if (result != nullptr) {
			co_return result->as<RefPtr<Object>>(lua);
		} else {
			co_return RefPtr<Object>();
		}
	}
}