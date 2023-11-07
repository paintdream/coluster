// Database.h
// PaintDream (paintdream@paintdream.com)
// 2023-04-02
//

#pragma once

#include "../../../src/Coluster.h"

#if !COLUSTER_MONOLITHIC
#ifdef DATABASE_EXPORT
	#ifdef __GNUC__
		#define DATABASE_API __attribute__ ((visibility ("default")))
	#else
		#define DATABASE_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define DATABASE_API __attribute__ ((visibility ("default")))
	#else
		#define DATABASE_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif
#else
#define DATABASE_API
#endif

struct sqlite3;
struct sqlite3_stmt;

namespace coluster {
	class Storage;
	class Database : protected Warp, public EnableReadWriteFence {
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

