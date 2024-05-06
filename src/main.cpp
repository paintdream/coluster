#include "Coluster.h"
#include "../ref/iris/src/iris_common.inl" // implementation of memory management apis
#include <chrono>
#include <signal.h>
using namespace coluster;

#if LUA_VERSION_NUM <= 501
int luaL_requiref(lua_State* L, const char* modname, lua_CFunction openf, int glb) {
	int rawtop = lua_gettop(L);
	lua_getglobal(L, "package");
	if (lua_type(L, -1) == LUA_TTABLE) {
		lua_getfield(L, -1, "loaded");
		if (lua_type(L, -1) == LUA_TTABLE) {
			lua_pushstring(L, modname);
			int top = lua_gettop(L);
			int ret = openf(L);
			if (ret >= 1) {
				lua_settop(L, top + 1);
				lua_rawset(L, -3);
			}
		}
	}

	lua_settop(L, rawtop + 1);
	return 1;
}
#endif

#include "plugins.inl"

class Coluster : public AsyncWorker {
public:
	enum Status : size_t {
		Status_Ready,
		Status_Running,
		Status_Stopping,
	};

	// Lua stubs
	Coluster();
	static void lua_registar(LuaState lua);
	void lua_initialize(LuaState lua, int index);
	void lua_finalize(LuaState lua, int index);
	static void LuaHook(lua_State* L, lua_Debug* ar);

	// Methods
	std::string_view GetStatus() const noexcept;
	bool Start(LuaState lua, size_t threadCount);
	bool Join(LuaState lua, Ref&& finalizer, bool enableConsole);
	bool Post(LuaState lua, Ref&& callback);
	bool Poll(bool pollAsyncTasks);
	bool Stop();
	void Sleep(size_t milliseconds);
	Ref GetProfile(LuaState lua);
	AsyncWorker::MemoryQuota::amount_t GetQuota() noexcept;

	static size_t GetHardwareConcurrency() noexcept;
	size_t GetWorkerThreadCount() const noexcept;
	size_t GetTaskCount() const noexcept;
	bool IsMainThread() const noexcept;
	bool IsWorkerTerminated() const noexcept;

protected:
	void doREPL(lua_State* L);
	int pushline(lua_State* L, int firstline);
	int multiline(lua_State* L);
	int loadline(lua_State* L);

protected:
	LuaState cothread;
	Ref cothreadRef;
	size_t mainThreadIndex = ~size_t(0);
	std::atomic<Status> workerStatus = Status_Ready;
};

Coluster::Coluster() : cothread(nullptr) {}

// Lua stubs
void Coluster::lua_registar(LuaState lua) {
	lua.set_current<&Coluster::Start>("Start");
	lua.set_current<&Coluster::Join>("Join");
	lua.set_current<&Coluster::Post>("Post");
	lua.set_current<&Coluster::Poll>("Poll");
	lua.set_current<&Coluster::Stop>("Stop");
	lua.set_current<&Coluster::Sleep>("Sleep");
	lua.set_current<&Coluster::GetProfile>("GetProfile");
	lua.set_current<&Coluster::GetQuota>("GetQuota");
	lua.set_current<&Coluster::GetStatus>("GetStatus");
	lua.set_current<&Coluster::GetHardwareConcurrency>("GetHardwareConcurrency");
	lua.set_current<&Coluster::GetWorkerThreadCount>("GetWorkerThreadCount");
	lua.set_current<&Coluster::IsWorkerTerminated>("IsWorkerTerminated");
	lua.set_current<&Coluster::GetTaskCount>("GetTaskCount");
	lua.set_current<&Coluster::IsMainThread>("IsMainThread");
}

bool Coluster::IsMainThread() const noexcept {
	return get_current_thread_index() == mainThreadIndex;
}

bool Coluster::IsWorkerTerminated() const noexcept {
	return is_terminated();
}

size_t Coluster::GetWorkerThreadCount() const noexcept {
	return get_thread_count();
}

size_t Coluster::GetTaskCount() const noexcept {
	return AsyncWorker::get_task_count();
}

