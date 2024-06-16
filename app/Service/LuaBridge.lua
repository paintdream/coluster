local LuaBridge = {}
LuaBridge.__index = LuaBridge

function LuaBridge.New(coluster, services)
	local instance = {}
	setmetatable(instance, LuaBridge)
	local import = require("Util/Import")

	instance.object = require("luabridge").new()
	instance.types = import.FetchTypes(instance.object)
	return instance
end

function LuaBridge:Reload(coluster, services)
end

function LuaBridge:Delete(coluster, services)
end

return LuaBridge

