local LuaBridge = {}
LuaBridge.__index = LuaBridge

function LuaBridge.New(coluster, services)
	local instance = {}
	setmetatable(instance, LuaBridge)
	local import = require("util/import")

	instance.object = require("luabridge").create()
	instance.types = import.FetchTypes(instance.object)
	return instance
end

function LuaBridge:Reload(coluster, services)
end

function LuaBridge:Delete(coluster, services)
end

return LuaBridge