size_t Coluster::GetHardwareConcurrency() noexcept {
	return std::thread::hardware_concurrency();
}

void Coluster::LuaHook(lua_State* L, lua_Debug* ar) {
	std::string_view name;

	if (lua_getinfo(L, "nS", ar)) {
		if (ar->name != nullptr) {
			name = ar->name;
		} else {
			name = "function ()";
		}
	}

	if (ar->event == LUA_HOOKCALL) {
	} else if (ar->event == LUA_HOOKRET) {
	}
}

void Coluster::lua_initialize(LuaState lua, int index) {
	lua_State* L = lua.get_state();
	lua_sethook(L, LuaHook, LUA_MASKCALL | LUA_MASKRET, 0);
	cothreadRef = lua.make_thread([&](LuaState lua) {
		cothread = lua;
	});

	lua_pushvalue(lua.get_state(), index);
	luaL_ref(L, LUA_REGISTRYINDEX);
}

void Coluster::lua_finalize(LuaState lua, int index) {
	if (workerStatus.load(std::memory_order_acquire) == Status_Running) {
		Stop();
		Join(lua, Ref(), false);
		assert(workerStatus.load(std::memory_order_acquire) == Status_Ready);
	}

	lua.deref(std::move(cothreadRef));
	cothread = LuaState(nullptr);
	lua_State* L = lua.get_state();
	lua_sethook(L, LuaHook, 0, 0);
}

// Methods

std::string_view Coluster::GetStatus() const noexcept {
	switch (workerStatus.load(std::memory_order_acquire)) {
		case Status_Ready:
			return "Ready";
		case Status_Running:
			return "Running";
		case Status_Stopping:
			return "Stoping";
	}

	return "Unknown";
}

bool Coluster::Start(LuaState lua, size_t threadCount) {
	if (AsyncWorker::get_current_thread_index() != ~(size_t)0)
		return false;

	size_t count = std::thread::hardware_concurrency();
	if (threadCount != 0) {
		count = std::min(threadCount, count + Priority_Count); // at most hardware_concurrency + Priority_Count threads
	}

	count = std::max(count, iris::iris_verify_cast<size_t>(Priority_Count)); // at least Priority_Count threads

	Status expected = Status_Ready;
	if (workerStatus.compare_exchange_strong(expected, Status_Running, std::memory_order_relaxed)) {
		AsyncWorker::resize(count);
		mainThreadIndex = AsyncWorker::append(std::thread()); // for main thread polling
		AsyncWorker::start();
		AsyncWorker::SetupSharedWarps(count);

		scriptWarp = std::make_unique<Warp>(*this);
		scriptWarp->BindLuaRoot(cothread);
		scriptWarp->Acquire();

		AsyncWorker::make_current(mainThreadIndex);

		return true;
	} else {
		return false;
	}
}

