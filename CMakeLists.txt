CMAKE_MINIMUM_REQUIRED (VERSION 3.1)
PROJECT (coluster)

#########

SET (STDLIB stdc++)
STRING (REPLACE "/" "\\" LOCAL_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
SET_PROPERTY (GLOBAL PROPERTY USE_FOLDERS ON)

# https://stackoverflow.com/questions/45092198/cmake-how-do-i-change-properties-on-subdirectory-project-targets
DEFINE_PROPERTY (
    TARGET
    PROPERTY FOLDER
    INHERITED
    BRIEF_DOCS "Set the folder name."
    FULL_DOCS  "Use to organize targets in an IDE."
)

FUNCTION (ADD_SUBDIRECTORY_WITH_FOLDER _folder_name _folder)
    ADD_SUBDIRECTORY (${_folder} ${ARGN})
    SET_PROPERTY (DIRECTORY "${_folder}" PROPERTY FOLDER "${_folder_name}")
ENDFUNCTION ()

SET (CMAKE_CXX_STANDARD 20)

MESSAGE (STATUS "Current platform: ${CMAKE_GENERATOR_PLATFORM}")
SET (COND_VLD WIN32 AND (${CMAKE_GENERATOR_PLATFORM} EQUAL "x64" OR ${CMAKE_GENERATOR_PLATFORM} EQUAL "Win32"))

IF (COND_VLD)
	OPTION (ENABLE_VLD "Enable Visual Leak Detector" OFF)
	IF (ENABLE_VLD)
		ADD_COMPILE_DEFINITIONS (USE_VLD)
		# cmake does not allow parentheses in environment variable, so $ENV{ProgramFiles(x86)} is prohibited
		FILE (TO_CMAKE_PATH "$ENV{ProgramFiles} (x86)" PROGRAM_FILES)
		INCLUDE_DIRECTORIES ("${PROGRAM_FILES}/Visual Leak Detector/include")

		IF (CMAKE_SIZEOF_VOID_P EQUAL 8)
			LINK_DIRECTORIES ("${PROGRAM_FILES}/Visual Leak Detector/lib/Win64")
		ELSE (CMAKE_SIZEOF_VOID_P EQUAL 8)
			LINK_DIRECTORIES ("${PROGRAM_FILES}/Visual Leak Detector/lib/Win32")
		ENDIF (CMAKE_SIZEOF_VOID_P EQUAL 8)
	ENDIF (ENABLE_VLD)
ENDIF (COND_VLD)

IF (NOT MSVC)
	ADD_DEFINITIONS(-fPIC)
ENDIF (NOT MSVC)

SET (USE_LUA_VERSION "Builtin" CACHE STRING "Lua version")
SET_PROPERTY (CACHE USE_LUA_VERSION PROPERTY STRINGS "Builtin" "LuaJIT" "5.1" "5.2" "5.3" "5.4")
SET (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
INCLUDE_DIRECTORIES ("${PROJECT_BINARY_DIR}")
INCLUDE_DIRECTORIES ("${PROJECT_SOURCE_DIR}/ref")

# BUILD COLUSTER
FILE (GLOB_RECURSE COLUSTER_SRC
	"${PROJECT_SOURCE_DIR}/src/*.h"
	"${PROJECT_SOURCE_DIR}/src/*.cpp"
	"${PROJECT_SOURCE_DIR}/ref/iris/iris_lua.h"
	"${PROJECT_SOURCE_DIR}/ref/iris/iris_dispatcher.h"
	"${PROJECT_SOURCE_DIR}/ref/iris/iris_coroutine.h"
	"${PROJECT_SOURCE_DIR}/ref/iris/iris_system.h"
	"${PROJECT_SOURCE_DIR}/ref/iris/iris_common.h"
	"${PROJECT_SOURCE_DIR}/ref/iris/iris_common.inl"
	"${PROJECT_SOURCE_DIR}/ref/glm/*.hpp"
	"${PROJECT_SOURCE_DIR}/ref/glm/*.inl"
	"${PROJECT_SOURCE_DIR}/app/*.lua"
	"${PROJECT_SOURCE_DIR}/app/*.glsl"
)

FOREACH (FILE ${COLUSTER_SRC}) 
	GET_FILENAME_COMPONENT(PARENT_DIR "${FILE}" PATH)

	# skip src or include and changes /'s to \\'s
	STRING (REGEX REPLACE "(\\./)?(src|include)/?" "" GROUP "${PARENT_DIR}")
	STRING (REPLACE "/" "\\" GROUP "${GROUP}")
	STRING (REPLACE "${LOCAL_SOURCE_DIR}" "" GROUP "${GROUP}")
	SOURCE_GROUP ("${GROUP}" FILES "${FILE}")
ENDFOREACH (FILE ${COLUSTER_SRC})

IF (${USE_LUA_VERSION} STREQUAL "Builtin")
	SET (ENABLE_LUA_BUILTIN ON)
ELSE (${USE_LUA_VERSION} STREQUAL "Builtin")
	SET (ENABLE_LUA_BUILTIN OFF)
ENDIF (${USE_LUA_VERSION} STREQUAL "Builtin")

IF (ENABLE_LUA_BUILTIN)
	MESSAGE (STATUS "Use built-in lua version.")
	# BUILD LUA
	FILE (GLOB_RECURSE LUA_EXE_SRC
		"${PROJECT_SOURCE_DIR}/ref/iris/src/lua/src/lauxlib.h"
		"${PROJECT_SOURCE_DIR}/ref/iris/src/lua/src/lua.h"
		"${PROJECT_SOURCE_DIR}/ref/iris/src/lua/src/luaconf.h"
		"${PROJECT_SOURCE_DIR}/ref/iris/src/lua/src/lualib.h"
		"${PROJECT_SOURCE_DIR}/ref/iris/src/lua/src/lua.c"
	)
	
	FILE (GLOB_RECURSE LUA_LIB_SRC
		"${PROJECT_SOURCE_DIR}/ref/iris/src/lua/src/*.h"
		"${PROJECT_SOURCE_DIR}/ref/iris/src/lua/src/*.hpp"
		"${PROJECT_SOURCE_DIR}/ref/iris/src/lua/src/*.c"
	)
	
	LIST (REMOVE_ITEM LUA_LIB_SRC "${PROJECT_SOURCE_DIR}/ref/iris/src/lua/src/luac.c")
	LIST (REMOVE_ITEM LUA_LIB_SRC "${PROJECT_SOURCE_DIR}/ref/iris/src/lua/src/lua.c")

	IF (BUILD_STANDALONE)
		ADD_LIBRARY (lua54 STATIC ${LUA_LIB_SRC})
	ELSE (BUILD_STANDALONE)
		ADD_LIBRARY (lua54 SHARED ${LUA_LIB_SRC})
		IF (MSVC)
			TARGET_COMPILE_DEFINITIONS (lua54 PRIVATE LUA_BUILD_AS_DLL)
		ELSE (MSVC)
			TARGET_COMPILE_DEFINITIONS (lua54 PRIVATE LUA_USE_LINUX)
			TARGET_LINK_LIBRARIES (lua54 m dl)
		ENDIF (MSVC)
		
		ADD_EXECUTABLE (lua ${LUA_EXE_SRC})
		TARGET_LINK_LIBRARIES (lua lua54)
	ENDIF (BUILD_STANDALONE)

	SET (LUA_CORE_LIB lua54)
	FILE (GLOB_RECURSE LUA_CORE_SRC
		"${PROJECT_SOURCE_DIR}/ref/iris/src/lua/src/lauxlib.h"
		"${PROJECT_SOURCE_DIR}/ref/iris/src/lua/src/lua.h"
		"${PROJECT_SOURCE_DIR}/ref/iris/src/lua/src/lua.hpp"
		"${PROJECT_SOURCE_DIR}/ref/iris/src/lua/src/luaconf.h"
		"${PROJECT_SOURCE_DIR}/ref/iris/src/lua/src/lualib.h"
	)

	FOREACH (FILE ${LUA_CORE_SRC}) 
		GET_FILENAME_COMPONENT(PARENT_DIR "${FILE}" PATH)
	
		# skip src or include and changes /'s to \\'s
		STRING (REGEX REPLACE "(\\./)?(src|include)/?" "" GROUP "${PARENT_DIR}")
		STRING (REPLACE "/" "\\" GROUP "${GROUP}")
		STRING (REPLACE "${LOCAL_SOURCE_DIR}" "" GROUP "${GROUP}")
		SOURCE_GROUP ("${GROUP}" FILES "${FILE}")
	ENDFOREACH (FILE ${LUA_CORE_SRC})
ELSE (ENABLE_LUA_BUILTIN)
	IF (${USE_LUA_VERSION} STREQUAL "LuaJIT")
		FIND_PACKAGE (LuaJIT)
	ELSE (${USE_LUA_VERSION} STREQUAL "LuaJIT")
		FIND_PACKAGE (Lua ${USE_LUA_VERSION} REQUIRED)
	ENDIF (${USE_LUA_VERSION} STREQUAL "LuaJIT")

	IF (LUA_FOUND OR LUAJIT_FOUND)
		MESSAGE (STATUS "Find lua version " ${USE_LUA_VERSION})
		FILE (GLOB_RECURSE LUA_CORE_SRC
			"${LUA_INCLUDE_DIR}/*.h"
			"${LUA_INCLUDE_DIR}/*.hpp"
		)

		SET (LUA_CORE_LIB ${LUA_LIBRARIES})
	ELSE (LUA_FOUND OR LUAJIT_FOUND)
		MESSAGE (FATAL_ERROR "Unable to find lua with version " ${USE_LUA_VERSION})
	ENDIF (LUA_FOUND OR LUAJIT_FOUND)
ENDIF (ENABLE_LUA_BUILTIN)

OPTION (BUILD_MONOLITHIC "Build Monolithic" OFF)
OPTION (BUILD_STANDALONE "Build Standalone (must enable Monolithic/Builtin Lua first)" OFF)

IF (BUILD_MONOLITHIC)
	SET (COLUSTER_CORE_LIBNAME coluster_static)
	LIST (REMOVE_ITEM COLUSTER_SRC "${PROJECT_SOURCE_DIR}/src/main.cpp")
	ADD_LIBRARY (${COLUSTER_CORE_LIBNAME} STATIC ${COLUSTER_SRC} ${LUA_CORE_SRC})
	TARGET_COMPILE_DEFINITIONS (${COLUSTER_CORE_LIBNAME} PUBLIC COLUSTER_MONOLITHIC)
ELSE (BUILD_MONOLITHIC)
	SET (COLUSTER_CORE_LIBNAME coluster)
	ADD_LIBRARY (${COLUSTER_CORE_LIBNAME} SHARED ${COLUSTER_SRC} ${LUA_CORE_SRC})
	TARGET_COMPILE_DEFINITIONS (${COLUSTER_CORE_LIBNAME} PUBLIC COLUSTER_IMPORT)
	TARGET_COMPILE_DEFINITIONS (${COLUSTER_CORE_LIBNAME} PRIVATE COLUSTER_EXPORT)
ENDIF (BUILD_MONOLITHIC)

IF (MSVC)
	# Default to statically-linked runtime.
	IF (BUILD_STANDALONE)
		SET (MSVC_RUNTIME "static")
	ELSE (BUILD_STANDALONE)
		SET (MSVC_RUNTIME "dynamic")
	ENDIF (BUILD_STANDALONE)
	# SET compiler options.
	SET (variables
		CMAKE_C_FLAGS_DEBUG
		CMAKE_C_FLAGS_MINSIZEREL
		CMAKE_C_FLAGS_RELEASE
		CMAKE_C_FLAGS_RELWITHDEBINFO
		CMAKE_CXX_FLAGS_DEBUG
		CMAKE_CXX_FLAGS_MINSIZEREL
		CMAKE_CXX_FLAGS_RELEASE
		CMAKE_CXX_FLAGS_RELWITHDEBINFO
	)

	ADD_DEFINITIONS (-D_HAS_EXCEPTIONS=0)
	IF (${MSVC_RUNTIME} STREQUAL "static")
		# disable RTTI & Exceptions
		SET (CMAKE_CXX_FLAGS, "/DWIN32 /D_WINDOWS /W3")
		MESSAGE (STATUS
			"MSVC -> forcing use of statically-linked runtime."
		)
		FOREACH (variable ${variables})
			IF (${variable} MATCHES "/MD")
				string(REGEX REPLACE "/MD" "/MT" ${variable} "${${variable}}")
			ENDIF ()
		ENDFOREACH ()
	ELSE ()
		MESSAGE(STATUS
			"MSVC -> forcing use of dynamically-linked runtime."
		)
		FOREACH (variable ${variables})
			IF (${variable} MATCHES "/MT")
				string(REGEX REPLACE "/MT" "/MD" ${variable} "${${variable}}")
			ENDIF ()
		ENDFOREACH ()
	ENDIF ()
ENDIF ()

IF (NOT ENABLE_LUA_BUILTIN)
	TARGET_INCLUDE_DIRECTORIES (${COLUSTER_CORE_LIBNAME} PUBLIC "${LUA_INCLUDE_DIR}")
	TARGET_COMPILE_DEFINITIONS (${COLUSTER_CORE_LIBNAME} PUBLIC USE_LUA_LIBRARY)
ENDIF (NOT ENABLE_LUA_BUILTIN)

IF (MSVC)
	TARGET_COMPILE_DEFINITIONS(${COLUSTER_CORE_LIBNAME} PRIVATE LUA_BUILD_AS_DLL)
	TARGET_LINK_LIBRARIES (${COLUSTER_CORE_LIBNAME} ${LUA_CORE_LIB})
ELSE (MSVC)
	TARGET_LINK_LIBRARIES (${COLUSTER_CORE_LIBNAME} m dl stdc++ pthread uuid ${LUA_CORE_LIB})
ENDIF (MSVC)

# Plugins

SET (COLUSTER_PLUGINS)
SET (COLUSTER_PLUGINS_INL_FILE "${PROJECT_BINARY_DIR}/plugins.inl")
SET (COLUSTER_PLUGINS_INL_DECLARE "")
SET (COLUSTER_PLUGINS_INL_REGISTER "\nstatic void ColusterRegisterPlugins(lua_State* L) {\n")

MACRO (ADD_PLUGIN PLUGIN_NAME)
	ADD_SUBDIRECTORY_WITH_FOLDER ("plugin" plugin/${PLUGIN_NAME})
	SET (COLUSTER_PLUGINS ${COLUSTER_PLUGINS} ${PLUGIN_NAME})
	IF (BUILD_MONOLITHIC)
		STRING (APPEND COLUSTER_PLUGINS_INL_DECLARE "extern \"C\" int luaopen_${PLUGIN_NAME}(lua_State* L)\;\n")
		STRING (APPEND COLUSTER_PLUGINS_INL_REGISTER "\tluaL_requiref(L, \"${PLUGIN_NAME}\", luaopen_${PLUGIN_NAME}, 1)\;\tlua_pop(L, 1)\;\n")
	ENDIF (BUILD_MONOLITHIC)
ENDMACRO ()

OPTION (ENABLE_UTIL "Enable Util" ON)
IF (ENABLE_UTIL)
	ADD_PLUGIN (util)
ENDIF (ENABLE_UTIL)

OPTION (ENABLE_SPACE "Enable Space" ON)
IF (ENABLE_SPACE)
	ADD_PLUGIN (space)
ENDIF (ENABLE_SPACE)

OPTION (ENABLE_TRACE "Enable Trace" ON)
IF (ENABLE_TRACE)
	ADD_PLUGIN (trace)
ENDIF (ENABLE_TRACE)

OPTION (ENABLE_STORAGE "Enable Storage" ON)
IF (ENABLE_STORAGE)
	ADD_PLUGIN (storage)
ENDIF (ENABLE_STORAGE)

OPTION (ENABLE_LUABRIDGE "Enable Multi-LuaVM" ON)
IF (ENABLE_LUABRIDGE)
	ADD_PLUGIN (luabridge)
ENDIF (ENABLE_LUABRIDGE)

OPTION (ENABLE_CHANNEL "Enable Channel" ON)
IF (ENABLE_CHANNEL)
	ADD_PLUGIN (channel)
ENDIF (ENABLE_CHANNEL)

OPTION (ENABLE_DATABASE "Enable Database" ON)
IF (ENABLE_DATABASE)
	ADD_PLUGIN (database)
ENDIF (ENABLE_DATABASE)

OPTION (ENABLE_DEVICE "Enable Vulkan Device" ON)
IF (ENABLE_DEVICE)
	IF (NOT ENABLE_STORAGE)
		MESSAGE (FATAL_ERROR "Vulkan Device requires Storage enabled")
	ENDIF (NOT ENABLE_STORAGE)

	ADD_PLUGIN (device)
ENDIF (ENABLE_DEVICE)

OPTION (ENABLE_GRAPHIC "Enable Vulkan Graphic" ON)
IF (ENABLE_GRAPHIC)
	ADD_PLUGIN (graphic)
ENDIF (ENABLE_GRAPHIC)

OPTION (ENABLE_PYBRIDGE "Enable PyBridge" OFF)
IF (ENABLE_PYBRIDGE)
	ADD_PLUGIN (pybridge)
ENDIF (ENABLE_PYBRIDGE)

STRING (APPEND COLUSTER_PLUGINS_INL_REGISTER "}\n")
FILE (WRITE ${COLUSTER_PLUGINS_INL_FILE} ${COLUSTER_PLUGINS_INL_DECLARE})
FILE (APPEND ${COLUSTER_PLUGINS_INL_FILE} ${COLUSTER_PLUGINS_INL_REGISTER})

IF (BUILD_MONOLITHIC)
	ADD_LIBRARY (coluster SHARED "${PROJECT_SOURCE_DIR}/src/main.cpp" ${COLUSTER_PLUGINS_INL_FILE})
	TARGET_LINK_LIBRARIES (coluster ${COLUSTER_CORE_LIBNAME} ${COLUSTER_PLUGINS})
	TARGET_COMPILE_DEFINITIONS (coluster PUBLIC COLUSTER_IMPORT)
	TARGET_COMPILE_DEFINITIONS (coluster PRIVATE COLUSTER_EXPORT)
ENDIF (BUILD_MONOLITHIC)
