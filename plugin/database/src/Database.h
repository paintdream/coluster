// Database.h
// PaintDream (paintdream@paintdream.com)
// 2023-04-02
//

#pragma once

#include "DatabaseCommon.h"

struct sqlite3;
struct sqlite3_stmt;

namespace coluster {
	class Storage;
	class Database : public Object, protected Warp {
	public:
		Database(AsyncWorker& asyncWorker);
		Warp& GetWarp() noexcept { return *this; }

		static void lua_registar(LuaState lua);
		Coroutine<bool> Initialize(std::string_view path, bool createIfNotExist);
		Coroutine<void> Uninitialize();
		Coroutine<Ref> Execute(LuaState lua, std::string_view sqlTemplate, Ref&& postData, bool asyncPost);
		void lua_finalize(LuaState lua, int index);

	protected:
		int WriteResult(LuaState lua, int stackIndex, int startIndex, sqlite3_stmt* stmt);
		void PostData(LuaState lua, sqlite3_stmt* stmt);
		void Close();

	protected:
		sqlite3* handle = nullptr;
	};
}