void Coluster::Sleep(size_t milliseconds) {
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

AsyncWorker::MemoryQuota::amount_t Coluster::GetQuota() noexcept {
	return GetMemoryQuotaQueue().GetAmount();
}

Ref Coluster::GetProfile(LuaState lua) {
	if (scriptWarp) {
		LuaState::stack_guard_t guard(lua.get_state());
		return scriptWarp->GetProfileTable().as<Ref>(lua);
	} else {
		fprintf(stderr, "[ERROR] Cannot GetProfile while coluster is not running!");
		return Ref();
	}
}

bool Coluster::Post(LuaState lua, Ref&& callback) {
	if (scriptWarp && callback) {
		scriptWarp->queue_routine_post([this, callback = std::make_shared<Ref>(std::move(callback))]() mutable {
			assert(cothread);
			cothread.call<void>(std::move(*callback.get()));
		});

		return true;
	} else {
		lua.deref(std::move(callback));
		fprintf(stderr, "[ERROR] Cannot Post new routines while coluster is not running!");
		return false;
	}
}

bool Coluster::Stop() {
	Status expected = Status_Running;
	if (workerStatus.compare_exchange_strong(expected, Status_Stopping, std::memory_order_relaxed)) {
		AsyncWorker::terminate();
		return true;
	} else {
		return false;
	}
}

extern "C" COLUSTER_CORE_API int luaopen_coluster(lua_State * L) {
	ColusterRegisterPlugins(L);
	return LuaState::forward(L, [](LuaState luaState) {
		return luaState.make_type<Coluster>("Coluster");
	});
}

// copied from lua.c

/*
** Prints an error message, adding the program name in front of it
** (if present)
*/
static void l_message(const char* pname, const char* msg) {
	if (pname) fprintf(stderr, "%s: ", pname);
	fprintf(stderr, "%s\n", msg);
}

/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack. It assumes that the error object
** is a string, as it was either generated by Lua or by 'msghandler'.
*/
static int report(lua_State* L, int status) {
	if (status != LUA_OK) {
		const char* msg = lua_tostring(L, -1);
		l_message("Coluster", msg);
		lua_pop(L, 1);  /* remove message */
	}
	return status;
}


/*
** Message handler used to run all chunks
*/
static int msghandler(lua_State* L) {
	const char* msg = lua_tostring(L, 1);
	if (msg == NULL) {  /* is error object not a string? */
		if (luaL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
			lua_type(L, -1) == LUA_TSTRING)  /* that produces a string? */
			return 1;  /* that is the message */
		else {
			msg = lua_pushfstring(L, "(error object is a %s value)",
				luaL_typename(L, 1));
			lua_replace(L, -2);
		}
	}

	lua_getglobal(L, "debug");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return 1;
	}
	lua_getfield(L, -1, "traceback");
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 2);
		return 1;
	}
	lua_pushvalue(L, 1);  /* pass error message */
	lua_pushinteger(L, 2);  /* skip this function and traceback */
	lua_call(L, 2, 1);  /* call debug.traceback */

	return 1;
}


/*
** Interface to 'lua_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall(lua_State* L, int narg, int nres) {
	int status;
	int base = lua_gettop(L) - narg;  /* function index */
	lua_pushcfunction(L, msghandler);  /* push message handler */
	lua_insert(L, base);  /* put it under function and args */
	status = lua_pcall(L, narg, nres, base);
	lua_remove(L, base);  /* remove message handler from the stack */
	return status;
}

/*
** {==================================================================
** Read-Eval-Print Join (REPL)
** ===================================================================
*/

#if !defined(LUA_PROMPT)
#define LUA_PROMPT		"=> "
#define LUA_PROMPT2		"=>> "
#endif

#if !defined(LUA_MAXINPUT)
#define LUA_MAXINPUT		512
#endif


/*
** lua_stdin_is_tty detects whether the standard input is a 'tty' (that
** is, whether we're running lua interactively).
*/
#if !defined(lua_stdin_is_tty)	/* { */

#if defined(LUA_USE_POSIX)	/* { */

#include <unistd.h>
#define lua_stdin_is_tty()	isatty(0)

#elif defined(LUA_USE_WINDOWS)	/* }{ */

#include <io.h>
#include <windows.h>

#define lua_stdin_is_tty()	_isatty(_fileno(stdin))

#else				/* }{ */

/* ISO C definition */
#define lua_stdin_is_tty()	1  /* assume stdin is a tty */

#endif				/* } */

#endif				/* } */


/*
** lua_readline defines how to show a prompt and then read a line from
** the standard input.
** lua_saveline defines how to "save" a read line in a "history".
** lua_freeline defines how to free a line read by lua_readline.
*/
#if !defined(lua_readline)	/* { */

#if defined(LUA_USE_READLINE)	/* { */

#include <readline/readline.h>
#include <readline/history.h>
#define lua_initreadline(L)	((void)L, rl_readline_name="lua")
#define lua_readline(L,b,p)	((void)L, ((b)=readline(p)) != NULL)
#define lua_saveline(L,line)	((void)L, add_history(line))
#define lua_freeline(L,b)	((void)L, free(b))

#else				/* }{ */

