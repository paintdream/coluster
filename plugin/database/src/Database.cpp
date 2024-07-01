#include "Database.h"
#include "../ref/sqlite3/sqlite3.h"
#include <sstream>

namespace coluster {
	Database::Database(AsyncWorker& asyncWorker) : Warp(asyncWorker) {}

	void Database::lua_registar(LuaState lua) {
		lua.set_current<&Database::Initialize>("Initialize");
		lua.set_current<&Database::Uninitialize>("Uninitialize");
		lua.set_current<&Database::Execute>("Execute");
	}

	Coroutine<Result<bool>> Database::Initialize(std::string_view path, bool createIfNotExist) {
		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &GetWarp());
		if (handle != nullptr) {
			co_return ResultError("[WARNING] Database already initialized!");
		}

		bool result = sqlite3_open_v2(path.data(), &handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | (createIfNotExist ? SQLITE_OPEN_CREATE : 0), nullptr) == SQLITE_OK;
		co_await Warp::Switch(std::source_location::current(), currentWarp);
		status = result ? Status::Ready : Status::Invalid;
		co_return std::move(result);
	}

	void Database::Close() {
		if (handle != nullptr) {
			sqlite3_close_v2(handle);
			handle = nullptr;
		}

		status = Status::Invalid;
	}

	Coroutine<void> Database::Uninitialize() {
		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &GetWarp());
		Close();
		co_await Warp::Switch(std::source_location::current(), currentWarp);
	}

	void Database::lua_initialize(LuaState lua, int index) {
		lua_State* L = lua.get_state();
		LuaState::stack_guard_t guard(L);
		dataExchangeRef = lua.make_thread([&](LuaState lua) {
			dataExchangeStack = lua.get_state();
		});
	}

	void Database::lua_finalize(LuaState lua, int index) {
		status = Status::Invalid;

		dataExchangeStack = nullptr;
		lua.deref(std::move(dataExchangeRef));

		get_async_worker().Synchronize(lua, this);
		Close();
	}

	int Database::WriteResult(LuaState lua, int stackIndex, int startIndex, sqlite3_stmt* stmt) {
		auto guard = lua.stack_guard();
		lua_State* L = lua.get_state();
		int count = sqlite3_column_count(stmt);
		int n = 1;
		int status;

		do {
			lua_createtable(L, count, 0);
			int k = 1;
			for (int i = 0; i < count; i++) {
				// const char* name = sqlite3_column_name(stmt, i);
				// if (name == nullptr) name = "";
				int t = sqlite3_value_type(sqlite3_column_value(stmt, i));
				switch (t) {
					case SQLITE_INTEGER:
						lua_pushinteger(L, static_cast<lua_Integer>(sqlite3_column_int64(stmt, i)));
						break;
					case SQLITE_FLOAT:
						lua_pushnumber(L, sqlite3_column_double(stmt, i));
						break;
					case SQLITE_BLOB:
						lua_pushlstring(L, (const char*)sqlite3_column_blob(stmt, i), sqlite3_column_bytes(stmt, i));
						break;
					case SQLITE_NULL:
						lua_pushnil(L);
						break;
					case SQLITE_TEXT:
						lua_pushlstring(L, (const char*)sqlite3_column_text(stmt, i), sqlite3_column_bytes(stmt, i));
						break;
					default:
						lua_pushnil(L);
						break;
				}

				lua_rawseti(L, -2, k++);
			}

			lua_rawseti(L, stackIndex, startIndex++);
			status = sqlite3_step(stmt);
		} while (status == SQLITE_ROW);

		return startIndex;
	}

	void Database::PostData(LuaState lua, sqlite3_stmt* stmt) {
		auto guard = lua.stack_guard();
		lua_State* L = lua.get_state();
		int tupleSize = iris::iris_verify_cast<int>(lua_rawlen(L, -1));

		for (int j = 1; j <= tupleSize; j++) {
#if LUA_VERSION_NUM <= 502
			lua_rawgeti(L, -1, j);
			int type = lua_type(L, -1);
#else
			int type = lua_rawgeti(L, -1, j);
#endif
			switch (type) {
				case LUA_TSTRING:
				{
					size_t len = 0;
					const char* str = lua_tolstring(L, -1, &len);
					sqlite3_bind_text(stmt, j, str, iris::iris_verify_cast<int>(len), SQLITE_TRANSIENT);
					break;
				}
				case LUA_TNUMBER:
				{
#if LUA_VERSION_NUM <= 502
					sqlite3_bind_double(stmt, j, lua_tonumber(L, -1));
#else
					if (lua_isinteger(L, -1)) {
						sqlite3_bind_int64(stmt, j, lua_tointeger(L, -1));
					} else {
						sqlite3_bind_double(stmt, j, lua_tonumber(L, -1));
					}
#endif

					break;
				}
				default:
				{
					sqlite3_bind_null(stmt, j);
					break;
				}
			}

			lua_pop(L, 1);
		}
	}

	Coroutine<Result<Ref>> Database::Execute(LuaState lua, std::string_view sqlTemplate, Ref&& argPostData, bool asyncPost) {
		if (handle == nullptr || status == Status::Invalid) {
			co_return ResultError("[ERROR] Database::Execute() -> Uninitialized database!");
		}

		if (status != Status::Ready) {
			co_return ResultError("[ERROR] Database::Execute() -> Not Ready!");
		}

		status = Status::Pending;
		// use raw lua operations for better performance
		lua_State* D = dataExchangeStack;
		LuaState dataStack(D);
		lua_newtable(D); // prepare result

		sqlite3_stmt* stmt;
		Ref postData(std::move(argPostData));
		auto refGuard = lua.ref_guard(postData);

		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), Warp::get_current_warp(), &GetWarp());
		std::string message;

		if (sqlite3_prepare_v2(handle, sqlTemplate.data(), -1, &stmt, 0) == SQLITE_OK) {
			if (postData) {
				lua_rawgeti(D, LUA_REGISTRYINDEX, postData.get_ref_value());
				int size = iris::iris_verify_cast<int>(lua_rawlen(D, -1));

				int startIndex = 1;
				for (int i = 1; i <= size; i++) {
					lua_rawgeti(D, -1, i);

					if (lua_istable(D, -1)) {
						PostData(dataStack, stmt);

						if (asyncPost) {
							co_await Warp::Switch(std::source_location::current(), &GetWarp());
						}

						int status = sqlite3_step(stmt);

						if (asyncPost) {
							co_await Warp::Switch(std::source_location::current(), currentWarp, &GetWarp());
						}

						if (status == SQLITE_ROW) {
							startIndex = WriteResult(dataStack, -4, startIndex, stmt);
						} else if (status != SQLITE_DONE) {
							message = message + "[ERROR] Database::Execute() -> " + sqlite3_errmsg(handle) + "\n";
						}

						sqlite3_reset(stmt);
					}

					lua_pop(D, 1);
				}

				lua_pop(D, 1);
			} else {
				Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &GetWarp());
				int status = sqlite3_step(stmt);
				co_await Warp::Switch(std::source_location::current(), currentWarp, &GetWarp());

				if (status == SQLITE_ROW) {
					WriteResult(dataStack, -2, 1, stmt);
				} else if (status != SQLITE_DONE) {
					message = message + "[ERROR] Database::Execute() -> " + sqlite3_errmsg(handle) + "\n";
				}
			}

			sqlite3_finalize(stmt);
		} else {
			message = message + "[ERROR] Database::Execute() -> " + sqlite3_errmsg(handle) + "\n";
		}

		co_await Warp::Switch(std::source_location::current(), currentWarp);
		assert(lua_gettop(D) == 1);
		status = Status::Ready;

		lua_xmove(D, lua.get_state(), 1);
		if (message.empty()) {
			co_return Ref(luaL_ref(lua.get_state(), LUA_REGISTRYINDEX));
		} else {
			lua_pop(lua.get_state(), 1);
			co_return ResultError(std::move(message));
		}
	}
}