#define lua_initreadline(L)  ((void)L)
#define lua_readline(L,b,p) \
        ((void)L, fputs(p, stdout), fflush(stdout),  /* show prompt */ \
        fgets(b, LUA_MAXINPUT, stdin) != NULL)  /* get line */
#define lua_saveline(L,line)	{ (void)L; (void)line; }
#define lua_freeline(L,b)	{ (void)L; (void)b; }

#endif				/* } */

#endif				/* } */


/*
** Return the string to be used as a prompt by the interpreter. Leave
** the string (or nil, if using the default value) on the stack, to keep
** it anchored.
*/
static const char* get_prompt(lua_State* L, int firstline) {
	lua_pushnil(L);
	return firstline ? LUA_PROMPT : LUA_PROMPT2;  /* use the default */
}

/* mark in error messages for incomplete statements */
#define EOFMARK		"<eof>"
#define marklen		(sizeof(EOFMARK)/sizeof(char) - 1)


/*
** Check whether 'status' signals a syntax error and the error
** message at the top of the stack ends with the above mark for
** incomplete statements.
*/
static int incomplete(lua_State* L, int status) {
	if (status == LUA_ERRSYNTAX) {
		size_t lmsg;
		const char* msg = lua_tolstring(L, -1, &lmsg);
		if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0) {
			lua_pop(L, 1);
			return 1;
		}
	}
	return 0;  /* else... */
}


/*
** Prompt the user, read a line, and push it into the Lua stack.
*/
int Coluster::pushline(lua_State* L, int firstline) {
	char buffer[LUA_MAXINPUT];
	char* b = buffer;
	size_t l;
	const char* prmt = get_prompt(L, firstline);
	scriptWarp->Release();
	int readstatus = lua_readline(L, b, prmt);
	scriptWarp->Acquire();
	if (readstatus == 0)
		return 0;  /* no input (prompt will be popped by caller) */
	lua_pop(L, 1);  /* remove prompt */
	l = strlen(b);
	if (l > 0 && b[l - 1] == '\n')  /* line ends with newline? */
		b[--l] = '\0';  /* remove it */
	if (firstline && b[0] == '=')  /* for compatibility with 5.2, ... */
		lua_pushfstring(L, "return %s", b + 1);  /* change '=' to 'return' */
	else
		lua_pushlstring(L, b, l);
	lua_freeline(L, b);
	return 1;
}


/*
** Try to compile line on the stack as 'return <line>;'; on return, stack
** has either compiled chunk or original line (if compilation failed).
*/
static int addreturn(lua_State* L) {
	const char* line = lua_tostring(L, -1);  /* original line */
	const char* retline = lua_pushfstring(L, "return %s;", line);
	int status = luaL_loadbuffer(L, retline, strlen(retline), "=stdin");
	if (status == LUA_OK) {
		lua_remove(L, -2);  /* remove modified line */
		if (line[0] != '\0')  /* non empty? */
			lua_saveline(L, line);  /* keep history */
	} else
		lua_pop(L, 2);  /* pop result from 'luaL_loadbuffer' and modified line */
	return status;
}


/*
** Read multiple lines until a complete Lua statement
*/
int Coluster::multiline(lua_State* L) {
	for (;;) {  /* repeat until gets a complete statement */
		size_t len;
		const char* line = lua_tolstring(L, 1, &len);  /* get what it has */
		int status = luaL_loadbuffer(L, line, len, "=stdin");  /* try it */
		if (!incomplete(L, status) || !pushline(L, 0)) {
			lua_saveline(L, line);  /* keep history */
			return status;  /* cannot or should not try to add continuation line */
		}
		lua_pushliteral(L, "\n");  /* add newline... */
		lua_insert(L, -2);  /* ...between the two lines */
		lua_concat(L, 3);  /* join them */
	}
}


/*
** Read a line and try to load (compile) it first as an expression (by
** adding "return " in front of it) and second as a statement. Return
** the final status of load/call with the resulting function (if any)
** in the top of the stack.
*/
int Coluster::loadline(lua_State* L) {
	int status;
	lua_settop(L, 0);
	if (!pushline(L, 1))
		return -1;  /* no input */
	if ((status = addreturn(L)) != LUA_OK)  /* 'return ...' did not work? */
		status = multiline(L);  /* try as command, maybe with continuation lines */
	lua_remove(L, 1);  /* remove line from the stack */
	lua_assert(lua_gettop(L) == 1);
	return status;
}


/*
** Prints (calling the Lua 'print' function) any values on the stack
*/
static void l_print(lua_State* L) {
	int n = lua_gettop(L);
	if (n > 0) {  /* any result to be printed? */
		luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
		lua_getglobal(L, "print");
		lua_insert(L, 1);
		if (lua_pcall(L, n, 0, 0) != LUA_OK)
			l_message("Coluster", lua_pushfstring(L, "error calling 'print' (%s)",
				lua_tostring(L, -1)));
	}
}


/*
** Do the REPL: repeatedly read (load) a line, evaluate (call) it, and
** print any results.
*/

void Coluster::doREPL(lua_State* L) {
	int status;
	lua_initreadline(L);
	while ((status = loadline(L)) != -1) {
		if (status == LUA_OK)
			status = docall(L, 0, LUA_MULTRET);
		if (status == LUA_OK) l_print(L);
		else report(L, status);
	}
	lua_settop(L, 0);  /* clear stack */
	fprintf(stdout, "\n");
	fflush(stdout);
}

bool Coluster::Poll(bool pollAsyncTasks) {
	if (!scriptWarp->join()) {
		return !pollAsyncTasks || AsyncWorker::poll(Priority_Highest);
	} else {
		return false;
	}
}

static std::vector<Coluster*> theJoiningInstances;
static std::mutex theJoiningMutex;

static decltype(signal(SIGINT, SIG_IGN)) originalSignalHandler = nullptr;
static void SignalHandler(int i) {
	std::lock_guard<std::mutex> guard(theJoiningMutex);
	fprintf(stderr, "[SYSTEM] Ctrl-C pressed. Try to terminate all colusters ...\nIf you are in linux terminal, you may need another Ctrl-D to exit the program.\n");

	for (auto&& instance : theJoiningInstances) {
		instance->Stop();
	}
}

bool Coluster::Join(LuaState lua, Ref&& finalizer, bool enableConsole) {
	if (AsyncWorker::get_current_thread_index() != mainThreadIndex) {
		fprintf(stderr, "[ERROR] Coluster::Join() must be called in main thread.\n");
		return false;
	}

	if (workerStatus.load(std::memory_order_acquire) == Status_Stopping) {
		AsyncWorker::join();
	} else {
		do {
			std::lock_guard<std::mutex> guard(theJoiningMutex);
			theJoiningInstances.emplace_back(this);
		} while (false);

		originalSignalHandler = ::signal(SIGINT, SignalHandler);

		if (enableConsole && lua_stdin_is_tty()) {
			doREPL(lua.get_state());
		} else {
			scriptWarp->Release();

			// manually polling events
			while (!AsyncWorker::is_terminated()) {
				AsyncWorker::poll_delay(Priority_Highest, 20);
			}

			scriptWarp->Acquire();
		}

		AsyncWorker::join();
		do {
			std::lock_guard<std::mutex> guard(theJoiningMutex);
			std::erase_if(theJoiningInstances, [this](auto&& p) { return p == this; });
		} while (false);

		::signal(SIGINT, originalSignalHandler);
		assert(workerStatus.load(std::memory_order_acquire) == Status_Stopping);
	}

	mainThreadIndex = ~size_t(0);
	AsyncWorker::make_current(mainThreadIndex);
	sharedWarps.clear();
	while (!scriptWarp->join()) {}

	if (finalizer) {
		lua.call<void>(std::move(finalizer));
	}

	scriptWarp->Release();
	scriptWarp->UnbindLuaRoot(lua.get_state());
	scriptWarp.reset();

	workerStatus.store(Status_Ready, std::memory_order_release);

	return true;
}

